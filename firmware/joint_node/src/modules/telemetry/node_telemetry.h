// What this node measures about itself, read once per loop and shared.
// The AS5600 is read here (not inside MotorController) so the position is
// available even while the motor is disabled: the host's state stream and the
// Base board's view of the other nodes both need it before anything moves.
//
// Non-Base nodes also publish it on the CAN bus (docs/reference/
// CAN_MESSAGE_PROTOCOL.md): ENCODER_TELEMETRY at 50 Hz (position and speed,
// what the host's state stream needs) and NODE_STATUS at 10 Hz (faults, stall
// flag, motors enabled; also the liveness signal Base watches for).
// The protocol allows ENCODER_TELEMETRY at the control-loop rate; 50 Hz is
// enough for the host's 50 to 100 Hz state stream and keeps the bus quiet.

#pragma once
#include <cstdint>

class NodeTelemetry {
public:
  static NodeTelemetry &instance();

  // Call every loop().
  void update(uint32_t now_ms);

  // Non-Base nodes: send ENCODER_TELEMETRY and NODE_STATUS when due. Does
  // nothing on Base, whose own values are read directly.
  void publish(uint32_t now_ms);

  // Motor-shaft counts. Until the node is homed they are relative to the first
  // good read after boot; homing sets an offset so that afterwards the counts
  // describe the true joint angle (counts() = raw + offset).
  int32_t counts() const { return raw_ + offset_; }
  int32_t rawCounts() const { return raw_; }
  void setCountsOffset(int32_t offset) { offset_ = offset; }
  void clearCountsOffset() { offset_ = 0; }
  bool encoderFault() const { return fault_; }

private:
  NodeTelemetry() = default;

  int32_t raw_ = 0;
  int32_t offset_ = 0;
  bool fault_ = true;
  bool tried_ = false;
  uint32_t last_try_ms_ = 0;
  uint32_t last_telemetry_ms_ = 0;
  uint32_t last_status_ms_ = 0;
  int32_t last_telemetry_counts_ = 0;
  uint16_t telemetry_seq_ = 0;
};
