/**
 * @file vla_command_handler.cpp
 * @brief Implementation of VLA mode command routing
 */

#include "vla_command_handler.h"
#include "modules/comms/uart_json_parser/uart_json_messages.h"
#include "modules/comms/uart_json_parser/uart_json_parser.h"
#include "modules/comms/uart_leader/uart_leader.h"
#include "modules/config/mode_config.h"
#include "modules/motor_control/motor_controller.h"
#include "modules/safety/safety_task.h"
#include "modules/servo_control/servo_manager.h"
#include "modules/util/logging.h"

// Callback handlers for each message type
static void handleMotorTarget(const BaseMessage &msg) {
  const MotorTargetMessage &m = static_cast<const MotorTargetMessage &>(msg);
  Logging::debugf("VLA: Received MOTOR_TARGET, seq=%lu", m.seq);
  // Convert array to std::array expected by MotorController
  std::array<float, NUM_STEPPERS> targets{};
  for (size_t i = 0; i < NUM_STEPPERS && i < m.motor_count; ++i)
    targets[i] = m.targets[i];
  MotorController::instance().setAbsoluteTargets(targets);
}

static void handleServoTarget(const BaseMessage &msg) {
  const ServoTargetMessage &m = static_cast<const ServoTargetMessage &>(msg);
  Logging::debug("VLA: Received SERVO_TARGET");
  if (m.is_pwm_us) {
    for (size_t i = 0; i < NUM_SERVOS && i < m.servo_count; ++i) {
      ServoManager::instance().setPulseMicroseconds(
          static_cast<uint8_t>(i), static_cast<uint16_t>(m.values[i]));
    }
    return;
  }

  std::array<float, NUM_SERVOS> positions{};
  for (size_t i = 0; i < NUM_SERVOS && i < m.servo_count; ++i) {
    positions[i] = m.values[i];
  }
  ServoManager::instance().setPositions(positions);
}

static void handleHeartbeat(const BaseMessage &msg) {
  // Heartbeat is handled by safety task directly
  Logging::debug("VLA: Received HEARTBEAT");
}

static void handleEnableMotors(const BaseMessage &msg) {
  const EnableMotorsMessage &m = static_cast<const EnableMotorsMessage &>(msg);
  Logging::infof("VLA: ENABLE_MOTORS: %lu", m.enable);
  if (m.enable) {
    MotorController::instance().enableMotors();
  } else {
    MotorController::instance().disableMotors();
  }
}

static void handleHomeZero(const BaseMessage &msg) {
  (void)msg;
  Logging::info("VLA: HOME_ZERO requested");
  MotorController::instance().setCurrentPositionAsZero();
}

static void handleResetFault(const BaseMessage &msg) {
  (void)msg;
  Logging::info("VLA: RESET_FAULT requested");
  SafetyTask::instance().clearFault();
  MotorController::instance().clearSkippedStepEstimates();
}

static void handleHomeStallGuard(const BaseMessage &msg) {
  const HomeStallGuardMessage &m =
      static_cast<const HomeStallGuardMessage &>(msg);
  Logging::infof("VLA: HOME_STALLGUARD requested mask=0x%02X sensitivity=%u",
                 m.motor_mask, m.sensitivity);
  if (!MotorController::instance().startStallGuardHoming(m.motor_mask,
                                                         m.sensitivity)) {
    Logging::warn("VLA: HOME_STALLGUARD failed to start");
  }
}

static void handleParseError(const char *error, uint32_t line) {
  Logging::warnf("VLA: JSON parse error on line %lu",
                 static_cast<unsigned long>(line));
  Logging::warn(error);
}

void VLACommandHandler::init() {
  Logging::info("VLACommandHandler: Initializing VLA Inference mode handlers");

  // Register handlers with the JSON parser
  UartJsonParser &parser = UartJsonParser::instance();

  parser.registerHandler(MessageType::MOTOR_TARGET, handleMotorTarget);
  parser.registerHandler(MessageType::SERVO_TARGET, handleServoTarget);
  parser.registerHandler(MessageType::HEARTBEAT, handleHeartbeat);
  parser.registerHandler(MessageType::ENABLE_MOTORS, handleEnableMotors);
  parser.registerHandler(MessageType::HOME_ZERO, handleHomeZero);
  parser.registerHandler(MessageType::HOME_STALLGUARD, handleHomeStallGuard);
  parser.registerHandler(MessageType::RESET_FAULT, handleResetFault);

  parser.setErrorHandler(handleParseError);

  Logging::info("VLACommandHandler: Handlers registered");
}

bool VLACommandHandler::handleCommand(const char *cmd,
                                      const JsonDocument &doc) {
  (void)doc;

  if (!cmd) {
    Logging::warn("VLA: Null command ignored");
    return false;
  }

  // Commands handled via UartJsonParser callbacks registered in init().
  // Validate that the command is a known VLA-mode type; reject anything else
  // so callers get an accurate return value.
  if (strcmp(cmd, "MOTOR_TARGET") == 0 || strcmp(cmd, "SERVO_TARGET") == 0 ||
      strcmp(cmd, "HEARTBEAT") == 0 || strcmp(cmd, "ENABLE_MOTORS") == 0 ||
      strcmp(cmd, "HOME_ZERO") == 0 || strcmp(cmd, "HOME_STALLGUARD") == 0 ||
      strcmp(cmd, "RESET_FAULT") == 0 || strcmp(cmd, "TUNE_PID") == 0) {
    Logging::debugf("VLA: Command dispatched via parser: %s", cmd);
    return true;
  }

  if (strcmp(cmd, "MODE_CHANGE") == 0) {
    Logging::warn("VLA: MODE_CHANGE not supported at runtime (recompile with "
                  "-DOPERATION_MODE=2 for teleoperation)");
    return false;
  }

  Logging::warnf("VLA: Unknown command rejected: %s", cmd);
  return false;
}
