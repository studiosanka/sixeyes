#pragma once
#include "modules/config/board_config.h"
#include <cstdint>

/**
 * SafetyTask: Monitor heartbeat, control EN pin, manage faults
 *
 * Runs at CONTROL_LOOP_HZ and enforces:
 * - Motors disabled if heartbeat timeout (~500 ms from ROS2 safety_node)
 * - EN pin directly controlled by fault status
 * - Fault latch policy (fault persists until explicit clear)
 * - Safe state on initialization
 *
 * Integrates with ROS2 heartbeat protocol:
 * - Receives "HB:<source_id>,<seq>" packets from laptop
 * - Sends "SB:<fault>,<motors_en>,<ros2_alive>" status updates
 * - Detects ROS2 connectivity loss and triggers motor safety shutdown
 */
class SafetyTask {
public:
  static SafetyTask &instance();
  void init(class HardwareSerial *uart = nullptr);
  void update(); // Call at CONTROL_LOOP_HZ

  // Public fault management interface
  void clearFault();
  bool isSafeToOperate() const;

  // ROS2 heartbeat bridge (used for diagnostics)
  bool isROS2Connected() const;

private:
  SafetyTask();

  static constexpr unsigned long FAULT_CHECK_INTERVAL_MS =
      10; // Check every 10ms

  unsigned long last_check_ms_ = 0;
  bool motors_enabled_ = false;
  bool user_requested_enable_ = false;
  HardwareSerial *uart_ = nullptr;
};
