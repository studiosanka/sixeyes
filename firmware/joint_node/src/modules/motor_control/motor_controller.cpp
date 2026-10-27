#include "motor_controller.h"
#include "modules/config/joint_config.h"
#include "modules/drivers/tmc2209/tmc2209_driver.h"
#include "modules/motor_control/step_generator.h"
#include "modules/safety/can_safety_task.h"
#include "modules/telemetry/node_telemetry.h"
#include "modules/util/logging.h"
#include <Arduino.h>
#include <cmath>

namespace {

// --- unit conversions: output-shaft degrees <-> steps <-> encoder counts ------

inline float stepsPerMotorRev() {
  return TMC2209_FULL_STEPS_PER_REV * static_cast<float>(TMC2209Driver::instance().getMicrosteps());
}

// Positive output degrees -> positive steps, except on Shoulder R (mirrored).
inline float outputDegToSteps(float deg) {
  return deg * JOINT_GEAR_RATIO * JOINT_DIRECTION_SIGN * stepsPerMotorRev() / 360.0f;
}
inline float stepsToOutputDeg(float steps) {
  return steps * 360.0f / (stepsPerMotorRev() * JOINT_GEAR_RATIO * JOINT_DIRECTION_SIGN);
}
inline float stepsPerSecFromOutputDegPerSec(float deg_per_s) {
  return fabsf(outputDegToSteps(deg_per_s));
}

// Encoder counts the AS5600 should move per step (signed by ENCODER_SIGN_VS_STEPS).
inline float countsPerStep() {
  return static_cast<float>(ENCODER_COUNTS_PER_MOTOR_REV) / stepsPerMotorRev() * ENCODER_SIGN_VS_STEPS;
}

// Steps for a given output-shaft direction (+1 or -1) towards the homing stop.
inline int homingStepSign() { return HOMING_DIRECTION_OUTPUT * static_cast<int>(JOINT_DIRECTION_SIGN); }

// --- tuning constants -----------------------------------------------------------

constexpr uint32_t SG_POLL_MS = 20;      // StallGuard read interval (UART, about 1 ms each)
constexpr int SG_HITS_NEEDED = 3;        // consecutive low readings (60 ms)
constexpr uint32_t ENC_WINDOW_MS = 100;  // encoder stall window
constexpr int ENC_HITS_NEEDED = 2;       // consecutive windows
constexpr float ENC_MIN_EXPECTED_COUNTS = 40.0f; // a window must have moved this much to judge
constexpr float ENC_FOLLOW_RATIO_MIN = 0.25f;    // below this fraction of the expected motion = stalled

// Encoder-vs-steps divergence that counts as a lost-step stall (see the header).
constexpr float STALL_DIVERGENCE_THRESHOLD_DEG = 5.0f; // output shaft; 125 deg at the motor. TODO bench-tune.

} // namespace

MotorController &MotorController::instance() {
  static MotorController inst;
  return inst;
}

void MotorController::init() {
  TMC2209Driver::instance().init();
  TMC2209Driver::instance().configureMotor(0);
  StepGenerator::instance().init();
  // Motors start disabled (TMC2209Driver::init() holds EN high). Only
  // CanSafetyTask::requestEnable() enables them, and only for homing.
  enabled_ = false;
  last_update_us_ = micros();
}

// ---------------------------------------------------------------- the loop

