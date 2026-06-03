/**
 * @file vla_command_handler.h
 * @brief VLA Inference mode command router
 *
 * Routes incoming commands in VLA Inference mode to the appropriate handler:
 * - MOTOR_TARGET, SERVO_TARGET: Forward to motor/servo controllers
 * - HEARTBEAT: Update ROS2 connection monitor
 * - CONFIG: Save to EEPROM
 */

#ifndef VLA_COMMAND_HANDLER_H
#define VLA_COMMAND_HANDLER_H

#include <ArduinoJson.h>

class VLACommandHandler {
public:
  /**
   * Initialize VLA mode handlers
   */
  static void init();

  /**
   * Route a VLA command to the appropriate handler
   *
   * @param cmd Command name (from doc["cmd"])
   * @param doc Full JSON document
   * @return true if command was processed successfully
   */
  static bool handleCommand(const char *cmd, const JsonDocument &doc);
};

#endif // VLA_COMMAND_HANDLER_H
