#pragma once

#include "modules/config/board_config.h"
#include "modules/drivers/tmc2209/tmc2209_config.h"
#include <array>
#include <cstdint>
#include <cstring>

/**
 * @file uart_json_messages.h
 * @brief JSON message type definitions for UART extensibility
 *
 * Message Format:
 *   {"cmd":"TYPE","seq":N,... other_fields ...}\n
 *
 * All messages are delimited by newline and can include optional fields.
 * This design allows backward-compatible feature expansion.
 */

// Forward declarations
struct CommandMessage;
struct ConfigMessage;
struct DiagnosticRequest;
struct TelemetryQuery;
struct FaultResponse;

/**
 * @enum MessageType
 * Extensible message type enumeration
 */
enum class MessageType : uint8_t {
  // Command messages (ROS2 → ESP32)
  MOTOR_TARGET = 1,    // Set motor position targets
  SERVO_TARGET = 2,    // Set servo PWM values
  ENABLE_MOTORS = 3,   // Enable/disable motor outputs
  RESET_FAULT = 4,     // Clear fault state
  TUNE_PID = 5,        // Update PID gains
  HOME_ZERO = 6,       // Set current step-counter position as software zero
  HOME_STALLGUARD = 7, // Run StallGuard-based homing sequence
  JOINT_STATE = 8,     // Teleoperation: leader joint positions → follower

  // Configuration messages
  CONFIG_PARAM = 10, // Set config parameter
  CONFIG_SAVE = 11,  // Save to EEPROM
  CONFIG_RESET = 12, // Reset to defaults

  // Query/Diagnostic messages (ROS2 ← ESP32)
  MOTOR_STATUS = 20,  // Current motor state
  SERVO_STATUS = 21,  // Current servo state
  SYSTEM_STATUS = 22, // Overall system health
  FAULT_DETAILS = 23, // Detailed fault information
  STATISTICS = 24,    // Runtime statistics

  // Telemetry control
  TELEMETRY_ENABLE = 30, // Enable/disable telemetry stream
  TELEMETRY_RATE = 31,   // Set telemetry rate (Hz)

  // Heartbeat (can be JSON or ASCII)
  HEARTBEAT = 40,        // ROS2 heartbeat (alt JSON version)
  STATUS_HEARTBEAT = 41, // ESP32 status heartbeat (alt JSON version)

  // Unknown / Invalid
  UNKNOWN = 255
};

/**
 * @brief Parse message type from command string
 * @param cmd Command string (e.g., "MOTOR_TARGET")
 * @return Corresponding MessageType enum
 */
inline MessageType cmdStringToType(const char *cmd) {
  if (strcmp(cmd, "MOTOR_TARGET") == 0)
    return MessageType::MOTOR_TARGET;
  if (strcmp(cmd, "SERVO_TARGET") == 0)
    return MessageType::SERVO_TARGET;
  if (strcmp(cmd, "ENABLE_MOTORS") == 0)
    return MessageType::ENABLE_MOTORS;
  if (strcmp(cmd, "RESET_FAULT") == 0)
    return MessageType::RESET_FAULT;
  if (strcmp(cmd, "TUNE_PID") == 0)
    return MessageType::TUNE_PID;
  if (strcmp(cmd, "HOME_ZERO") == 0)
    return MessageType::HOME_ZERO;
  if (strcmp(cmd, "HOME_STALLGUARD") == 0)
    return MessageType::HOME_STALLGUARD;
  if (strcmp(cmd, "JOINT_STATE") == 0)
    return MessageType::JOINT_STATE;
  if (strcmp(cmd, "CONFIG_PARAM") == 0)
    return MessageType::CONFIG_PARAM;
  if (strcmp(cmd, "CONFIG_SAVE") == 0)
    return MessageType::CONFIG_SAVE;
  if (strcmp(cmd, "CONFIG_RESET") == 0)
    return MessageType::CONFIG_RESET;
  if (strcmp(cmd, "MOTOR_STATUS") == 0)
    return MessageType::MOTOR_STATUS;
  if (strcmp(cmd, "SERVO_STATUS") == 0)
    return MessageType::SERVO_STATUS;
  if (strcmp(cmd, "SYSTEM_STATUS") == 0)
    return MessageType::SYSTEM_STATUS;
  if (strcmp(cmd, "FAULT_DETAILS") == 0)
    return MessageType::FAULT_DETAILS;
  if (strcmp(cmd, "STATISTICS") == 0)
    return MessageType::STATISTICS;
  if (strcmp(cmd, "TELEMETRY_ENABLE") == 0)
    return MessageType::TELEMETRY_ENABLE;
  if (strcmp(cmd, "TELEMETRY_RATE") == 0)
    return MessageType::TELEMETRY_RATE;
  if (strcmp(cmd, "HEARTBEAT") == 0)
    return MessageType::HEARTBEAT;
  if (strcmp(cmd, "STATUS_HEARTBEAT") == 0)
    return MessageType::STATUS_HEARTBEAT;
  return MessageType::UNKNOWN;
}

