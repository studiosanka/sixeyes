#include "heartbeat_transmitter.h"
#include "heartbeat_receiver.h"
#include <cstdio>

// ============================================================================
// HeartbeatTransmitter Implementation
// ============================================================================

HeartbeatTransmitter &HeartbeatTransmitter::instance() {
  static HeartbeatTransmitter instance_;
  return instance_;
}

HeartbeatTransmitter::HeartbeatTransmitter() : last_tx_ms_(0) {}

void HeartbeatTransmitter::init() {
  // Serial (USB CDC) already initialized by Arduino framework
  Serial.println("[HeartbeatTransmitter] Initialized");
}

void HeartbeatTransmitter::update(bool fault, bool motors_enabled,
                                  bool ros2_alive) {
  unsigned long now_ms = millis();

  // Transmit at TX_INTERVAL_MS (10 Hz)
  if (now_ms - last_tx_ms_ >= TX_INTERVAL_MS) {
    last_tx_ms_ = now_ms;
    sendStatus(fault, motors_enabled, ros2_alive);
  }
}

void HeartbeatTransmitter::sendStatus(bool fault, bool motors_enabled,
                                      bool ros2_alive) {
  // Format: "SB:<fault>,<motors_en>,<ros2_alive>\n"
  // Example: "SB:0,1,1\n"

  if (Serial.availableForWrite() > 32) {
    // Only send if buffer has space (prevent blocking)
    Serial.printf("SB:%d,%d,%d\n", fault ? 1 : 0, motors_enabled ? 1 : 0,
                  ros2_alive ? 1 : 0);
    packets_sent_++;
  } else {
    packets_dropped_++;
  }
}

uint32_t HeartbeatTransmitter::getPacketsSent() const { return packets_sent_; }

uint32_t HeartbeatTransmitter::getPacketsDropped() const {
  return packets_dropped_;
}

void HeartbeatTransmitter::printStats() {
  Serial.printf("[HeartbeatTransmitter] Sent: %lu, Dropped: %lu\n",
                packets_sent_, packets_dropped_);
}

// ============================================================================
// SafetyNodeBridge Implementation
// ============================================================================

SafetyNodeBridge &SafetyNodeBridge::instance() {
  static SafetyNodeBridge instance_;
  return instance_;
}

SafetyNodeBridge::SafetyNodeBridge() : ros2_alive_(false) {}

void SafetyNodeBridge::init(HardwareSerial &uart) {
  HeartbeatReceiver::instance().init(uart);
  HeartbeatTransmitter::instance().init();
  Serial.println("[SafetyNodeBridge] Initialized (RX + TX)");
}

void SafetyNodeBridge::update(bool fault, bool motors_enabled) {
  // Update receiver (parses incoming heartbeats)
  HeartbeatReceiver::instance().update();

  // Query ROS2 state (checks if we've received recent heartbeats)
  ros2_alive_ = ROS2SafetyBridge::instance().isROS2Connected();

  // Update transmitter (sends status feedback)
  HeartbeatTransmitter::instance().update(fault, motors_enabled, ros2_alive_);
}

bool SafetyNodeBridge::isROS2Alive() const { return ros2_alive_; }

void SafetyNodeBridge::printBridgeStatus() {
  Serial.println("\n[SafetyNodeBridge] Status:");
  ROS2SafetyBridge::instance().printStatus();
  HeartbeatTransmitter::instance().printStats();
}
