#include "heartbeat_receiver.h"
#include "heartbeat_monitor.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// HeartbeatReceiver Implementation
// ============================================================================

HeartbeatReceiver &HeartbeatReceiver::instance() {
  static HeartbeatReceiver instance_;
  return instance_;
}

HeartbeatReceiver::HeartbeatReceiver() : rx_index_(0) { clearRxBuffer(); }

void HeartbeatReceiver::init(HardwareSerial &uart) {
  uart_ = &uart;
  // UART already initialized; this just sets the reference
  Serial.println("[HeartbeatReceiver] Initialized");
}

void HeartbeatReceiver::update() {
  if (!uart_)
    return;

  // Non-blocking read of available bytes
  while (uart_->available() && rx_index_ < RX_BUFFER_SIZE - 1) {
    int c = uart_->read();
    if (c < 0)
      break;

    rx_buffer_[rx_index_] = (uint8_t)c;
    last_rx_ms_ = millis();

    // Check for end of line (newline or linefeed)
    if (c == '\n' || c == '\r') {
      // Null-terminate the line
      rx_buffer_[rx_index_] = '\0';

      // Parse the heartbeat packet
      uint32_t source_id = 0;
      uint32_t seq = 0;
      if (parseHeartbeatPacket((const char *)rx_buffer_, source_id, seq)) {
        // Feed to HeartbeatMonitor
        HeartbeatMonitor::instance().feed(source_id, seq);
        packets_received_++;
      } else {
        packets_dropped_++;
      }

      clearRxBuffer();
    } else {
      rx_index_++;
    }
  }

  // Clear buffer on timeout (malformed packet detection)
  if (rx_index_ > 0 && millis() - last_rx_ms_ > RX_TIMEOUT_MS) {
    packets_dropped_++;
    clearRxBuffer();
  }
}

bool HeartbeatReceiver::parseHeartbeatPacket(const char *line,
                                             uint32_t &source_id,
                                             uint32_t &seq) {
  // Expected format: "HB:<source_id>,<seq>"
  // Example: "HB:0,42"

  if (!line || strlen(line) < 7) {
    return false;
  }

  // Check for "HB:" prefix
  if (line[0] != 'H' || line[1] != 'B' || line[2] != ':') {
    return false;
  }

  // Parse source_id and sequence
  // Using sscanf for simplicity; ensure format is correct
  int result = sscanf(line, "HB:%lu,%lu", (unsigned long *)&source_id,
                      (unsigned long *)&seq);

  return (result == 2);
}

void HeartbeatReceiver::clearRxBuffer() {
  memset(rx_buffer_, 0, RX_BUFFER_SIZE);
  rx_index_ = 0;
}

uint32_t HeartbeatReceiver::getPacketsReceived() const {
  return packets_received_;
}

uint32_t HeartbeatReceiver::getPacketsDropped() const {
  return packets_dropped_;
}

void HeartbeatReceiver::printStats() {
  Serial.printf("[HeartbeatReceiver] Received: %lu, Dropped: %lu\n",
                packets_received_, packets_dropped_);
}

// ============================================================================
// ROS2SafetyBridge Implementation
// ============================================================================

ROS2SafetyBridge &ROS2SafetyBridge::instance() {
  static ROS2SafetyBridge instance_;
  return instance_;
}

ROS2SafetyBridge::ROS2SafetyBridge()
    : ros2_connected_(false), last_seq_(0), last_packet_ms_(0) {}

void ROS2SafetyBridge::init(HardwareSerial &uart) {
  HeartbeatReceiver::instance().init(uart);
  Serial.println("[ROS2SafetyBridge] Initialized");
}

void ROS2SafetyBridge::update() {
  // Update the receiver (non-blocking)
  HeartbeatReceiver::instance().update();

  // Check ROS2 connection status
  checkConnection();
}

bool ROS2SafetyBridge::isROS2Connected() const { return ros2_connected_; }

uint32_t ROS2SafetyBridge::getLastSequence() const { return last_seq_; }

void ROS2SafetyBridge::checkConnection() {
  uint32_t packets_rx = HeartbeatReceiver::instance().getPacketsReceived();

  if (packets_rx > last_seq_) {
    // New packet received
    last_seq_ = packets_rx;
    last_packet_ms_ = millis();
    ros2_connected_ = true;
  } else if (millis() - last_packet_ms_ > ROS2_TIMEOUT_MS) {
    // No packet for ROS2_TIMEOUT_MS
    ros2_connected_ = false;
  }
}

void ROS2SafetyBridge::printStatus() {
  Serial.printf("[ROS2SafetyBridge] Connected: %s, Last Seq: %lu\n",
                ros2_connected_ ? "YES" : "NO", last_seq_);
  HeartbeatReceiver::instance().printStats();
}