void MotorController::update(uint32_t now_ms) {
  const uint32_t now_us = micros();
  float dt = static_cast<float>(static_cast<uint32_t>(now_us - last_update_us_)) * 1e-6f;
  last_update_us_ = now_us;
  if (dt > 0.05f) dt = 0.05f; // a long stall elsewhere must not become one huge velocity step
  if (dt < 0.0005f) dt = 0.0005f;

  if (!enabled_) {
    velocity_sps_ = 0.0f;
    applyRate();
    return;
  }

  switch (mode_) {
    case Mode::Idle:
      velocity_sps_ = 0.0f;
      break;
    case Mode::Position:
      runProfile(dt, stepsPerSecFromOutputDegPerSec(MOTION_MAX_SPEED_OUTPUT_DEG_S),
                 stepsPerSecFromOutputDegPerSec(MOTION_MAX_ACCEL_OUTPUT_DEG_S2));
      break;
    case Mode::HomingSeek:
      seekStep(now_ms, dt);
      break;
    case Mode::HomingBackoff:
      runProfile(dt, stepsPerSecFromOutputDegPerSec(HOMING_SPEED_OUTPUT_DEG_S),
                 stepsPerSecFromOutputDegPerSec(HOMING_ACCEL_OUTPUT_DEG_S2));
      if (fabsf(velocity_sps_) < 1.0f &&
          fabsf(static_cast<float>(target_steps_ - StepGenerator::instance().position())) < 2.0f) {
        finishHoming();
      }
      break;
  }

  applyRate();
  checkDivergence();
  settleTrim(now_ms);
}

// Position control against the encoder: once the shaft has come to rest at its
// target, check the AS5600. If the shaft is short of where the step count says it
// is (lost steps, or a load that held the rotor back), pull the step count back to
// where the shaft really is and let the normal move redo the missing steps.
void MotorController::settleTrim(uint32_t now_ms) {
  const bool at_rest = mode_ == Mode::Position && homed_ && !stall_detected_ &&
                       fabsf(velocity_sps_) < 1.0f &&
                       labs(target_steps_ - StepGenerator::instance().position()) <= 1;
  if (!at_rest || NodeTelemetry::instance().encoderFault()) {
    settling_ = false;
    return;
  }
  if (!settling_) {
    settling_ = true;
    settle_start_ms_ = now_ms;
    trim_attempts_ = 0;
    return;
  }
  if (static_cast<uint32_t>(now_ms - settle_start_ms_) < TRIM_SETTLE_MS || trim_attempts_ >= TRIM_MAX_ATTEMPTS) return;

  // Where the AS5600 should read, if the shaft is exactly where the step count says.
  const int32_t pos = StepGenerator::instance().position();
  const float expected_rad = stepsToOutputDeg(static_cast<float>(pos)) * DEG_TO_RAD_F;
  const int32_t err_counts = outputRadToMotorCounts(expected_rad, JOINT_NODE_ID) - NodeTelemetry::instance().counts();
  settle_start_ms_ = now_ms; // look again after another rest period
  if (labs(err_counts) <= TRIM_DEADBAND_COUNTS || labs(err_counts) > TRIM_MAX_COUNTS) return;

  const int32_t err_steps = static_cast<int32_t>(lroundf(static_cast<float>(err_counts) / countsPerStep()));
  StepGenerator::instance().setPosition(pos - err_steps); // the shaft is really this far back
  trim_attempts_++;
  Logging::infof("MotorController: trimming %ld steps (encoder was %ld counts off)",
                 static_cast<long>(err_steps), static_cast<long>(err_counts));
}

void MotorController::applyRate() { StepGenerator::instance().setRate(velocity_sps_); }

// Trapezoid move to target_steps_: speed up at most `accel`, never faster than
// `max_speed`, and slow down so that it arrives at the target at rest.
void MotorController::runProfile(float dt, float max_speed_sps, float accel_sps2) {
  const float err = static_cast<float>(target_steps_ - StepGenerator::instance().position());
  const float dir = err > 0.0f ? 1.0f : (err < 0.0f ? -1.0f : 0.0f);
  const float stop_speed = sqrtf(2.0f * accel_sps2 * fabsf(err)); // fastest speed we could still stop from
  const float v_des = dir * fminf(max_speed_sps, stop_speed);

  const float dv = accel_sps2 * dt;
  if (v_des > velocity_sps_ + dv) {
    velocity_sps_ += dv;
  } else if (v_des < velocity_sps_ - dv) {
    velocity_sps_ -= dv;
  } else {
    velocity_sps_ = v_des;
  }
  if (fabsf(err) < 1.0f && fabsf(velocity_sps_) < StepGenerator::MIN_RATE_SPS * 2.0f) velocity_sps_ = 0.0f;
}

// ---------------------------------------------------------------- targets

