#include "motor_control_scheduler.h"
#include "modules/comms/uart_json_parser/uart_json_parser.h"
#include "modules/config/mode_config.h"
#include "modules/safety/heartbeat_monitor.h"
#include "modules/safety/safety_task.h"
#include "modules/servo_control/servo_manager.h"
#if OPERATION_MODE == MODE_TELEOPERATION
#include "modules/modes/teleoperation/teleoperation_handler.h"
#endif
#include "motor_controller.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static TaskHandle_t control_task_handle = nullptr;

MotorControlScheduler &MotorControlScheduler::instance() {
  static MotorControlScheduler inst;
  return inst;
}

MotorControlScheduler::MotorControlScheduler() {}

void MotorControlScheduler::init() {
  Serial.print("MotorControlScheduler: initializing");
  Serial.print(" (CONTROL_LOOP_HZ=");
  Serial.print(CONTROL_LOOP_HZ);
  Serial.print(" Hz, period=");
  Serial.print(CONTROL_PERIOD_US);
  Serial.println(" µs)");

  Serial.println("  Control loop priority: 24/32");
  Serial.println("  Execution order:");
  Serial.println("    1. SafetyTask (fault monitoring, EN pin)");
  Serial.println("    2. MotorController (PID closed-loop)");
  Serial.println("    3. ServoManager (PWM + watchdog)");
  Serial.println("    4. HeartbeatMonitor (timeout check)");

  running_ = false;
  loop_count_ = 0;
  total_loop_time_us_ = 0;
  max_loop_time_us_ = 0;
}

void MotorControlScheduler::start() {
  if (running_) {
    Serial.println("MotorControlScheduler: already running");
    return;
  }

  Serial.println("MotorControlScheduler: starting control loop task...");

  // Create FreeRTOS task for control loop
  // Runs on core 0 (default), high priority to prevent preemption by
  // low-priority tasks
  BaseType_t result = xTaskCreatePinnedToCore(
      controlLoopTask,      // Task function
      "MotorControlLoop",   // Task name (for debugging)
      TASK_STACK_SIZE,      // Stack size (words, not bytes on ESP32)
      (void *)this,         // Pass 'this' pointer as parameter
      TASK_PRIORITY,        // Priority (high)
      &control_task_handle, // Task handle for later control
      0                     // Core ID (0 or 1 on dual-core ESP32)
  );

  if (result == pdPASS) {
    running_ = true;
    Serial.println(
        "MotorControlScheduler: control loop task created successfully");
  } else {
    Serial.println("ERROR: MotorControlScheduler failed to create task");
  }
}

void MotorControlScheduler::stop() {
  if (!running_)
    return;

  Serial.println("MotorControlScheduler: stopping control loop...");

  if (control_task_handle != nullptr) {
    vTaskDelete(control_task_handle);
    control_task_handle = nullptr;
  }

  running_ = false;
  Serial.println("MotorControlScheduler: control loop stopped");
}

bool MotorControlScheduler::running() const { return running_; }

uint32_t MotorControlScheduler::getLoopCount() const { return loop_count_; }

float MotorControlScheduler::getAverageLoopTimeMs() const {
  if (loop_count_ == 0)
    return 0.0f;
  return (total_loop_time_us_ / (float)loop_count_) / 1000.0f;
}

uint32_t MotorControlScheduler::getMaxLoopTimeMs() const {
  return max_loop_time_us_ / 1000;
}

void MotorControlScheduler::controlLoopTask(void *arg) {
  MotorControlScheduler *self = (MotorControlScheduler *)arg;
  if (!self)
    return;

  // Delay task start to allow other initialization to complete
  vTaskDelay(pdMS_TO_TICKS(100));

  TickType_t last_wake_time = xTaskGetTickCount();
  while (1) {
    self->controlLoop();
    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(1000 / CONTROL_LOOP_HZ));
  }
}

void MotorControlScheduler::controlLoop() {
  unsigned long loop_start_us = micros();

  // === Deterministic Control Loop (Priority Order) ===

  // 1. Safety Task: Check heartbeat, manage EN pin
  SafetyTask::instance().update();

  // 1b. JSON parser: drain Serial into message handlers (non-blocking)
  UartJsonParser::instance().update();
  MotorController::instance().update();

  // 3. Servo Manager: PWM updates + watchdog
  ServoManager::instance().checkWatchdog();

#if OPERATION_MODE == MODE_TELEOPERATION
  // 3b. Teleoperation pipeline: periodic follower telemetry feedback
  TeleoperationHandler::update();
#endif

  // 4. Heartbeat Monitor: Check timeout (low priority check)
  // (Already updated in SafetyTask, but can do additional logging here)

  // === Update Statistics ===
  unsigned long loop_end_us = micros();
  uint32_t loop_time_us = loop_end_us - loop_start_us;

  loop_count_++;
  total_loop_time_us_ += loop_time_us;

  if (loop_time_us > max_loop_time_us_) {
    max_loop_time_us_ = loop_time_us;
  }

  // Log statistics periodically (every 100 loops = 200 ms @ 500 Hz)
  if ((loop_count_ % STATS_INTERVAL) == 0) {
    float avg_time_ms = getAverageLoopTimeMs();
    uint32_t max_time_ms = getMaxLoopTimeMs();

    Serial.print("MotorControlScheduler: loop #");
    Serial.print(loop_count_);
    Serial.print(" avg=");
    Serial.print(avg_time_ms, 2);
    Serial.print("ms max=");
    Serial.print(max_time_ms);
    Serial.println("ms");
  }
}