/**
 * @brief Convert message type enum to string
 * @param type MessageType enum
 * @return Human-readable command string
 */
inline const char *typeToString(MessageType type) {
  switch (type) {
  case MessageType::MOTOR_TARGET:
    return "MOTOR_TARGET";
  case MessageType::SERVO_TARGET:
    return "SERVO_TARGET";
  case MessageType::ENABLE_MOTORS:
    return "ENABLE_MOTORS";
  case MessageType::RESET_FAULT:
    return "RESET_FAULT";
  case MessageType::TUNE_PID:
    return "TUNE_PID";
  case MessageType::HOME_ZERO:
    return "HOME_ZERO";
  case MessageType::HOME_STALLGUARD:
    return "HOME_STALLGUARD";
  case MessageType::JOINT_STATE:
    return "JOINT_STATE";
  case MessageType::CONFIG_PARAM:
    return "CONFIG_PARAM";
  case MessageType::CONFIG_SAVE:
    return "CONFIG_SAVE";
  case MessageType::CONFIG_RESET:
    return "CONFIG_RESET";
  case MessageType::MOTOR_STATUS:
    return "MOTOR_STATUS";
  case MessageType::SERVO_STATUS:
    return "SERVO_STATUS";
  case MessageType::SYSTEM_STATUS:
    return "SYSTEM_STATUS";
  case MessageType::FAULT_DETAILS:
    return "FAULT_DETAILS";
  case MessageType::STATISTICS:
    return "STATISTICS";
  case MessageType::TELEMETRY_ENABLE:
    return "TELEMETRY_ENABLE";
  case MessageType::TELEMETRY_RATE:
    return "TELEMETRY_RATE";
  case MessageType::HEARTBEAT:
    return "HEARTBEAT";
  case MessageType::STATUS_HEARTBEAT:
    return "STATUS_HEARTBEAT";
  default:
    return "UNKNOWN";
  }
}

/**
 * @struct BaseMessage
 * Base structure for all JSON messages
 * All messages include: cmd, seq (optional), timestamp (optional)
 */
struct BaseMessage {
  MessageType type = MessageType::UNKNOWN;
  uint32_t seq = 0;          // Sequence number (for acknowledgment)
  uint64_t timestamp_ms = 0; // Message timestamp in milliseconds

  BaseMessage() = default;
  BaseMessage(MessageType t) : type(t) {}
  // Make BaseMessage polymorphic so dynamic_cast works in handlers
  virtual ~BaseMessage() = default;
};

/**
 * @struct MotorTargetMessage
 * Command: Set motor target positions (degrees)
 * Example: {"cmd":"MOTOR_TARGET","seq":42,"targets":[45.0,90.0,135.0,180.0]}
 */
struct MotorTargetMessage : public BaseMessage {
  static constexpr size_t MAX_MOTORS = NUM_STEPPERS;
  std::array<float, MAX_MOTORS> targets = {};
  uint8_t motor_count = 0;

  MotorTargetMessage() : BaseMessage(MessageType::MOTOR_TARGET) {}
};

/**
 * @struct ServoTargetMessage
 * Command: Set servo PWM values (microseconds or 0-180 degrees)
 * Example: {"cmd":"SERVO_TARGET","seq":42,"pwm_us":[1500,1400,1600]}
 * Alternative: {"cmd":"SERVO_TARGET","seq":42,"degrees":[90,85,95]}
 */
struct ServoTargetMessage : public BaseMessage {
  static constexpr size_t MAX_SERVOS = NUM_SERVOS;
  std::array<float, MAX_SERVOS> values = {}; // PWM (µs) or degrees
  uint8_t servo_count = 0;
  bool is_pwm_us = true; // true=PWM µs, false=degrees

  ServoTargetMessage() : BaseMessage(MessageType::SERVO_TARGET) {}
};

/**
 * @struct EnableMotorsMessage
 * Command: Enable or disable motor outputs
 * Example: {"cmd":"ENABLE_MOTORS","seq":42,"enable":true}
 */
struct EnableMotorsMessage : public BaseMessage {
  bool enable = false;

  EnableMotorsMessage() : BaseMessage(MessageType::ENABLE_MOTORS) {}
};