void MotorController::setTargetPosition(float degrees) {
  if (!homed_) return;       // without a reference the step count means nothing
  if (stall_detected_) return; // a latched stall needs resetFault() first
  const int idx = jointIndexForNode(JOINT_NODE_ID);
  const float low = JOINT_LIMIT_LOW[idx] * RAD_TO_DEG_F;
  const float high = JOINT_LIMIT_HIGH[idx] * RAD_TO_DEG_F;
  if (degrees < low) degrees = low;
  if (degrees > high) degrees = high;
  target_steps_ = static_cast<int32_t>(lroundf(outputDegToSteps(degrees)));
  if (mode_ == Mode::Idle) mode_ = Mode::Position;
}

void MotorController::holdHere() {
  if (mode_ == Mode::HomingSeek || mode_ == Mode::HomingBackoff) {
    abortHoming();
    return;
  }
  target_steps_ = StepGenerator::instance().position();
}

float MotorController::currentOutputDeg() const {
  return stepsToOutputDeg(static_cast<float>(StepGenerator::instance().position()));
}

bool MotorController::isMoving() const {
  return fabsf(velocity_sps_) > 0.5f ||
         labs(target_steps_ - StepGenerator::instance().position()) > 2;
}

// ---------------------------------------------------------------- stall check

void MotorController::checkDivergence() {
  if (mode_ == Mode::HomingSeek) return; // a stall is exactly what homing is waiting for
  if (NodeTelemetry::instance().encoderFault()) return;

  const int32_t steps = StepGenerator::instance().position();
  const int32_t raw = NodeTelemetry::instance().rawCounts();
  if (fabsf(velocity_sps_) < 1.0f) { // still: re-anchor, so only drift during a move counts
    anchor_steps_ = steps;
    anchor_raw_ = raw;
    return;
  }
  const float expected = static_cast<float>(anchor_raw_) +
                         static_cast<float>(steps - anchor_steps_) * countsPerStep();
  // 5 degrees at the output = 5 * 25 degrees at the motor = that many counts.
  const float limit = STALL_DIVERGENCE_THRESHOLD_DEG * JOINT_GEAR_RATIO *
                      static_cast<float>(ENCODER_COUNTS_PER_MOTOR_REV) / 360.0f;
  if (fabsf(static_cast<float>(raw) - expected) > limit) {
    stall_detected_ = true;
    velocity_sps_ = 0.0f;
    applyRate();
    target_steps_ = steps; // stay where we are until the host clears the fault
    Logging::error("MotorController: encoder is not following the steps (stall), motion stopped");
  }
}

bool MotorController::isStallDetected() const { return stall_detected_; }

void MotorController::resetFault() {
  stall_detected_ = false;
  homing_failed_ = false;
}

// ---------------------------------------------------------------- homing

bool MotorController::isHoming() const {
  return mode_ == Mode::HomingSeek || mode_ == Mode::HomingBackoff;
}

bool MotorController::startHoming() {
  if (isHoming()) return true;
  homed_ = false;
  homing_failed_ = false;
  stall_detected_ = false;
  NodeTelemetry::instance().clearCountsOffset();

  // The reference is stored as an encoder offset, so homing needs a working encoder.
  if (NodeTelemetry::instance().encoderFault()) {
    failHoming("the encoder is not working, homing needs the AS5600");
    return false;
  }
  if (!CanSafetyTask::instance().requestEnable()) {
    failHoming("the motor may not be enabled now (no BUS_HEARTBEAT, or a bus fault is latched)");
    return false;
  }

  TMC2209Driver &drv = TMC2209Driver::instance();
  if (drv.uartAvailable()) {
    drv.setCurrent(0, TMC2209_HOMING_RUN_MA);
    drv.enableStallGuard(0, TMC2209_SGTHRS_HOMING);
  } else {
    Logging::warn("MotorController: no UART to the driver, homing will use the encoder check only");
  }

  const uint32_t now = millis();
  homing_start_ms_ = now;
  seek_start_ms_ = now;
  last_sg_poll_ms_ = now;
  window_start_ms_ = now;
  window_start_steps_ = StepGenerator::instance().position();
  window_start_raw_ = NodeTelemetry::instance().rawCounts();
  sg_hits_ = 0;
  enc_hits_ = 0;
  velocity_sps_ = 0.0f;
  mode_ = Mode::HomingSeek;
  Logging::info("MotorController: homing started");
  return true;
}

