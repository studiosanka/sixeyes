#pragma once
#include <Arduino.h>
#include <cstdint>

/**
 * HeartbeatReceiver: Receives & parses heartbeat packets from ROS2/laptop
 *
 * Packet Format (ASCII):
 *   "HB:<source_id>,<seq>\n"
 *   Example: "HB:0,42\n" (source 0, sequence 42)
 *
 * The receiver parses incoming UART data and feeds valid heartbeats
 * to the HeartbeatMonitor for timeout detection.
 *
 * Runs in the main loop (non-blocking); compatible with ISR-based UART.
 */
class HeartbeatReceiver {
public:
  static HeartbeatReceiver &instance();

  void init(HardwareSerial &uart);
  void update(); // Call from main loop (non-blocking)

  // Statistics
  uint32_t getPacketsReceived() const;
  uint32_t getPacketsDropped() const;

  void printStats();

private:
  HeartbeatReceiver();

  static constexpr size_t RX_BUFFER_SIZE = 64;
  static constexpr unsigned long RX_TIMEOUT_MS = 50;

  HardwareSerial *uart_ = nullptr;
  uint8_t rx_buffer_[RX_BUFFER_SIZE];
  size_t rx_index_ = 0;

  uint32_t packets_received_ = 0;
  uint32_t packets_dropped_ = 0;
  unsigned long last_rx_ms_ = 0;

  // Helper methods
  bool parseHeartbeatPacket(const char *line, uint32_t &source_id,
                            uint32_t &seq);
  void clearRxBuffer();
};

/**
 * ROS2SafetyBridge: Integrates ROS2 heartbeat with safety system
 *
 * Bridges the gap between laptop-side safety_node (ROS2) and ESP32 firmware.
 * - Receives heartbeat packets from ROS2 safety_node
 * - Feeds to HeartbeatMonitor for timeout detection
 * - Monitors for sequence gaps or anomalies
 * - Logs integration status for diagnostics
 */
class ROS2SafetyBridge {
public:
  static ROS2SafetyBridge &instance();

  void init(HardwareSerial &uart);
  void update(); // Call from control loop

  bool isROS2Connected() const;
  uint32_t getLastSequence() const;

  void printStatus();

private:
  ROS2SafetyBridge();

  bool ros2_connected_ = false;
  uint32_t last_seq_ = 0;
  unsigned long last_packet_ms_ = 0;
  static constexpr unsigned long ROS2_TIMEOUT_MS = 1000; // 1 second

  // Helper method
  void checkConnection();
};
