#pragma once
#include <Arduino.h>
#include <cstdint>

/**
 * HeartbeatTransmitter: Sends heartbeat & status packets to ROS2/laptop
 *
 * Packet Format (ASCII):
 *   Status: "SB:<fault>,<motors_en>,<ros2_alive>\n"
 *   Example: "SB:0,1,1\n" (no fault, motors enabled, ROS2 connected)
 *
 * Transmits firmware status at ~10 Hz to indicate:
 * - Fault status (triggers emergency shutdown on ROS2 side if fault=1)
 * - Motor enable status (reflects current EN pin state)
 * - ROS2 connectivity (immediate feedback loop)
 *
 * Non-blocking USB-CDC writes; designed for integration with control loop.
 */
class HeartbeatTransmitter {
public:
  static HeartbeatTransmitter &instance();

  void init();
  void update(bool fault, bool motors_enabled,
              bool ros2_alive); // Call from control loop

  // Manual transmission (useful for diagnostics)
  void sendStatus(bool fault, bool motors_enabled, bool ros2_alive);

  // Statistics
  uint32_t getPacketsSent() const;
  uint32_t getPacketsDropped() const;

  void printStats();

private:
  HeartbeatTransmitter();

  unsigned long last_tx_ms_ = 0;
  static constexpr unsigned long TX_INTERVAL_MS = 100; // 10 Hz

  uint32_t packets_sent_ = 0;
  uint32_t packets_dropped_ = 0;
};

/**
 * SafetyNodeBridge: High-level interface for ROS2 safety integration
 *
 * Combines HeartbeatReceiver + HeartbeatTransmitter for symmetrical
 * heartbeat protocol. This bridge ensures:
 * - Laptop sends periodic heartbeats
 * - Firmware acknowledges with status
 * - Both sides can detect loss-of-communication
 *
 * Integrates with SafetyTask to enforce motor shutdown on heartbeat timeout.
 */
class SafetyNodeBridge {
public:
  static SafetyNodeBridge &instance();

  void init(HardwareSerial &uart);
  void update(bool fault,
              bool motors_enabled); // Call from control loop / SafetyTask

  // Query ROS2 state (non-blocking, used by SafetyTask)
  bool isROS2Alive() const;

  // Diagnostics
  void printBridgeStatus();

private:
  SafetyNodeBridge();

  bool ros2_alive_ = false;
};