void MotorController::seekStep(uint32_t now_ms, float dt) {
  if (static_cast<uint32_t>(now_ms - homing_start_ms_) > HOMING_TIMEOUT_MS) {
    failHoming("timed out without finding the stop");
    return;
  }
  if (NodeTelemetry::instance().encoderFault()) {
    failHoming("the encoder stopped answering while homing");
    return;
  }

  // Ramp up to the slow homing speed, towards the stop.
  const float target_rate = static_cast<float>(homingStepSign()) *
                            stepsPerSecFromOutputDegPerSec(HOMING_SPEED_OUTPUT_DEG_S);
  const float dv = stepsPerSecFromOutputDegPerSec(HOMING_ACCEL_OUTPUT_DEG_S2) * dt;
  if (target_rate > velocity_sps_ + dv) {
    velocity_sps_ += dv;
  } else if (target_rate < velocity_sps_ - dv) {
    velocity_sps_ -= dv;
  } else {
    velocity_sps_ = target_rate;
  }

  // Ignore stall signals while still accelerating: StallGuard and the encoder
  // check both read nonsense at low speed.
  if (static_cast<uint32_t>(now_ms - seek_start_ms_) < HOMING_BLANKING_MS) {
    window_start_ms_ = now_ms;
    window_start_steps_ = StepGenerator::instance().position();
    window_start_raw_ = NodeTelemetry::instance().rawCounts();
    sg_hits_ = 0;
    enc_hits_ = 0;
    return;
  }

  // 1. StallGuard, polled over UART (DIAG is not wired on the v1.2 board).
  if (TMC2209Driver::instance().uartAvailable() &&
      static_cast<uint32_t>(now_ms - last_sg_poll_ms_) >= SG_POLL_MS) {
    last_sg_poll_ms_ = now_ms;
    uint16_t sg = 0;
    if (TMC2209Driver::instance().readSGResult(0, sg)) { // a failed read counts as "no reading", not a stall
      sg_hits_ = TMC2209Driver::instance().isStalled(0) ? sg_hits_ + 1 : 0;
#ifdef BENCH_CONSOLE
      Logging::infof("home: SG_RESULT=%u (stall at <=%u) hits=%d", sg, 2u * TMC2209_SGTHRS_HOMING, sg_hits_);
#endif
    }
    if (sg_hits_ >= SG_HITS_NEEDED) {
      onStopFound("StallGuard");
      return;
    }
  }

  // 2. Encoder: the shaft must keep following the steps.
  if (static_cast<uint32_t>(now_ms - window_start_ms_) >= ENC_WINDOW_MS) {
    const int32_t steps = StepGenerator::instance().position();
    const int32_t raw = NodeTelemetry::instance().rawCounts();
    const float expected = static_cast<float>(steps - window_start_steps_) * countsPerStep();
    const float actual = static_cast<float>(raw - window_start_raw_);
    window_start_ms_ = now_ms;
    window_start_steps_ = steps;
    window_start_raw_ = raw;
    if (fabsf(expected) >= ENC_MIN_EXPECTED_COUNTS) {
      const float follow = actual / expected; // 1 = follows, 0 = not moving, negative = wrong way
      if (follow < -ENC_FOLLOW_RATIO_MIN) {
        failHoming("the encoder counts the opposite way to the steps: flip ENCODER_SIGN_VS_STEPS in joint_config.h");
        return;
      }
      enc_hits_ = (follow < ENC_FOLLOW_RATIO_MIN) ? enc_hits_ + 1 : 0;
      if (enc_hits_ >= ENC_HITS_NEEDED) {
        onStopFound("encoder");
        return;
      }
    }
  }
}

