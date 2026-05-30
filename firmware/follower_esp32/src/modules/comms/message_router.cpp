/**
 * @file message_router.cpp
 * @brief Implementation of mode-aware message routing
 */

#include "message_router.h"
#include "modules/comms/uart_json_parser/uart_json_parser.h"
#include "modules/config/mode_config.h"
#include "modules/util/logging.h"

// Mode-specific includes (VLA Inference)
#if OPERATION_MODE == MODE_VLA_INFERENCE
#include "../modes/vla_inference/vla_command_handler.h"
#endif

// Mode-specific includes (Teleoperation)
#if OPERATION_MODE == MODE_TELEOPERATION
#include "../modes/teleoperation/teleoperation_handler.h"
#endif

void MessageRouter::init() {
  Logging::infof("MessageRouter: Initializing for mode: %s", getModeName());

  // Initialize the JSON parser with the USB-CDC serial port (non-blocking reads)
  UartJsonParser::instance().init(&Serial);

#if OPERATION_MODE == MODE_VLA_INFERENCE
  VLACommandHandler::init();
#elif OPERATION_MODE == MODE_TELEOPERATION
  TeleoperationHandler::init();
#endif
}

bool MessageRouter::routeIncomingMessage(const JsonDocument &doc) {
  if (!doc["cmd"].is<const char *>()) {
    Logging::warn("MessageRouter: Message missing or invalid 'cmd' field");
    return false;
  }

  const char *cmd = doc["cmd"].as<const char *>();

  // Runtime mode switching is not supported; reject gracefully so the caller
  // receives a meaningful false instead of silently dropping the command.
  if (strcmp(cmd, "MODE_CHANGE") == 0) {
    Logging::warnf("MessageRouter: MODE_CHANGE rejected (current mode: %s). "
                   "Recompile with -DOPERATION_MODE=1 or =2 to change modes.",
                   getModeName());
    return false;
  }

#if OPERATION_MODE == MODE_VLA_INFERENCE
  return VLACommandHandler::handleCommand(cmd, doc);

#elif OPERATION_MODE == MODE_TELEOPERATION
  // In teleoperation mode, JOINT_STATE commands are dispatched via
  // UartJsonParser callbacks registered in TeleoperationHandler::init().
  // Other utility commands (HOME_ZERO, ENABLE_MOTORS, etc.) are handled below.
  Logging::warnf("MessageRouter: Unexpected routeIncomingMessage call in "
                 "teleoperation mode for cmd: %s", cmd);
  return false;
#endif
}

const char *MessageRouter::getModeName() {
#if OPERATION_MODE == MODE_VLA_INFERENCE
  return "VLA_INFERENCE";
#elif OPERATION_MODE == MODE_TELEOPERATION
  return "TELEOPERATION";
#else
  return "UNKNOWN";
#endif
}
