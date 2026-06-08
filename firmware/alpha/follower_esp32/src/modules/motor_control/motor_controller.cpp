#include "motor_controller.h"
#include "modules/drivers/tmc2209/tmc2209_driver.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <esp_rom_sys.h>

namespace {
constexpr float kDegreesPerStep =
    360.0f / (TMC2209_FULL_STEPS_PER_REV * TMC2209_MICROSTEPS);
constexpr int8_t kHomingDirectionSign[NUM_STEPPERS] = {
    -1, // Base
    -1, // Shoulder A
    1,  // Shoulder B (mirrored)
    -1  // Elbow
};

inline float signf_nonzero(float v) { return (v >= 0.0f) ? 1.0f : -1.0f; }
} // namespace

MotorController &MotorController::instance() {
  static MotorController inst;
  return inst;
}

MotorController::MotorController() {
  current_positions_.fill(0.0f);
  current_velocities_.fill(0.0f);
  target_positions_.fill(0.0f);
  position_errors_.fill(0.0f);
  current_steps_.fill(0);
  target_steps_.fill(0);
  zero_offsets_steps_.fill(0);
  homing_latched_zero_steps_.fill(0);
  planner_velocity_steps_s_.fill(0.0f);
  planner_accel_steps_s2_.fill(0.0f);
}

void MotorController::stepTimerCallback(void *arg) {
  auto *self = static_cast<MotorController *>(arg);
  if (!self) {
    return;
  }

  if (!self->enabled_ || self->homing_phase_ != HomingPhase::IDLE) {
    return;
  }

  int32_t emit_counts[NUM_STEPPERS] = {0};
  bool emit_direction_positive[NUM_STEPPERS] = {false};

  portENTER_CRITICAL(&self->motion_lock_);
  for (size_t motor_index = 0; motor_index < NUM_STEPPERS; ++motor_index) {
    const float rate_steps_s = self->commanded_step_rate_steps_s_[motor_index];
    const float abs_rate = std::abs(rate_steps_s);

    if (abs_rate < 0.001f) {
      continue;
    }

    float phase = self->step_phase_accumulator_[motor_index];
    phase += abs_rate * (static_cast<float>(STEP_TIMER_PERIOD_US) / 1000000.0f);

    int32_t emit = static_cast<int32_t>(phase);
    if (emit > 0) {
      phase -= static_cast<float>(emit);
      emit_counts[motor_index] = emit;
      emit_direction_positive[motor_index] = rate_steps_s > 0.0f;
      self->step_position_isr_[motor_index] +=
          emit_direction_positive[motor_index] ? emit : -emit;
    }

    self->step_phase_accumulator_[motor_index] = phase;
  }
  portEXIT_CRITICAL(&self->motion_lock_);

  for (size_t motor_index = 0; motor_index < NUM_STEPPERS; ++motor_index) {
    const int32_t emit = emit_counts[motor_index];
    if (emit <= 0) {
      continue;
    }

    digitalWrite(TMC2209_DIR_PINS[motor_index],
                 emit_direction_positive[motor_index] ? HIGH : LOW);
    for (int32_t step = 0; step < emit; ++step) {
      digitalWrite(TMC2209_STEP_PINS[motor_index], HIGH);
      esp_rom_delay_us(STEP_PULSE_HIGH_US);
      digitalWrite(TMC2209_STEP_PINS[motor_index], LOW);
      esp_rom_delay_us(STEP_PULSE_LOW_US);
    }
  }
}

void MotorController::initStepTimer() {
  if (step_timer_) {
    return;
  }

  esp_timer_create_args_t timer_args = {};
  timer_args.callback = &MotorController::stepTimerCallback;
  timer_args.arg = this;
  timer_args.dispatch_method = ESP_TIMER_ISR;
  timer_args.name = "step_gen";

  if (esp_timer_create(&timer_args, &step_timer_) != ESP_OK) {
    Serial.println("MotorController: failed to create step timer");
    step_timer_ = nullptr;
    return;
  }

  if (esp_timer_start_periodic(step_timer_, STEP_TIMER_PERIOD_US) != ESP_OK) {
    Serial.println("MotorController: failed to start step timer");
    esp_timer_delete(step_timer_);
    step_timer_ = nullptr;
    return;
  }

  Serial.print("MotorController: step timer started @ ");
  Serial.print(STEP_TIMER_PERIOD_US);
  Serial.println(" us");
}

