#include "safety_task.h"
#include "fault_manager.h"
#include "heartbeat_monitor.h"
#include "heartbeat_receiver.h"
#include "heartbeat_transmitter.h"
#include "modules/config/board_config.h"
#include "modules/hal/gpio.h"
#include <Arduino.h>

SafetyTask &SafetyTask::instance() {
  static SafetyTask inst;
  return inst;
}

SafetyTask::SafetyTask() {}

void SafetyTask::init(HardwareSerial *uart) {
  uart_ = uart;

  Serial.println("SafetyTask: initialized");
  Serial.print("  - Heartbeat timeout: ");
  Serial.print(SAFETY_HEARTBEAT_TIMEOUT_MS);
  Serial.println(" ms");
  Serial.println("  - Motors disabled by default");
  Serial.println("  - Fault latch enabled (clear required)");

  // Initialize subsystems
  HAL_GPIO::init();
  FaultManager::instance().init();
  HeartbeatMonitor::instance().init(SAFETY_HEARTBEAT_TIMEOUT_MS);

  // Initialize ROS2 heartbeat bridge if UART available
  if (uart_) {
    SafetyNodeBridge::instance().init(*uart_);
    Serial.println("  - ROS2 heartbeat bridge: ENABLED");
  } else {
    Serial.println("  - ROS2 heartbeat bridge: DISABLED (no UART)");
  }

  // Motors start disabled
  motors_enabled_ = false;
  user_requested_enable_ = false;
  last_check_ms_ = millis();
}

void SafetyTask::update() {
  unsigned long now_ms = millis();

  // Check heartbeat at regular intervals (not every call, to save cycles)
  if ((now_ms - last_check_ms_) < FAULT_CHECK_INTERVAL_MS) {
    return;
  }
  last_check_ms_ = now_ms;

  // Update ROS2 heartbeat bridge (receives & transmits heartbeats)
  if (uart_) {
    // The bridge will update both receiver and transmitter in one call
    SafetyNodeBridge::instance().update(FaultManager::instance().hasFault(),
                                        motors_enabled_);
  }

  // Run heartbeat monitor check
  HeartbeatMonitor::instance().check();

  // Determine if heartbeat is healthy
  // Priority: ROS2 heartbeat feedback > internal heartbeat
  bool heartbeat_ok = HeartbeatMonitor::instance().isHealthy();
  if (uart_) {
    // If ROS2 bridge is active, use its connectivity state
    heartbeat_ok = heartbeat_ok && SafetyNodeBridge::instance().isROS2Alive();
  }

  // Fault logic: raise HEARTBEAT_TIMEOUT if heartbeat lost
  if (!heartbeat_ok && !FaultManager::instance().hasFault()) {
    FaultManager::instance().raise(FaultManager::Fault::HEARTBEAT_TIMEOUT);
    Serial.println("SafetyTask: HEARTBEAT_TIMEOUT raised - disabling motors");
  }

  // EN pin control: motors can only run if:
  // 1. No fault is present
  // 2. User has requested enable (via setAbsoluteTargets from motor_controller)
  // 3. Heartbeat is healthy (includes ROS2 connectivity if bridge enabled)
  bool safe_to_operate = !FaultManager::instance().hasFault() && heartbeat_ok;
  bool should_enable = safe_to_operate && user_requested_enable_;

  // Update EN pin state
  if (should_enable && !motors_enabled_) {
    HAL_GPIO::setMotorEnable(true);
    motors_enabled_ = true;
    Serial.println("SafetyTask: EN pin HIGH - motors enabled");
  } else if (!should_enable && motors_enabled_) {
    HAL_GPIO::setMotorEnable(false);
    motors_enabled_ = false;
    Serial.println("SafetyTask: EN pin LOW - motors disabled");
  }
}

void SafetyTask::clearFault() {
  FaultManager::instance().clear();
  Serial.println(
      "SafetyTask: fault cleared - ready to enable motors on next heartbeat");
}

bool SafetyTask::isSafeToOperate() const {
  bool heartbeat_ok = HeartbeatMonitor::instance().isHealthy();
  if (uart_) {
    heartbeat_ok = heartbeat_ok && SafetyNodeBridge::instance().isROS2Alive();
  }
  return !FaultManager::instance().hasFault() && heartbeat_ok;
}

bool SafetyTask::isROS2Connected() const {
  // Returns ROS2 connectivity status only if bridge is initialized
  if (uart_) {
    return SafetyNodeBridge::instance().isROS2Alive();
  }
  return false;
}
