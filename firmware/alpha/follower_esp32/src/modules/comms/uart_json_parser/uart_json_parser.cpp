#include "uart_json_parser.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cstring>

/**
 * @file uart_json_parser.cpp
 * @brief Implementation of JSON message parser
 *
 * Non-blocking parser that processes complete lines of JSON from UART.
 * Integrates with ArduinoJson for efficient parsing.
 */

UartJsonParser::UartJsonParser()
    : uart_(nullptr), rx_buffer_(nullptr), buffer_size_(DEFAULT_BUFFER_SIZE),
      buffer_pos_(0), handler_count_(0), message_count_(0), error_count_(0),
      line_number_(0), has_complete_line_(false), line_end_pos_(0) {}

UartJsonParser &UartJsonParser::instance() {
  static UartJsonParser parser;
  return parser;
}

void UartJsonParser::init(HardwareSerial *uart, size_t buffer_size) {
  uart_ = uart;
  buffer_size_ = std::min(buffer_size, size_t(2048)); // Cap at 2KB

  if (rx_buffer_) {
    delete[] rx_buffer_;
  }
  rx_buffer_ = new uint8_t[buffer_size_];

  clearBuffer();

  // Optional: log initialization
  if (uart_) {
    uart_->println("[UartJsonParser] Initialized; buffer size: " +
                   String(buffer_size_) + " bytes");
  }
}

void UartJsonParser::clearBuffer() {
  if (rx_buffer_) {
    memset(rx_buffer_, 0, buffer_size_);
  }
  buffer_pos_ = 0;
  has_complete_line_ = false;
  line_end_pos_ = 0;
}

void UartJsonParser::registerHandler(MessageType type, MessageHandler handler) {
  if (handler_count_ >= MAX_HANDLERS) {
    reportError("Handler table full");
    return;
  }

  // Check if already registered
  for (uint8_t i = 0; i < handler_count_; i++) {
    if (handler_types_[i] == type) {
      handlers_[i] = handler; // Replace existing
      return;
    }
  }

  // Add new handler
  handlers_[handler_count_] = handler;
  handler_types_[handler_count_] = type;
  handler_count_++;
}

void UartJsonParser::unregisterHandler(MessageType type) {
  for (uint8_t i = 0; i < handler_count_; i++) {
    if (handler_types_[i] == type) {
      // Shift remaining handlers
      for (uint8_t j = i; j < handler_count_ - 1; j++) {
        handlers_[j] = handlers_[j + 1];
        handler_types_[j] = handler_types_[j + 1];
      }
      handler_count_--;
      return;
    }
  }
}

void UartJsonParser::setErrorHandler(ErrorHandler handler) {
  error_handler_ = handler;
}

const char *UartJsonParser::getLastError() const { return last_error_; }

uint8_t UartJsonParser::update() {
  if (!uart_)
    return 0;

  uint8_t parsed_count = 0;

  // Read available bytes from UART
  while (uart_->available() && buffer_pos_ < buffer_size_ - 1) {
    int c = uart_->read();
    if (c < 0)
      break;

    if (c == '\n') {
      // Found end of line
      rx_buffer_[buffer_pos_] = '\0';

      // Parse the complete line
      if (buffer_pos_ > 0) { // Ignore empty lines
        if (parseLine(reinterpret_cast<const char *>(rx_buffer_))) {
          parsed_count++;
        }
      }

      // Reset buffer for next line
      buffer_pos_ = 0;
      line_number_++;

    } else if (c == '\r') {
      // Skip carriage return
      continue;
    } else {
      // Add character to buffer
      rx_buffer_[buffer_pos_++] = static_cast<uint8_t>(c);
    }
  }

  // Check for buffer overflow
  if (buffer_pos_ >= buffer_size_ - 1) {
    reportError("RX buffer overflow");
    clearBuffer();
  }

  return parsed_count;
}