void MotorController::setCommandedStepRate(uint8_t motor_index,
                                           float steps_per_s) {
  const float max_step_rate = MAX_MOTOR_SPEED_DEG_PER_S / kDegreesPerStep;
  const float clamped =
      std::max(-max_step_rate, std::min(max_step_rate, steps_per_s));

  portENTER_CRITICAL(&motion_lock_);
  commanded_step_rate_steps_s_[motor_index] = clamped;
  portEXIT_CRITICAL(&motion_lock_);
}

void MotorController::setAllCommandedRatesToZero() {
  portENTER_CRITICAL(&motion_lock_);
  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    commanded_step_rate_steps_s_[i] = 0.0f;
    step_phase_accumulator_[i] = 0.0f;
  }
  portEXIT_CRITICAL(&motion_lock_);
}

void MotorController::syncIsrStepPositionFromCurrent() {
  portENTER_CRITICAL(&motion_lock_);
  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    step_position_isr_[i] = current_steps_[i];
  }
  portEXIT_CRITICAL(&motion_lock_);
}

void MotorController::init() {
  setupStepPins();
  initStepTimer();
  syncIsrStepPositionFromCurrent();

  Serial.println("MotorController: init timer-driven planner + step generator "
                 "(open-loop)");
  Serial.print("  - Deg/step: ");
  Serial.println(kDegreesPerStep, 6);
  Serial.print("  - Max speed (deg/s): ");
  Serial.println(MAX_MOTOR_SPEED_DEG_PER_S, 1);
  Serial.print("  - Max accel (deg/s^2): ");
  Serial.println(MAX_MOTOR_ACCEL_DEG_PER_S2, 1);
  Serial.print("  - Max jerk (deg/s^3): ");
  Serial.println(MAX_MOTOR_JERK_DEG_PER_S3, 1);
}

void MotorController::enforceShoulderAntiPhaseLock() {
  constexpr size_t SHOULDER_A = 1;
  constexpr size_t SHOULDER_B = 2;

  const int32_t shoulder_a_relative_steps =
      target_steps_[SHOULDER_A] - zero_offsets_steps_[SHOULDER_A];
  target_steps_[SHOULDER_B] =
      zero_offsets_steps_[SHOULDER_B] - shoulder_a_relative_steps;
  target_positions_[SHOULDER_B] = stepsToDegrees(
      target_steps_[SHOULDER_B] - zero_offsets_steps_[SHOULDER_B]);
}

void MotorController::setAbsoluteTargets(
    const std::array<float, NUM_STEPPERS> &targets) {
  if (homing_phase_ != HomingPhase::IDLE &&
      homing_phase_ != HomingPhase::COMPLETE) {
    return;
  }

  for (int i = 0; i < NUM_STEPPERS; ++i) {
    target_positions_[i] = targets[i];
    target_steps_[i] =
        zero_offsets_steps_[i] + degreesToSteps(target_positions_[i]);
  }

  enforceShoulderAntiPhaseLock();

  for (int i = 0; i < NUM_STEPPERS; ++i) {
    position_errors_[i] = target_positions_[i] - current_positions_[i];
  }
}

float MotorController::stepsToDegrees(int32_t steps) {
  return static_cast<float>(steps) * kDegreesPerStep;
}

int32_t MotorController::degreesToSteps(float degrees) {
  const float steps_f = degrees / kDegreesPerStep;
  return static_cast<int32_t>(lroundf(steps_f));
}

void MotorController::setupStepPins() {
  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    pinMode(TMC2209_STEP_PINS[i], OUTPUT);
    pinMode(TMC2209_DIR_PINS[i], OUTPUT);
    digitalWrite(TMC2209_STEP_PINS[i], LOW);
    digitalWrite(TMC2209_DIR_PINS[i], LOW);
  }
}

void MotorController::pulseStepPin(uint8_t motor_index) {
  digitalWrite(TMC2209_STEP_PINS[motor_index], HIGH);
  delayMicroseconds(STEP_PULSE_HIGH_US);
  digitalWrite(TMC2209_STEP_PINS[motor_index], LOW);
  delayMicroseconds(STEP_PULSE_LOW_US);
}

bool MotorController::isMotorSelectedForHoming(uint8_t motor_index) const {
  return ((homing_mask_ >> motor_index) & 0x01) != 0;
}

int32_t MotorController::maxStepsForSpeed(float speed_deg_per_s,
                                          float dt) const {
  const float max_steps_f = (speed_deg_per_s / kDegreesPerStep) * dt;
  return std::max<int32_t>(1, static_cast<int32_t>(floorf(max_steps_f)));
}

