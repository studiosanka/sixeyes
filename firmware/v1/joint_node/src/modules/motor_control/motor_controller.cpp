#include "motor_controller.h"
#include "modules/drivers/encoder/encoder_driver.h"
#include "modules/drivers/tmc2209/tmc2209_driver.h"
#include "modules/util/logging.h"
#include <cmath>

MotorController &MotorController::instance() {
  static MotorController inst;
  return inst;
}

void MotorController::init() {
  TMC2209Driver::instance().init();
  TMC2209Driver::instance().configureMotor(0);
  // Motors start disabled (TMC2209Driver::init() holds EN high) --
  // CanSafetyTask / boot-state rule in can_safety_task.cpp governs when
  // enable() is called.
  enabled_ = false;
}

void MotorController::update() {
  if (!enabled_) return;

  // TODO: actual step generation. Legacy used a per-tick step-rate scheduler
  // (motor_control_scheduler.cpp) across 4 motors; v1 has one motor per node
  // so this can likely be simpler (e.g. driven by a hardware timer/RMT
  // channel rather than software stepping in the control loop), but that's
  // an implementation choice for whoever picks this up -- not decided yet.

  // Stall/fault check: encoder-vs-commanded divergence (§4.3a). Not
  // StallGuard -- StallGuard is homing-only, see startHoming() below.
  // Always read so the encoder can recover from a transient I2C fault;
  // hasFault() reports whether this reading can be trusted.
  int32_t counts = EncoderDriver::instance().readPosition();
  if (!EncoderDriver::instance().hasFault()) {
    // TODO: counts -> joint degrees still needs the mechanical decision of
    // where the magnet sits (motor shaft vs. 1:25 gearbox output) and, if on
    // the motor shaft, the 1:25 ratio. countsToDegrees() gives degrees of the
    // magnet's own shaft only, so this divergence is only correct for an
    // output-shaft magnet until that is settled. Not bench-tuned either.
    float encoder_deg = EncoderDriver::countsToDegrees(counts);
    float divergence = fabsf(encoder_deg - target_position_deg_);
    stall_detected_ = divergence > STALL_DIVERGENCE_THRESHOLD_DEG;
  }
}

void MotorController::setTargetPosition(float degrees) {
  target_position_deg_ = degrees;
}

void MotorController::startHoming() {
  homing_in_progress_ = true;
  homing_complete_ = false;
  TMC2209Driver::instance().enableStallGuard(0);
  // TODO: homing sequence (drive toward hard stop, poll isStalled(), zero
  // position on stall detect, disableStallGuard()). Not implemented --
  // legacy's motor_controller.cpp startStallGuardHoming()/runHomingStep()
  // is the closest reference, adapted for one motor instead of four.
  Logging::warn("MotorController: startHoming() is a stub, not implemented");
}

bool MotorController::isHomingComplete() const { return homing_complete_; }

bool MotorController::isStallDetected() const { return stall_detected_; }

void MotorController::enable() {
  TMC2209Driver::instance().setEnabled(0, true);
  enabled_ = true;
}

void MotorController::disable() {
  // Cut driver power stage first, then clear the flag. Safe from the E-stop
  // path: this is a single GPIO write, no UART traffic.
  TMC2209Driver::instance().setEnabled(0, false);
  enabled_ = false;
}

bool MotorController::isEnabled() const { return enabled_; }
