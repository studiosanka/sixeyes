// Single-stepper position control and homing for one v1 joint node.
//
// Unlike legacy motor_controller.cpp (4 steppers, no position feedback),
// this drives exactly one TMC2209. Position is OPEN LOOP in steps (counted by
// StepGenerator) with a smooth trapezoid move; the AS5600 encoder is a check,
// not a control input: if it stops following the steps the node latches a
// stall and stops (docs/reference/CAN_MESSAGE_PROTOCOL.md §4.3a).
//
// Until the node is homed it ignores targets: the step count means nothing
// without a reference. Homing finds that reference by driving slowly into a
// hard stop and noticing the stall (see startHoming()).
//
// Not bench-tested. Every number that matters here is a placeholder in
// config/joint_config.h or drivers/tmc2209/tmc2209_config.h.

#pragma once
#include <cstdint>

class MotorController {
public:
  static MotorController &instance();

  void init();

  // Call every loop (about CONTROL_LOOP_HZ); it measures its own elapsed time.
  void update(uint32_t now_ms);

  // From MOTOR_TARGET CAN frames (see can_protocol.h). Degrees at the OUTPUT
  // shaft; converted to steps with the 1:25 ratio (joint_config.h). Ignored
  // until the node is homed, and clamped to the joint limits.
  void setTargetPosition(float degrees);

  // Stop and hold where the shaft is (decelerates, then returns to this spot).
  void holdHere();

  // === Homing ===
  // Sensorless homing: StallGuard (UART, polled: DIAG is not wired on v1.2) and
  // an encoder check (the AS5600 stops following the steps). Either one ends the
  // search. Returns false, with the reason logged, if it cannot start: the
  // encoder is faulted, or the safety task will not allow the motor to be enabled.
  bool startHoming();
  // Stop, forget the reference and go back to unhomed. For host `stop` during homing.
  void abortHoming();
  bool isHoming() const;
  bool isHomed() const { return homed_; }
  bool homingFailed() const { return homing_failed_; }
  // Kept for the old API name.
  bool isHomingComplete() const { return homed_; }

  // Clears the stall and homing-failed latches (MOTOR_FLAG_RESET_FAULT).
  void resetFault();

  // Encoder-vs-steps divergence, reported in NODE_STATUS. Latched until resetFault().
  bool isStallDetected() const;

  bool isMoving() const;

#ifdef BENCH_CONSOLE
  // Bench only (v1_bench environment): treat the shaft's present pose as 0 degrees
  // without homing, so a lone motor on the bench can be moved and measured.
  // Never compiled into the arm's firmware.
  void benchZeroHere();
  const char *benchModeName() const;
  float benchVelocitySps() const { return velocity_sps_; }
#endif

  // Joint angle in degrees at the output shaft, from the step count. Only
  // meaningful once homed.
  float currentOutputDeg() const;

  // Drives the TMC2209 EN pin. disable() is what the E-stop path calls.
  void enable();
  void disable();
  bool isEnabled() const;

private:
  MotorController() = default;

  enum class Mode : uint8_t { Idle, Position, HomingSeek, HomingBackoff };

  void runProfile(float dt, float max_speed_sps, float accel_sps2);
  void seekStep(uint32_t now_ms, float dt);
  void onStopFound(const char *method);
  void finishHoming();
  void failHoming(const char *reason);
  void endHomingDriver();
  void checkDivergence();
  void applyRate();
  void settleTrim(uint32_t now_ms);

  Mode mode_ = Mode::Idle;
  bool enabled_ = false;
  bool homed_ = false;
  bool homing_failed_ = false;
  bool stall_detected_ = false;

  float velocity_sps_ = 0.0f; // commanded step rate, signed
  int32_t target_steps_ = 0;
  uint32_t last_update_us_ = 0;

  // Encoder-vs-steps check (re-anchored whenever the shaft is still).
  int32_t anchor_steps_ = 0;
  int32_t anchor_raw_ = 0;

  // Settle trim (see joint_config.h TRIM_*).
  bool settling_ = false;
  uint32_t settle_start_ms_ = 0;
  int trim_attempts_ = 0;

  // Homing state.
  uint32_t homing_start_ms_ = 0;
  uint32_t seek_start_ms_ = 0;
  uint32_t last_sg_poll_ms_ = 0;
  int sg_hits_ = 0;
  uint32_t window_start_ms_ = 0;
  int32_t window_start_steps_ = 0;
  int32_t window_start_raw_ = 0;
  int enc_hits_ = 0;
};