int32_t MotorController::doSingleMotorStep(uint8_t motor_index,
                                           bool direction_positive,
                                           int32_t max_steps_this_cycle) {
  if (max_steps_this_cycle <= 0)
    return 0;

  digitalWrite(TMC2209_DIR_PINS[motor_index], direction_positive ? HIGH : LOW);
  for (int32_t step = 0; step < max_steps_this_cycle; ++step) {
    pulseStepPin(motor_index);
  }

  const int32_t signed_move =
      direction_positive ? max_steps_this_cycle : -max_steps_this_cycle;
  current_steps_[motor_index] += signed_move;
  syncIsrStepPositionFromCurrent();
  return signed_move;
}

bool MotorController::advanceToNextHomingMotor() {
  for (uint8_t index = homing_motor_index_; index < NUM_STEPPERS; ++index) {
    if (!isMotorSelectedForHoming(index)) {
      continue;
    }

    homing_motor_index_ = index;
    homing_seek_steps_done_ = 0;
    homing_phase_steps_done_ = 0;
    homing_stall_debounce_count_ = 0;
    homing_phase_ = HomingPhase::SEEK_STALL;

    TMC2209Driver::instance().enableStallGuard(index, homing_sensitivity_);
    Serial.print("MotorController: homing motor ");
    Serial.println(index);
    return true;
  }

  completeHomingSuccess();
  return false;
}

void MotorController::completeHomingSuccess() {
  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    if (isMotorSelectedForHoming(static_cast<uint8_t>(i))) {
      TMC2209Driver::instance().disableStallGuard(static_cast<uint8_t>(i));
      zero_offsets_steps_[i] = homing_latched_zero_steps_[i];
      target_steps_[i] = current_steps_[i];
      target_positions_[i] = 0.0f;
    }
  }

  homing_phase_ = HomingPhase::COMPLETE;
  last_homing_success_ = true;
  Serial.println("MotorController: StallGuard homing completed");
}

void MotorController::failHoming(const char *reason) {
  if (homing_motor_index_ < NUM_STEPPERS) {
    TMC2209Driver::instance().disableStallGuard(homing_motor_index_);
  }
  homing_phase_ = HomingPhase::FAILED;
  last_homing_success_ = false;
  Serial.print("MotorController: StallGuard homing failed: ");
  Serial.println(reason);
}

void MotorController::runHomingStep(float dt) {
  if (homing_phase_ == HomingPhase::COMPLETE ||
      homing_phase_ == HomingPhase::FAILED) {
    homing_phase_ = HomingPhase::IDLE;
    return;
  }

  if (homing_motor_index_ >= NUM_STEPPERS) {
    failHoming("invalid motor index");
    return;
  }

  const int8_t sign = kHomingDirectionSign[homing_motor_index_];
  const bool seek_positive = sign > 0;

  if (homing_phase_ == HomingPhase::SEEK_STALL) {
    const int32_t steps = maxStepsForSpeed(HOMING_SEEK_SPEED_DEG_PER_S, dt);
    homing_seek_steps_done_ +=
        std::abs(doSingleMotorStep(homing_motor_index_, seek_positive, steps));

    if (TMC2209Driver::instance().isStalled(homing_motor_index_)) {
      homing_stall_debounce_count_++;
    } else {
      homing_stall_debounce_count_ = 0;
    }

    if (homing_stall_debounce_count_ >= HOMING_STALL_DEBOUNCE_CYCLES) {
      homing_phase_ = HomingPhase::BACKOFF;
      homing_phase_steps_done_ = 0;
      homing_stall_debounce_count_ = 0;
      Serial.print("MotorController: stall detected, backoff motor ");
      Serial.println(homing_motor_index_);
      return;
    }

    if (homing_seek_steps_done_ >= HOMING_SEEK_MAX_STEPS) {
      failHoming("seek max steps exceeded");
      return;
    }
  }

  if (homing_phase_ == HomingPhase::BACKOFF) {
    const int32_t steps = maxStepsForSpeed(HOMING_SEEK_SPEED_DEG_PER_S, dt);
    const int32_t remaining = HOMING_BACKOFF_STEPS - homing_phase_steps_done_;
    const int32_t to_move = std::min(steps, remaining);
    homing_phase_steps_done_ += std::abs(
        doSingleMotorStep(homing_motor_index_, !seek_positive, to_move));

    if (homing_phase_steps_done_ >= HOMING_BACKOFF_STEPS) {
      homing_phase_ = HomingPhase::APPROACH;
      homing_phase_steps_done_ = 0;
      Serial.print("MotorController: re-approach motor ");
      Serial.println(homing_motor_index_);
      return;
    }
  }

  if (homing_phase_ == HomingPhase::APPROACH) {
    const int32_t steps = maxStepsForSpeed(HOMING_APPROACH_SPEED_DEG_PER_S, dt);
    homing_phase_steps_done_ +=
        std::abs(doSingleMotorStep(homing_motor_index_, seek_positive, steps));

    if (TMC2209Driver::instance().isStalled(homing_motor_index_)) {
      homing_stall_debounce_count_++;
    } else {
      homing_stall_debounce_count_ = 0;
    }

    if (homing_stall_debounce_count_ >= HOMING_STALL_DEBOUNCE_CYCLES) {
      homing_latched_zero_steps_[homing_motor_index_] =
          current_steps_[homing_motor_index_];
      TMC2209Driver::instance().disableStallGuard(homing_motor_index_);

      homing_motor_index_++;
      advanceToNextHomingMotor();
      return;
    }

    if (homing_phase_steps_done_ >= HOMING_APPROACH_MAX_STEPS) {
      failHoming("approach max steps exceeded");
      return;
    }
  }
}