bool UartJsonParser::parseLine(const char *json_line) {
  if (!json_line || strlen(json_line) == 0) {
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json_line);

  if (error) {
    reportError(error.c_str());
    return false;
  }

  // Extract message type
  if (!doc["cmd"].is<const char *>()) {
    reportError("Missing or invalid required field: cmd");
    return false;
  }

  const char *cmd_str = doc["cmd"].as<const char *>();
  MessageType msg_type = cmdStringToType(cmd_str);

  if (msg_type == MessageType::UNKNOWN) {
    String err = String("Unknown command type: ") + String(cmd_str);
    reportError(err.c_str());
    return false;
  }

  // Create base message
  BaseMessage base_msg(msg_type);
  if (doc["seq"].is<uint32_t>()) {
    base_msg.seq = doc["seq"].as<uint32_t>();
  }
  if (doc["ts"].is<uint64_t>()) {
    base_msg.timestamp_ms = doc["ts"].as<uint64_t>();
  }

  // Parse type-specific fields
  bool parse_success = false;

  switch (msg_type) {
  case MessageType::MOTOR_TARGET: {
    MotorTargetMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;

    if (doc["targets"].is<JsonArray>()) {
      JsonArray targets = doc["targets"];
      msg.motor_count = std::min((size_t)NUM_STEPPERS, targets.size());
      for (uint8_t i = 0; i < msg.motor_count; i++) {
        msg.targets[i] = targets[i].as<float>();
      }
      parse_success = true;
    }
    if (parse_success) {
      message_count_++;
      dispatchMessage(msg);
    }
    break;
  }

  case MessageType::SERVO_TARGET: {
    ServoTargetMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;

    bool has_pwm = false, has_degrees = false;

    if (doc["pwm_us"].is<JsonArray>()) {
      JsonArray pwm = doc["pwm_us"];
      msg.servo_count = std::min((size_t)NUM_SERVOS, pwm.size());
      msg.is_pwm_us = true;
      for (uint8_t i = 0; i < msg.servo_count; i++) {
        msg.values[i] = pwm[i].as<float>();
      }
      has_pwm = true;
    } else if (doc["degrees"].is<JsonArray>()) {
      JsonArray deg = doc["degrees"];
      msg.servo_count = std::min((size_t)NUM_SERVOS, deg.size());
      msg.is_pwm_us = false;
      for (uint8_t i = 0; i < msg.servo_count; i++) {
        msg.values[i] = deg[i].as<float>();
      }
      has_degrees = true;
    }

    parse_success = has_pwm || has_degrees;
    if (parse_success) {
      message_count_++;
      dispatchMessage(msg);
    }
    break;
  }

  case MessageType::ENABLE_MOTORS: {
    EnableMotorsMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;

    if (doc["enable"].is<bool>()) {
      msg.enable = doc["enable"].as<bool>();
      parse_success = true;
      message_count_++;
      dispatchMessage(msg);
    }
    break;
  }

  case MessageType::RESET_FAULT: {
    ResetFaultMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;

    if (doc["ack"].is<bool>()) {
      msg.acknowledge = doc["ack"].as<bool>();
    }
    parse_success = true;
    message_count_++;
    dispatchMessage(msg);
    break;
  }

  case MessageType::TUNE_PID: {
    TunePidMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;

    if (doc["motor"].is<uint8_t>()) {
      msg.motor_index = doc["motor"].as<uint8_t>();
    }
    if (doc["kp"].is<float>()) {
      msg.kp = doc["kp"].as<float>();
    }
    if (doc["ki"].is<float>()) {
      msg.ki = doc["ki"].as<float>();
    }
    if (doc["kd"].is<float>()) {
      msg.kd = doc["kd"].as<float>();
    }
    if (doc["antiwindup"].is<bool>()) {
      msg.antiwindup = doc["antiwindup"].as<bool>();
    }
    parse_success = true;
    message_count_++;
    dispatchMessage(msg);
    break;
  }

  case MessageType::HOME_ZERO: {
    HomeZeroMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;
    parse_success = true;
    message_count_++;
    dispatchMessage(msg);
    break;
  }

  case MessageType::HOME_STALLGUARD: {
    HomeStallGuardMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;
    if (doc["motor_mask"].is<uint8_t>()) {
      msg.motor_mask = doc["motor_mask"].as<uint8_t>();
    }
    if (doc["sensitivity"].is<uint8_t>()) {
      msg.sensitivity = doc["sensitivity"].as<uint8_t>();
    }
    parse_success = true;
    message_count_++;
    dispatchMessage(msg);
    break;
  }

  case MessageType::JOINT_STATE: {
    JointStateMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;
    if (doc["leader_joints"].is<JsonArray>()) {
      JsonArray arr = doc["leader_joints"];
      msg.joint_count = std::min((size_t)JointStateMessage::MAX_JOINTS, arr.size());
      for (uint8_t i = 0; i < msg.joint_count; ++i) {
        msg.leader_joints[i] = arr[i].as<float>();
      }
    }
    if (doc["valid_mask"].is<JsonArray>()) {
      JsonArray vm = doc["valid_mask"];
      for (uint8_t i = 0; i < msg.joint_count; ++i) {
        if (i < vm.size()) {
          msg.valid_mask[i] = vm[i].as<uint8_t>();
        }
      }
    }
    parse_success = true;
    message_count_++;
    dispatchMessage(msg);
    break;
  }

  case MessageType::HEARTBEAT: {
    HeartbeatMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;
    parse_success = true;
    message_count_++;
    dispatchMessage(msg);
    break;
  }

  case MessageType::STATUS_HEARTBEAT: {
    StatusHeartbeatMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;

    if (doc["fault"].is<uint8_t>())
      msg.fault = doc["fault"].as<uint8_t>();
    if (doc["motors_en"].is<uint8_t>())
      msg.motors_en = doc["motors_en"].as<uint8_t>();
    if (doc["ros2_alive"].is<uint8_t>())
      msg.ros2_alive = doc["ros2_alive"].as<uint8_t>();
    if (doc["freq"].is<float>())
      msg.freq = doc["freq"].as<float>();

    parse_success = true;
    message_count_++;
    dispatchMessage(msg);
    break;
  }

  // Configuration messages
  case MessageType::CONFIG_PARAM: {
    ConfigParamMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;

    if (doc["param"].is<const char *>() && doc["value"].is<float>()) {
      strlcpy(msg.param_name, doc["param"].as<const char *>(),
              sizeof(msg.param_name));
      msg.value = doc["value"].as<float>();
      parse_success = true;
      message_count_++;
      dispatchMessage(msg);
    }
    break;
  }

  case MessageType::TELEMETRY_ENABLE: {
    TelemetryEnableMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;

    if (doc["enable"].is<bool>()) {
      msg.enable = doc["enable"].as<bool>();
      parse_success = true;
      message_count_++;
      dispatchMessage(msg);
    }
    break;
  }

  case MessageType::TELEMETRY_RATE: {
    TelemetryRateMessage msg;
    msg.seq = base_msg.seq;
    msg.timestamp_ms = base_msg.timestamp_ms;

    if (doc["hz"].is<uint16_t>()) {
      msg.rate_hz = doc["hz"].as<uint16_t>();
      parse_success = true;
      message_count_++;
      dispatchMessage(msg);
    }
    break;
  }

  // Other message types (CONFIG_SAVE, CONFIG_RESET, etc.) just dispatch with
  // base message
  default: {
    parse_success = true;
    message_count_++;
    dispatchMessage(base_msg);
    break;
  }
  }

  return parse_success;
}