/**
 * @struct ResetFaultMessage
 * Command: Clear any active faults and re-enable if safe
 * Example: {"cmd":"RESET_FAULT","seq":42}
 */
struct ResetFaultMessage : public BaseMessage {
  bool acknowledge = false; // Optional: explicit ack

  ResetFaultMessage() : BaseMessage(MessageType::RESET_FAULT) {}
};

/**
 * @struct TunePidMessage
 * Command: Update PID controller gains
 * Example: {"cmd":"TUNE_PID","seq":42,"kp":2.0,"ki":0.05,"kd":0.1,"motor":0}
 */
struct TunePidMessage : public BaseMessage {
  uint8_t motor_index = 0; // Which motor to tune
  float kp = 0.0f;         // Proportional gain
  float ki = 0.0f;         // Integral gain
  float kd = 0.0f;         // Derivative gain
  bool antiwindup = true;  // Enable anti-windup for integrator

  TunePidMessage() : BaseMessage(MessageType::TUNE_PID) {}
};

/**
 * @struct HomeZeroMessage
 * Command: Set the current motor step counters as software zero reference
 * Example: {"cmd":"HOME_ZERO","seq":42}
 */
struct HomeZeroMessage : public BaseMessage {
  HomeZeroMessage() : BaseMessage(MessageType::HOME_ZERO) {}
};

/**
 * @struct HomeStallGuardMessage
 * Command: Run StallGuard-based homing routine.
 * Example: {"cmd":"HOME_STALLGUARD","seq":42,"motor_mask":15,"sensitivity":100}
 */
struct HomeStallGuardMessage : public BaseMessage {
  uint8_t motor_mask = 0x0F; // bit0..bit3 select motors
  uint8_t sensitivity = TMC2209_SGTHRS_DEFAULT;

  HomeStallGuardMessage() : BaseMessage(MessageType::HOME_STALLGUARD) {}
};

/**
 * @struct JointStateMessage
 * Command: Teleoperation joint positions from leader.
 * Example: {"cmd":"JOINT_STATE","seq":10,"leader_joints":[0,0,0,90,90,90],"valid_mask":[1,1,1,1,1,1]}
 */
struct JointStateMessage : public BaseMessage {
  static constexpr size_t MAX_JOINTS = 6;
  float leader_joints[MAX_JOINTS] = {};
  uint8_t valid_mask[MAX_JOINTS] = {1, 1, 1, 1, 1, 1};
  uint8_t joint_count = 0;

  JointStateMessage() : BaseMessage(MessageType::JOINT_STATE) {}
};

/**
 * @struct ConfigParamMessage
 * Command: Set a configuration parameter
 * Example:
 * {"cmd":"CONFIG_PARAM","seq":42,"param":"MAX_MOTOR_SPEED","value":300.0}
 */
struct ConfigParamMessage : public BaseMessage {
  char param_name[32] = {0};
  float value = 0.0f;

  ConfigParamMessage() : BaseMessage(MessageType::CONFIG_PARAM) {}
};

/**
 * @struct ConfigSaveMessage
 * Command: Save current configuration to EEPROM
 * Example: {"cmd":"CONFIG_SAVE","seq":42}
 */
struct ConfigSaveMessage : public BaseMessage {
  ConfigSaveMessage() : BaseMessage(MessageType::CONFIG_SAVE) {}
};

/**
 * @struct ConfigResetMessage
 * Command: Reset all configuration to defaults
 * Example: {"cmd":"CONFIG_RESET","seq":42}
 */
struct ConfigResetMessage : public BaseMessage {
  ConfigResetMessage() : BaseMessage(MessageType::CONFIG_RESET) {}
};

/**
 * @struct MotorStatusMessage
 * Response: Current motor state (from ESP32 → ROS2)
 * Example:
 * {"cmd":"MOTOR_STATUS","seq":42,"motor":0,"pos":45.2,"vel":12.5,"current":1.2}
 */
struct MotorStatusMessage : public BaseMessage {
  uint8_t motor_index = 0;
  float position_deg = 0.0f;   // Current position
  float velocity_deg_s = 0.0f; // Current velocity
  float current_ma = 0.0f;     // Motor current
  float temp_celsius = 0.0f;   // Driver temperature
  uint8_t state = 0;           // State enum (idle, moving, fault)

  MotorStatusMessage() : BaseMessage(MessageType::MOTOR_STATUS) {}
};

/**
 * @struct ServoStatusMessage
 * Response: Current servo state
 * Example:
 * {"cmd":"SERVO_STATUS","seq":42,"servo":0,"pwm_us":1500,"current_ma":250}
 */