void MotorController::updateVelocitiesFromStepDelta(
    const std::array<int32_t, NUM_STEPPERS> &step_delta, float dt) {
  if (dt <= 0.0f) {
    current_velocities_.fill(0.0f);
    return;
  }
  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    current_velocities_[i] = stepsToDegrees(step_delta[i]) / dt;
  }
}

void MotorController::updatePlannerTargets(float dt) {
  const float max_step_rate = MAX_MOTOR_SPEED_DEG_PER_S / kDegreesPerStep;
  const float max_step_accel = MAX_MOTOR_ACCEL_DEG_PER_S2 / kDegreesPerStep;
  const float max_step_jerk = MAX_MOTOR_JERK_DEG_PER_S3 / kDegreesPerStep;

  int32_t max_remaining_steps = 0;
  std::array<int32_t, NUM_STEPPERS> remaining_steps{};
  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    remaining_steps[i] = target_steps_[i] - current_steps_[i];
    max_remaining_steps =
        std::max(max_remaining_steps, std::abs(remaining_steps[i]));
  }

  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    const int32_t rem = remaining_steps[i];
    const int32_t abs_rem = std::abs(rem);

    float desired_vel = 0.0f;
    if (abs_rem > 1 && max_remaining_steps > 0) {
      const float sync_scale =
          static_cast<float>(abs_rem) / static_cast<float>(max_remaining_steps);
      const float sync_target = max_step_rate * sync_scale;
      const float brake_limit =
          sqrtf(2.0f * max_step_accel * static_cast<float>(abs_rem));
      const float limited = std::min(sync_target, brake_limit);
      desired_vel = signf_nonzero(static_cast<float>(rem)) * limited;
    }

    float accel_target =
        (desired_vel - planner_velocity_steps_s_[i]) / std::max(0.0005f, dt);
    accel_target =
        std::max(-max_step_accel, std::min(max_step_accel, accel_target));

    const float max_d_accel = max_step_jerk * dt;
    const float accel_error = accel_target - planner_accel_steps_s2_[i];
    planner_accel_steps_s2_[i] +=
        std::max(-max_d_accel, std::min(max_d_accel, accel_error));

    planner_velocity_steps_s_[i] += planner_accel_steps_s2_[i] * dt;

    if (desired_vel == 0.0f && std::abs(planner_velocity_steps_s_[i]) < 0.5f) {
      planner_velocity_steps_s_[i] = 0.0f;
      planner_accel_steps_s2_[i] = 0.0f;
    }

    if (desired_vel != 0.0f) {
      if ((planner_velocity_steps_s_[i] * desired_vel) < 0.0f) {
        planner_velocity_steps_s_[i] = 0.0f;
      }
      if (std::abs(planner_velocity_steps_s_[i]) > std::abs(desired_vel)) {
        planner_velocity_steps_s_[i] = desired_vel;
      }
    }

    setCommandedStepRate(static_cast<uint8_t>(i), planner_velocity_steps_s_[i]);
  }
}

