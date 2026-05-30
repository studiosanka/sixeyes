#pragma once
#include "modules/config/board_config.h"
#include <cstdint>

/**
 * MotorControlScheduler: Deterministic control loop scheduler
 *
 * Runs motor control at exactly CONTROL_LOOP_HZ (400 Hz) using ESP32 hardware
 * timer Calls MotorController::update(), SafetyTask::update(), and
 * ServoManager::checkWatchdog() in strict priority order:
 *
 * 1. SafetyTask (check faults, EN pin control)
 * 2. MotorController (PID loop)
 * 3. ServoManager (PWM updates + watchdog)
 * 4. HeartbeatMonitor (check timeout)
 *
 * Uses FreeRTOS high-priority task on ESP32-S3 for deterministic scheduling
 * with minimal jitter and priority inversion protection.
 */
class MotorControlScheduler {
public:
  static MotorControlScheduler &instance();

  // Start/stop the scheduler
  void init();
  void start();
  void stop();
  bool running() const;

  // Get statistics for diagnostics
  uint32_t getLoopCount() const;
  float getAverageLoopTimeMs() const;
  uint32_t getMaxLoopTimeMs() const;

private:
  MotorControlScheduler();

  // Scheduler configuration
  static constexpr uint32_t CONTROL_PERIOD_US =
      1000000 / CONTROL_LOOP_HZ; // µs per loop (2500 µs @ 400 Hz)
  static constexpr uint32_t TASK_STACK_SIZE =
      4096; // FreeRTOS task stack (bytes)
  static constexpr uint32_t TASK_PRIORITY =
      24; // FreeRTOS priority (0-configMAX_PRIORITIES, ESP32 default app is 1)

  // State tracking
  bool running_ = false;
  uint32_t loop_count_ = 0;
  uint32_t total_loop_time_us_ = 0;
  uint32_t max_loop_time_us_ = 0;

  // Statistics (updated every 100 loops)
  static constexpr uint32_t STATS_INTERVAL = 100;

  // Static control loop function (for FreeRTOS task)
  static void controlLoopTask(void *arg);

  // Actual loop implementation
  void controlLoop();
};
