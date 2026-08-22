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
  // Motors start disabled -- CanSafetyTask::emergencyDisable() / boot-state
  // rule in can_safety_task.cpp governs when this becomes true.
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
  if (!EncoderDriver::instance().hasFault()) {
    int32_t raw = EncoderDriver::instance().readPosition();
    // TODO: raw encoder counts -> degrees conversion depends on final
    // encoder part (MT6835 vs AS5048A) and gear ratio -- not implemented,
    // see encoder_driver.cpp. Divergence check below is a placeholder using
    // raw counts directly, which is almost certainly the wrong scale.
    float encoder_deg_placeholder = static_cast<float>(raw);
    float divergence = fabsf(encoder_deg_placeholder - target_position_deg_);
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

void MotorController::disable() {
  enabled_ = false;
  // TODO: assert TMC2209 EN pin once TMC2209_EN_PINS is wired to a real
  // GPIO -- see tmc2209_config.h placeholder note.
}

bool MotorController::isEnabled() const { return enabled_; }