void MotorController::update() {
  static unsigned long last_update_us = 0;
  const unsigned long now_us = micros();
  float dt = 1.0f / static_cast<float>(CONTROL_LOOP_HZ);

  if (last_update_us != 0) {
    dt = static_cast<float>(now_us - last_update_us) / 1000000.0f;
    if (dt < 0.0005f)
      dt = 0.0005f;
    if (dt > 0.05f)
      dt = 1.0f / static_cast<float>(CONTROL_LOOP_HZ);
  }
  last_update_us = now_us;

  std::array<int32_t, NUM_STEPPERS> isr_positions{};
  portENTER_CRITICAL(&motion_lock_);
  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    isr_positions[i] = step_position_isr_[i];
  }
  portEXIT_CRITICAL(&motion_lock_);

  std::array<int32_t, NUM_STEPPERS> step_delta{};
  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    step_delta[i] = isr_positions[i] - current_steps_[i];
    current_steps_[i] = isr_positions[i];
  }

  if (homing_phase_ != HomingPhase::IDLE) {
    setAllCommandedRatesToZero();
    runHomingStep(dt);
    current_velocities_.fill(0.0f);
    for (size_t i = 0; i < NUM_STEPPERS; ++i) {
      current_positions_[i] =
          stepsToDegrees(current_steps_[i] - zero_offsets_steps_[i]);
      target_positions_[i] =
          stepsToDegrees(target_steps_[i] - zero_offsets_steps_[i]);
      position_errors_[i] = target_positions_[i] - current_positions_[i];
    }
    return;
  }

  if (!enabled_) {
    setAllCommandedRatesToZero();
    planner_velocity_steps_s_.fill(0.0f);
    planner_accel_steps_s2_.fill(0.0f);
    current_velocities_.fill(0.0f);

    for (size_t i = 0; i < NUM_STEPPERS; ++i) {
      current_positions_[i] =
          stepsToDegrees(current_steps_[i] - zero_offsets_steps_[i]);
      target_positions_[i] =
          stepsToDegrees(target_steps_[i] - zero_offsets_steps_[i]);
      position_errors_[i] = target_positions_[i] - current_positions_[i];
    }
    return;
  }

  enforceShoulderAntiPhaseLock();
  updatePlannerTargets(dt);
  updateVelocitiesFromStepDelta(step_delta, dt);

  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    current_positions_[i] =
        stepsToDegrees(current_steps_[i] - zero_offsets_steps_[i]);
    target_positions_[i] =
        stepsToDegrees(target_steps_[i] - zero_offsets_steps_[i]);
    position_errors_[i] = target_positions_[i] - current_positions_[i];
  }
}

void MotorController::enableMotors() {
  enabled_ = true;
  Serial.println("MotorController: motors enabled");
}

void MotorController::disableMotors() {
  enabled_ = false;
  setAllCommandedRatesToZero();
  planner_velocity_steps_s_.fill(0.0f);
  planner_accel_steps_s2_.fill(0.0f);
  current_velocities_.fill(0.0f);
  Serial.println("MotorController: motors disabled");
}

bool MotorController::motorsEnabled() const { return enabled_; }

bool MotorController::startStallGuardHoming(uint8_t motor_mask,
                                            uint8_t sensitivity) {
  if (isHoming()) {
    Serial.println("MotorController: homing already active");
    return false;
  }

  if (motor_mask == 0) {
    Serial.println("MotorController: homing rejected (empty motor mask)");
    return false;
  }

  if (!enabled_) {
    enableMotors();
  }

  setAllCommandedRatesToZero();
  planner_velocity_steps_s_.fill(0.0f);
  planner_accel_steps_s2_.fill(0.0f);

  homing_mask_ = static_cast<uint8_t>(motor_mask & 0x0F);
  homing_sensitivity_ = sensitivity;
  homing_motor_index_ = 0;
  homing_seek_steps_done_ = 0;
  homing_phase_steps_done_ = 0;
  homing_stall_debounce_count_ = 0;
  last_homing_success_ = false;

  Serial.print("MotorController: starting StallGuard homing, mask=0x");
  Serial.print(homing_mask_, HEX);
  Serial.print(" sensitivity=");
  Serial.println(homing_sensitivity_);

  return advanceToNextHomingMotor();
}

bool MotorController::isHoming() const {
  return homing_phase_ != HomingPhase::IDLE;
}

bool MotorController::lastHomingSucceeded() const {
  return last_homing_success_;
}

void MotorController::setCurrentPositionAsZero() {
  for (size_t i = 0; i < NUM_STEPPERS; ++i) {
    zero_offsets_steps_[i] = current_steps_[i];
    target_steps_[i] = current_steps_[i];
    current_positions_[i] = 0.0f;
    target_positions_[i] = 0.0f;
    position_errors_[i] = 0.0f;
  }

  syncIsrStepPositionFromCurrent();
  setAllCommandedRatesToZero();

  Serial.println(
      "MotorController: software zero reference set to current position");
}

std::array<float, NUM_STEPPERS> MotorController::getCurrentPositions() const {
  return current_positions_;
}

std::array<float, NUM_STEPPERS> MotorController::getCurrentVelocities() const {
  return current_velocities_;
}

std::array<float, NUM_STEPPERS> MotorController::getErrors() const {
  return position_errors_;
}
