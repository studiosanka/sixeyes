/**
 * @file message_router.h
 * @brief Route incoming JSON messages to mode-specific handlers
 *
 * This module provides a single routing point for all incoming messages,
 * automatically directing them to the appropriate mode-specific handler
 * based on OPERATION_MODE configuration.
 *
 * In VLA Inference mode: Routes to VLA command handlers
 * In Teleoperation mode: Routes to teleoperation handlers
 */

#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#include "modules/config/mode_config.h"
#include <ArduinoJson.h>

class MessageRouter {
public:
  /**
   * Initialize the message router (call during setup)
   */
  static void init();

  /**
   * Route an incoming JSON message to the appropriate mode handler
   *
   * @param doc The parsed JSON document
   * @return true if message was successfully routed and processed
   */
  static bool routeIncomingMessage(const JsonDocument &doc);

  /**
   * Get the name of the current operation mode (for diagnostics)
   */
  static const char *getModeName();
};

#endif // MESSAGE_ROUTER_H