// The stop is found: take it as the reference, then back off.
void MotorController::onStopFound(const char *method) {
  velocity_sps_ = 0.0f;
  applyRate();

  // The stop sits at HOMING_STOP_ANGLE_RAD. Make the step count and the encoder
  // counts both say so, so that from here on they describe the true joint angle.
  const float stop_deg = HOMING_STOP_ANGLE_RAD * RAD_TO_DEG_F;
  const int32_t stop_steps = static_cast<int32_t>(lroundf(outputDegToSteps(stop_deg)));
  const int32_t stop_counts = outputRadToMotorCounts(HOMING_STOP_ANGLE_RAD, JOINT_NODE_ID);
  StepGenerator::instance().setPosition(stop_steps);
  NodeTelemetry::instance().setCountsOffset(stop_counts - NodeTelemetry::instance().rawCounts());

  const int32_t backoff = static_cast<int32_t>(
      lroundf(fabsf(outputDegToSteps(HOMING_BACKOFF_OUTPUT_DEG))));
  target_steps_ = stop_steps - homingStepSign() * backoff;
  mode_ = Mode::HomingBackoff;
  Logging::infof("MotorController: stop found by %s, backing off", method);
}

void MotorController::finishHoming() {
  endHomingDriver();
  homed_ = true;
  homing_failed_ = false;
  stall_detected_ = false;
  mode_ = Mode::Position;
  target_steps_ = StepGenerator::instance().position();
  anchor_steps_ = target_steps_;
  anchor_raw_ = NodeTelemetry::instance().rawCounts();
  Logging::info("MotorController: homed");
}

void MotorController::failHoming(const char *reason) {
  velocity_sps_ = 0.0f;
  applyRate();
  homed_ = false;
  homing_failed_ = true;
  mode_ = Mode::Idle;
  endHomingDriver();
  Logging::errorf("MotorController: homing failed: %s", reason);
  disable(); // do not leave the motor energised after a failure
}

void MotorController::abortHoming() {
  velocity_sps_ = 0.0f;
  applyRate();
  homed_ = false;
  homing_failed_ = false;
  mode_ = Mode::Idle;
  endHomingDriver();
  target_steps_ = StepGenerator::instance().position();
  NodeTelemetry::instance().clearCountsOffset();
}

// Put the driver back to its normal run current with StallGuard off.
void MotorController::endHomingDriver() {
  TMC2209Driver &drv = TMC2209Driver::instance();
  if (!drv.uartAvailable()) return;
  drv.disableStallGuard(0);
  drv.setCurrent(0, TMC2209_DEFAULT_RUN_MA);
}

// ---------------------------------------------------------------- enable

void MotorController::enable() {
  TMC2209Driver::instance().setEnabled(0, true);
  enabled_ = true;
}

void MotorController::disable() {
  // Cut driver power stage first, then clear the flags. Safe from the E-stop
  // path: GPIO writes and a timer stop, no UART traffic.
  TMC2209Driver::instance().setEnabled(0, false);
  StepGenerator::instance().setRate(0.0f);
  velocity_sps_ = 0.0f;
  if (isHoming()) homing_failed_ = true; // interrupted before it finished
  // With the motor unpowered the shaft can move, so the reference is gone:
  // after any disable the joint must be homed again.
  homed_ = false;
  mode_ = Mode::Idle;
  enabled_ = false;
  CanSafetyTask::instance().noteMotorsDisabled();
}

bool MotorController::isEnabled() const { return enabled_; }

#ifdef BENCH_CONSOLE
void MotorController::benchZeroHere() {
  StepGenerator::instance().setPosition(0);
  NodeTelemetry::instance().setCountsOffset(-NodeTelemetry::instance().rawCounts());
  target_steps_ = 0;
  anchor_steps_ = 0;
  anchor_raw_ = NodeTelemetry::instance().rawCounts();
  homed_ = true; // only so that targets are accepted; the bench pose is not a real reference
  homing_failed_ = false;
  stall_detected_ = false;
  mode_ = Mode::Position;
}

const char *MotorController::benchModeName() const {
  switch (mode_) {
    case Mode::Idle: return "idle";
    case Mode::Position: return "position";
    case Mode::HomingSeek: return "homing-seek";
    case Mode::HomingBackoff: return "homing-backoff";
  }
  return "?";
}
#endif
