/**
 * @file teleoperation_handler.h
 * @brief Teleoperation mode command router (Phase 2)
 *
 * Placeholder for teleoperation mode handler.
 * Will be implemented in Phase 2 with:
 * - JOINT_STATE: Receive leader joint positions
 * - TELEMETRY_STATE: Send follower feedback
 * - MODE_CHANGE: Handle mode transitions
 */

#ifndef TELEOPERATION_HANDLER_H
#define TELEOPERATION_HANDLER_H

#include <ArduinoJson.h>

class TeleoperationHandler {
public:
  /**
   * Initialize teleoperation mode handlers
   */
  static void init();

  /**
   * Periodic teleoperation update (called from control loop scheduler)
   * Handles telemetry stream timing and emission.
   */
  static void update();

  /**
   * Route a teleoperation command to the appropriate handler
   *
   * @param cmd Command name
   * @param doc Full JSON document
   * @return true if command was processed
   */
  static bool handleCommand(const char *cmd, const JsonDocument &doc);
};

#endif // TELEOPERATION_HANDLER_H
