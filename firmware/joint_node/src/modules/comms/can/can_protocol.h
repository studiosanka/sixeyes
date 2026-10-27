// CAN message IDs and payload layouts for the v1 joint bus.
// Mirrors docs/reference/CAN_MESSAGE_PROTOCOL.md exactly — that document is
// the source of truth; keep this header in sync with it, not the other way
// around.

#pragma once
#include <cstdint>

// --- CAN ID allocation (11-bit standard ID), §3 ---------------------------

static constexpr uint32_t CAN_ID_ESTOP          = 0x000; // bus-wide, any sender
static constexpr uint32_t CAN_ID_BUS_HEARTBEAT  = 0x010; // Base -> all
static constexpr uint32_t CAN_ID_NODE_STATUS    = 0x020; // + node_id, node -> Base
static constexpr uint32_t CAN_ID_MOTOR_TARGET   = 0x100; // + node_id, Base -> node
static constexpr uint32_t CAN_ID_SERVO_TARGET   = 0x110; // + node_id (Elbow only), Base -> node
static constexpr uint32_t CAN_ID_ENCODER_TELEM  = 0x200; // + node_id, node -> Base
static constexpr uint32_t CAN_ID_FAULT_REPORT   = 0x300; // + node_id, node -> Base

inline uint32_t canIdFor(uint32_t base, uint8_t node_id) { return base + node_id; }

// --- Payload structs, §4 ---------------------------------------------------
// All multi-byte fields little-endian; ESP32-C6 is little-endian natively so
// plain struct layout matches on-wire layout for these packed structs.
// Every struct here must be <= 8 bytes (CAN 2.0A data payload limit).

#pragma pack(push, 1)

enum class EstopReason : uint8_t {
  LAPTOP_ROS2_REQUEST = 0,
  HEARTBEAT_TIMEOUT   = 1,
  NODE_DETECTED_FAULT = 2,
  MANUAL_BUTTON       = 3,
};

struct EstopFrame {
  uint8_t reason; // EstopReason
}; // 1 byte

struct BusHeartbeatFrame {
  uint32_t sequence;
  uint8_t  ros2_alive; // 0/1
}; // 5 bytes

// NODE_STATUS fault_bitmask bits. Bits 0-1 are faults; bits 4-5 report state
// (the protocol doc calls the byte fault_bitmask; homing state rides in it).
static constexpr uint8_t NODE_FAULT_ENCODER       = 1 << 0; // AS5600 not answering, or no magnet seen
static constexpr uint8_t NODE_FAULT_HOMING_FAILED = 1 << 1; // homing stopped without finding the stop
static constexpr uint8_t NODE_STATE_HOMED         = 1 << 4; // the node has a reference and follows targets
static constexpr uint8_t NODE_STATE_HOMING        = 1 << 5; // homing is running

struct NodeStatusFrame {
  uint8_t  fault_bitmask;
  uint8_t  motors_enabled;   // 0/1
  uint8_t  stall_detected;   // 0/1 — encoder-vs-command divergence, see §4.3a
  int32_t  encoder_position; // raw AS5600 encoder counts
}; // 7 bytes

struct MotorTargetFrame {
  float    target_position; // degrees at the OUTPUT shaft (the node multiplies by the 1:25 ratio)
  uint16_t seq;
  uint8_t  flags; // MOTOR_FLAG_* below
}; // 7 bytes

// MOTOR_TARGET flags. A frame with HOME or ABORT set ignores target_position.
static constexpr uint8_t MOTOR_FLAG_HOME        = 1 << 0; // start homing
static constexpr uint8_t MOTOR_FLAG_RESET_FAULT = 1 << 1; // clear stall and homing-failed latches
static constexpr uint8_t MOTOR_FLAG_ABORT       = 1 << 2; // stop where you are; abort homing (added 2026-10-27)

struct ServoTargetFrame {
  uint8_t  servo_index; // 0-2
  float    target_angle; // degrees
  uint16_t seq;
}; // 7 bytes

struct EncoderTelemetryFrame {
  int32_t  position;          // raw AS5600 encoder counts
  int16_t  velocity_estimate; // counts/tick
  uint16_t seq;
}; // 8 bytes

struct FaultReportFrame {
  uint8_t  fault_code;
  uint32_t timestamp_ms; // node-local millis
}; // 5 bytes

#pragma pack(pop)
