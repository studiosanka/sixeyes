// Single-stepper position control for one v1 joint node.
// Unlike legacy motor_controller.cpp (4 steppers, no position feedback),
// this drives exactly one TMC2209 and closes the loop against the SPI
// encoder for stall/fault detection only -- it does not do closed-loop PID
// position control (encoder is a divergence check, not a control input;
// see docs/protocols/CAN_MESSAGE_PROTOCOL.md §4.3a for why).

#pragma once
#include <cstdint>

class MotorController {
public:
  static MotorController &instance();

  void init();

  // Call at CONTROL_LOOP_HZ.
  void update();

  // From MOTOR_TARGET CAN frames (see can_protocol.h).
  void setTargetPosition(float degrees);

  // StallGuard-based sensorless homing -- homing only, per §4.3a. Not used
  // for runtime stall detection.
  void startHoming();
  bool isHomingComplete() const;

  // Encoder-vs-commanded-position divergence, reported in NODE_STATUS.
  // Threshold is a bench-tuning constant, not fixed at the protocol level.
  bool isStallDetected() const;

  void disable();
  bool isEnabled() const;

private:
  MotorController() = default;

  float target_position_deg_ = 0.0f;
  bool enabled_ = false;
  bool homing_in_progress_ = false;
  bool homing_complete_ = false;
  bool stall_detected_ = false;

  // TODO: bench-tune. Starting placeholder only.
  static constexpr float STALL_DIVERGENCE_THRESHOLD_DEG = 5.0f;
};
