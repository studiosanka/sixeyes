#pragma once
#include "esp_timer.h"
#include "modules/config/board_config.h"
#include "modules/drivers/tmc2209/tmc2209_config.h"
#include <array>
#include <cmath>
#include <cstdint>

enum class HomingPhase : uint8_t {
  IDLE = 0,
  SEEK_STALL,
  BACKOFF,
  APPROACH,
  COMPLETE,
  FAILED
};

class MotorController {
public:
  static MotorController &instance();
  void init();
  void setAbsoluteTargets(const std::array<float, NUM_STEPPERS> &targets);
  void update(); // called at CONTROL_LOOP_HZ
  void enableMotors();
  void disableMotors();
  bool motorsEnabled() const;
  void setCurrentPositionAsZero();
  bool startStallGuardHoming(uint8_t motor_mask = 0x0F,
                             uint8_t sensitivity = TMC2209_SGTHRS_DEFAULT);
  bool isHoming() const;
  bool lastHomingSucceeded() const;

  // Accessors for diagnostics
  std::array<float, NUM_STEPPERS> getCurrentPositions() const;
  std::array<float, NUM_STEPPERS> getCurrentVelocities() const;
  std::array<float, NUM_STEPPERS> getErrors() const;

private:
  MotorController();

  // Open-loop motion constants (motor shaft degrees).
  static constexpr float MAX_MOTOR_SPEED_DEG_PER_S = 720.0f;
  static constexpr float MAX_MOTOR_ACCEL_DEG_PER_S2 = 2400.0f;
  static constexpr float MAX_MOTOR_JERK_DEG_PER_S3 = 30000.0f;
  static constexpr float POSITION_TOLERANCE_DEG = 0.05f;
  static constexpr uint32_t STEP_PULSE_HIGH_US = 2;
  static constexpr uint32_t STEP_PULSE_LOW_US = 2;
  static constexpr uint32_t STEP_TIMER_PERIOD_US = 100;
  static constexpr float HOMING_SEEK_SPEED_DEG_PER_S = 180.0f;
  static constexpr float HOMING_APPROACH_SPEED_DEG_PER_S = 60.0f;
  static constexpr int32_t HOMING_SEEK_MAX_STEPS = 30000;
  static constexpr int32_t HOMING_BACKOFF_STEPS = 800;
  static constexpr int32_t HOMING_APPROACH_MAX_STEPS = 3000;
  static constexpr uint8_t HOMING_STALL_DEBOUNCE_CYCLES = 3;

  // State tracking
  std::array<float, NUM_STEPPERS> current_positions_{};  // motor-shaft deg
  std::array<float, NUM_STEPPERS> current_velocities_{}; // motor-shaft deg/s
  std::array<float, NUM_STEPPERS> target_positions_{};   // motor-shaft deg
  std::array<float, NUM_STEPPERS> position_errors_{};    // target-current deg
  std::array<int32_t, NUM_STEPPERS> current_steps_{};
  std::array<int32_t, NUM_STEPPERS> target_steps_{};
  std::array<int32_t, NUM_STEPPERS> zero_offsets_steps_{};
  std::array<int32_t, NUM_STEPPERS> homing_latched_zero_steps_{};
  std::array<float, NUM_STEPPERS> planner_velocity_steps_s_{};
  std::array<float, NUM_STEPPERS> planner_accel_steps_s2_{};

  portMUX_TYPE motion_lock_ = portMUX_INITIALIZER_UNLOCKED;
  volatile float commanded_step_rate_steps_s_[NUM_STEPPERS] = {0};
  volatile float step_phase_accumulator_[NUM_STEPPERS] = {0};
  volatile int32_t step_position_isr_[NUM_STEPPERS] = {0};
  esp_timer_handle_t step_timer_ = nullptr;

  bool is_interpolating_ = false;
  bool enabled_ = false;
  HomingPhase homing_phase_ = HomingPhase::IDLE;
  uint8_t homing_mask_ = 0;
  uint8_t homing_sensitivity_ = TMC2209_SGTHRS_DEFAULT;
  uint8_t homing_motor_index_ = 0;
  int32_t homing_seek_steps_done_ = 0;
  int32_t homing_phase_steps_done_ = 0;
  uint8_t homing_stall_debounce_count_ = 0;
  bool last_homing_success_ = false;

  // Helper methods
  static float stepsToDegrees(int32_t steps);
  static int32_t degreesToSteps(float degrees);
  static void stepTimerCallback(void *arg);
  void initStepTimer();
  void setCommandedStepRate(uint8_t motor_index, float steps_per_s);
  void setAllCommandedRatesToZero();
  void syncIsrStepPositionFromCurrent();
  void updatePlannerTargets(float dt);
  void enforceShoulderAntiPhaseLock();
  void pulseStepPin(uint8_t motor_index);
  void setupStepPins();
  void updateVelocitiesFromStepDelta(
      const std::array<int32_t, NUM_STEPPERS> &step_delta, float dt);
  bool isMotorSelectedForHoming(uint8_t motor_index) const;
  bool advanceToNextHomingMotor();
  void completeHomingSuccess();
  void failHoming(const char *reason);
  int32_t maxStepsForSpeed(float speed_deg_per_s, float dt) const;
  int32_t doSingleMotorStep(uint8_t motor_index, bool direction_positive,
                            int32_t max_steps_this_cycle);
  void runHomingStep(float dt);
};
