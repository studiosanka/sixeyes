#include "node_telemetry.h"
#include "modules/comms/can/can_driver.h"
#include "modules/comms/can/can_protocol.h"
#include "modules/config/board_config.h"
#include "modules/config/node_config.h"
#include "modules/drivers/encoder/encoder_driver.h"
#include "modules/motor_control/motor_controller.h"

// A missing or faulted encoder (a bare dev board, a loose module) would cost
// up to the I2C timeout on every loop. Retry it only this often instead.
static constexpr uint32_t ENCODER_RETRY_MS = 100;

// ENCODER_TELEMETRY at 50 Hz and NODE_STATUS at 10 Hz (the rate in the CAN
// protocol doc). Both are well inside NODE_LIVENESS_TIMEOUT_MS, and four nodes
// sending them use under 5% of the 1 Mbps bus.
static constexpr uint32_t TELEMETRY_PERIOD_MS = 20;
static constexpr uint32_t STATUS_PERIOD_MS = 100;

NodeTelemetry &NodeTelemetry::instance() {
  static NodeTelemetry inst;
  return inst;
}

void NodeTelemetry::update(uint32_t now_ms) {
  if (fault_ && tried_ && static_cast<uint32_t>(now_ms - last_try_ms_) < ENCODER_RETRY_MS) return;
  tried_ = true;
  last_try_ms_ = now_ms;
  raw_ = EncoderDriver::instance().readPosition();
  fault_ = EncoderDriver::instance().hasFault();
}

void NodeTelemetry::publish(uint32_t now_ms) {
#if JOINT_NODE_IS_BASE
  (void)now_ms;
#else
  const uint32_t telemetry_elapsed = static_cast<uint32_t>(now_ms - last_telemetry_ms_);
  if (telemetry_elapsed >= TELEMETRY_PERIOD_MS) {
    // Speed in counts per control-loop tick, as the protocol defines it.
    const int32_t delta = counts() - last_telemetry_counts_;
    int32_t per_tick = static_cast<int32_t>(
        (static_cast<int64_t>(delta) * 1000) / (static_cast<int64_t>(telemetry_elapsed) * CONTROL_LOOP_HZ));
    if (per_tick > INT16_MAX) per_tick = INT16_MAX;
    if (per_tick < INT16_MIN) per_tick = INT16_MIN;

    EncoderTelemetryFrame frame = {};
    frame.position = counts();
    frame.velocity_estimate = static_cast<int16_t>(per_tick);
    frame.seq = telemetry_seq_++;
    CanDriver::instance().send(canIdFor(CAN_ID_ENCODER_TELEM, JOINT_NODE_ID), frame);
    last_telemetry_ms_ = now_ms;
    last_telemetry_counts_ = counts();
  }

  if (static_cast<uint32_t>(now_ms - last_status_ms_) >= STATUS_PERIOD_MS) {
    NodeStatusFrame frame = {};
    uint8_t bits = fault_ ? NODE_FAULT_ENCODER : 0;
    if (MotorController::instance().homingFailed()) bits |= NODE_FAULT_HOMING_FAILED;
    if (MotorController::instance().isHomed()) bits |= NODE_STATE_HOMED;
    if (MotorController::instance().isHoming()) bits |= NODE_STATE_HOMING;
    frame.fault_bitmask = bits;
    frame.motors_enabled = MotorController::instance().isEnabled() ? 1 : 0;
    frame.stall_detected = MotorController::instance().isStallDetected() ? 1 : 0;
    frame.encoder_position = counts();
    CanDriver::instance().send(canIdFor(CAN_ID_NODE_STATUS, JOINT_NODE_ID), frame);
    last_status_ms_ = now_ms;
  }
#endif
}
