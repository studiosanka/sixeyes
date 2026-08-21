// SixEyes follower ESP32 - deterministic control loop wiring modules
// Responsibilities: initialize modules and run deterministic control loop at
// CONTROL_LOOP_HZ Dual-mode firmware: VLA Inference (AI task) or Teleoperation
// (real-time mirroring)

#include "modules/comms/message_router.h" // Mode-aware message routing
#include "modules/comms/uart_leader/uart_leader.h"
#include "modules/comms/usb_cdc/usb_cdc.h" // update() called in loop()
#include "modules/config/board_config.h"
#include "modules/config/mode_config.h" // Operation mode selection
#include "modules/drivers/tmc2209/tmc2209_driver.h"
#include "modules/hal/gpio.h"
#include "modules/motor_control/motor_control_scheduler.h"
#include "modules/motor_control/motor_controller.h"
#include "modules/safety/fault_manager.h"
#include "modules/safety/heartbeat_monitor.h"
#include "modules/safety/safety_task.h"
#include "modules/servo_control/servo_manager.h"
#include "modules/telemetry/telemetry_formatter.h"
#include "modules/util/logging.h"
#include <Arduino.h>
#include <array>

void setup() {
  Serial.begin(115200);
  delay(100);
  Logging::info("SixEyes follower_esp32 starting");
  Logging::infof("Operation mode: %s", MODE_NAME());

  HAL_GPIO::init(); // ensures motors disabled by default

  // Initialize drivers and modules (shared by all modes)
  TMC2209Driver::instance().init();
  MotorController::instance().init();
  ServoManager::instance().init();
  TelemetryFormatter::instance().init();
  UartLeader::instance().init(115200);

  // Initialize message router (mode-aware)
  MessageRouter::init();

  // Initialize safety subsystem with ROS2 heartbeat bridge
  // Pass Serial (USB CDC) for ROS2 heartbeat communication
  SafetyTask::instance().init(&Serial);
  FaultManager::instance().init();

  // Start the FreeRTOS control loop (400 Hz, core 0)
  // This runs all control tasks deterministically:
  // SafetyTask → MotorController → ServoManager → HeartbeatMonitor
  MotorControlScheduler::instance().init();
  MotorControlScheduler::instance().start();

  Logging::info("Initialization complete; control loop running on FreeRTOS");
}

void loop() {
  // USB CDC: check host connect/disconnect state (100 ms poll is sufficient)
  UsbCDC::instance().update();
  // Main FreeRTOS control loop runs on core 0 at CONTROL_LOOP_HZ
  delay(100);
}