void UartJsonParser::dispatchMessage(const BaseMessage &msg) {
  if (!callHandler(msg)) {
    // No handler registered for this type
    // Could log this or silently ignore based on configuration
  }
}

bool UartJsonParser::callHandler(const BaseMessage &msg) {
  for (uint8_t i = 0; i < handler_count_; i++) {
    if (handler_types_[i] == msg.type && handlers_[i]) {
      handlers_[i](msg);
      return true;
    }
  }
  return false;
}

void UartJsonParser::reportError(const char *error_msg) {
  error_count_++;
  strlcpy(last_error_, error_msg, sizeof(last_error_));

  if (error_handler_) {
    error_handler_(error_msg, line_number_);
  }

  // Optional: log to serial for debugging
  if (uart_) {
    uart_->print("[UartJsonParser ERROR] Line ");
    uart_->print(line_number_);
    uart_->print(": ");
    uart_->println(error_msg);
  }
}

/**
 * @brief Helper to send JSON response
 *
 * Example usage:
 *   MotorStatusMessage status = {...};
 *   sendJsonStatus(Serial, status);
 */
void sendJsonStatus(HardwareSerial &uart, const MotorStatusMessage &msg) {
  JsonDocument doc;
  doc["cmd"] = "MOTOR_STATUS";
  doc["seq"] = msg.seq;
  doc["motor"] = msg.motor_index;
  doc["pos"] = msg.position_deg;
  doc["vel"] = msg.velocity_deg_s;
  doc["current"] = msg.current_ma;
  doc["temp"] = msg.temp_celsius;
  doc["state"] = msg.state;

  serializeJson(doc, uart);
  uart.println();
}

void sendJsonStatus(HardwareSerial &uart, const ServoStatusMessage &msg) {
  JsonDocument doc;
  doc["cmd"] = "SERVO_STATUS";
  doc["seq"] = msg.seq;
  doc["servo"] = msg.servo_index;
  doc["pwm_us"] = msg.pwm_us;
  doc["current"] = msg.current_ma;
  doc["temp"] = msg.temp_celsius;
  doc["state"] = msg.state;

  serializeJson(doc, uart);
  uart.println();
}

void sendJsonStatus(HardwareSerial &uart, const SystemStatusMessage &msg) {
  JsonDocument doc;
  doc["cmd"] = "SYSTEM_STATUS";
  doc["seq"] = msg.seq;
  doc["uptime_ms"] = msg.uptime_ms;
  doc["heap_free"] = msg.heap_free;
  doc["heap_total"] = msg.heap_total;
  doc["core0_cpu"] = msg.core0_cpu_pct;
  doc["core1_cpu"] = msg.core1_cpu_pct;
  doc["fault_count"] = msg.fault_count;
  doc["motors_en"] = msg.motors_enabled;
  doc["ros2_alive"] = msg.ros2_connected;

  serializeJson(doc, uart);
  uart.println();
}

void sendJsonStatus(HardwareSerial &uart, const StatisticsMessage &msg) {
  JsonDocument doc;
  doc["cmd"] = "STATISTICS";
  doc["seq"] = msg.seq;
  doc["loop_freq"] = msg.control_loop_freq;
  doc["heartbeat_miss"] = msg.heartbeat_miss_count;
  doc["uart_rx_err"] = msg.uart_rx_errors;
  doc["uart_tx_err"] = msg.uart_tx_errors;
  doc["cmd_total"] = msg.total_commands;
  doc["error_total"] = msg.total_errors;
  doc["avg_exec_ms"] = msg.avg_execution_time_ms;

  serializeJson(doc, uart);
  uart.println();
}
