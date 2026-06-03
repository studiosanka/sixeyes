#include "telemetry_formatter.h"
#include "modules/config/board_config.h"
#include "modules/motor_control/motor_controller.h"
#include "modules/servo_control/servo_manager.h"
#include <Arduino.h>

TelemetryFormatter &TelemetryFormatter::instance() {
  static TelemetryFormatter inst;
  return inst;
}

TelemetryFormatter::TelemetryFormatter() {}

void TelemetryFormatter::init() { Serial.println("TelemetryFormatter: init"); }

size_t TelemetryFormatter::formatBasicTelemetry(char *buf, size_t buf_len) {
  // Minimal JSON with uptime and control loop freq
  unsigned long ms = millis();
  int written = snprintf(buf, buf_len, "{\"uptime_ms\":%lu,\"hz\":%d}", ms,
                         CONTROL_LOOP_HZ);
  if (written < 0)
    return 0;
  return (size_t)written;
}

size_t TelemetryFormatter::formatJointTelemetry(char *buf, size_t buf_len) {
  // Compact JSON with joint states
  unsigned long ms = millis();

  auto stepper_pos = MotorController::instance().getCurrentPositions();
  auto servo_pos = ServoManager::instance().getPositions();

  int written = snprintf(
      buf, buf_len,
      "{\"t\":%lu,\"s\":[%.1f,%.1f,%.1f,%.1f],\"servo\":[%.1f,%.1f,%.1f]}", ms,
      stepper_pos[0], stepper_pos[1], stepper_pos[2], stepper_pos[3],
      servo_pos[0], servo_pos[1], servo_pos[2]);

  if (written < 0)
    return 0;
  return (size_t)written;
}