struct ServoStatusMessage : public BaseMessage {
  uint8_t servo_index = 0;
  float pwm_us = 0.0f;       // Current PWM pulse (microseconds)
  float current_ma = 0.0f;   // Servo current
  float temp_celsius = 0.0f; // Internal temp estimate
  uint8_t state = 0;         // State enum

  ServoStatusMessage() : BaseMessage(MessageType::SERVO_STATUS) {}
};

/**
 * @struct SystemStatusMessage
 * Response: Overall system health
 * Example:
 * {"cmd":"SYSTEM_STATUS","seq":42,"uptime_ms":123456,"heap_free":65536,"heap_total":327680}
 */
struct SystemStatusMessage : public BaseMessage {
  uint32_t uptime_ms = 0;      // System uptime in milliseconds
  uint32_t heap_free = 0;      // Free heap memory
  uint32_t heap_total = 0;     // Total heap memory
  float core0_cpu_pct = 0.0f;  // Core 0 load percentage
  float core1_cpu_pct = 0.0f;  // Core 1 load percentage
  uint8_t fault_count = 0;     // Total number of faults
  bool motors_enabled = false; // Output enable status
  bool ros2_connected = false; // ROS2 heartbeat received

  SystemStatusMessage() : BaseMessage(MessageType::SYSTEM_STATUS) {}
};

/**
 * @struct FaultDetailsMessage
 * Response: Detailed fault information
 * Example:
 * {"cmd":"FAULT_DETAILS","seq":42,"fault_code":0x0001,"description":"Motor 0
 * overcurrent"}
 */
struct FaultDetailsMessage : public BaseMessage {
  uint16_t fault_code = 0;    // Fault code bitmask
  char description[64] = {0}; // Human-readable description
  uint32_t timestamp_ms = 0;  // When fault occurred
  uint8_t severity = 0;       // 0=warning, 1=error, 2=critical
  bool is_latched = false;    // Requires explicit reset

  FaultDetailsMessage() : BaseMessage(MessageType::FAULT_DETAILS) {}
};

/**
 * @struct StatisticsMessage
 * Response: Runtime statistics
 * Example:
 * {"cmd":"STATISTICS","seq":42,"control_loop_freq":400.5,"heartbeat_miss_count":0}
 */
struct StatisticsMessage : public BaseMessage {
  float control_loop_freq = 0.0f; // Measured control loop frequency
  uint32_t heartbeat_miss_count = 0;
  uint32_t uart_rx_errors = 0;
  uint32_t uart_tx_errors = 0;
  uint32_t total_commands = 0;
  uint32_t total_errors = 0;
  float avg_execution_time_ms = 0.0f;

  StatisticsMessage() : BaseMessage(MessageType::STATISTICS) {}
};

/**
 * @struct TelemetryEnableMessage
 * Command: Enable or disable telemetry stream
 * Example:
 * {"cmd":"TELEMETRY_ENABLE","seq":42,"enable":true,"types":["MOTOR_STATUS","SERVO_STATUS"]}
 */
struct TelemetryEnableMessage : public BaseMessage {
  bool enable = false;
  // Could include array of enabled telemetry types (future expansion)

  TelemetryEnableMessage() : BaseMessage(MessageType::TELEMETRY_ENABLE) {}
};

/**
 * @struct TelemetryRateMessage
 * Command: Set telemetry output rate (Hz)
 * Example: {"cmd":"TELEMETRY_RATE","seq":42,"hz":10}
 */
struct TelemetryRateMessage : public BaseMessage {
  uint16_t rate_hz = 10;

  TelemetryRateMessage() : BaseMessage(MessageType::TELEMETRY_RATE) {}
};

/**
 * @struct HeartbeatMessage
 * JSON version of ASCII heartbeat protocol
 * Example: {"cmd":"HEARTBEAT","seq":1234}
 */
struct HeartbeatMessage : public BaseMessage {
  HeartbeatMessage() : BaseMessage(MessageType::HEARTBEAT) {}
};

/**
 * @struct StatusHeartbeatMessage
 * JSON version of status heartbeat (ESP32 → ROS2)
 * Example:
 * {"cmd":"STATUS_HEARTBEAT","seq":242,"fault":0,"motors_en":1,"ros2_alive":1,"freq":400.2}
 */
struct StatusHeartbeatMessage : public BaseMessage {
  uint8_t fault = 0;      // Fault flag
  uint8_t motors_en = 0;  // Motors enabled flag
  uint8_t ros2_alive = 0; // ROS2 heartbeat received flag
  float freq = 0.0f;      // Control loop frequency

  StatusHeartbeatMessage() : BaseMessage(MessageType::STATUS_HEARTBEAT) {}
};
