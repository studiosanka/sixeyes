/**
 * @file teleoperation_handler.cpp
 * @brief Teleoperation mode command router
 */

#include "teleoperation_handler.h"
#include "modules/comms/uart_json_parser/uart_json_parser.h"
#include "modules/drivers/tmc2209/tmc2209_driver.h"
#include "modules/motor_control/motor_controller.h"
#include "modules/safety/fault_manager.h"
#include "modules/servo_control/servo_manager.h"
#include "modules/util/logging.h"
#include <Arduino.h>
#include <algorithm>
#include <array>

namespace {
constexpr size_t TELEOP_INPUT_COUNT = 6;

constexpr size_t JOINT_BASE = 0;
constexpr size_t JOINT_SHOULDER = 1;
constexpr size_t JOINT_ELBOW = 2;
constexpr size_t JOINT_WRIST_PITCH = 3;
constexpr size_t JOINT_WRIST_YAW = 4;
constexpr size_t JOINT_GRIPPER = 5;

constexpr size_t MOTOR_BASE = 0;
constexpr size_t MOTOR_SHOULDER_A = 1;
constexpr size_t MOTOR_SHOULDER_B = 2;
constexpr size_t MOTOR_ELBOW = 3;

constexpr float STEPPER_GEAR_RATIO = 25.0f;
constexpr uint16_t DEFAULT_TELEMETRY_RATE_HZ = 100;
constexpr uint16_t MIN_TELEMETRY_RATE_HZ = 1;
constexpr uint16_t MAX_TELEMETRY_RATE_HZ = 200;

std::array<float, TELEOP_INPUT_COUNT> last_leader_joints = {
    0.0f, 0.0f, 0.0f, 90.0f, 90.0f, 90.0f};
std::array<uint8_t, TELEOP_INPUT_COUNT> last_valid_mask = {1, 1, 1, 1, 1, 1};
uint32_t last_input_seq = 0;
bool has_leader_state = false;

bool telemetry_enabled = true;
uint16_t telemetry_rate_hz = DEFAULT_TELEMETRY_RATE_HZ;
unsigned long last_telemetry_ms = 0;

float clampDegrees(float value) {
  if (value < 0.0f)
    return 0.0f;
  if (value > 180.0f)
    return 180.0f;
  return value;
}

bool isJointValid(const JsonArrayConst &valid_mask, size_t index) {
  if (index >= valid_mask.size()) {
    return true;
  }

  const JsonVariantConst value = valid_mask[index];
  if (value.is<uint8_t>()) {
    return value.as<uint8_t>() != 0;
  }
  if (value.is<bool>()) {
    return value.as<bool>();
  }
  return true;
}

uint8_t faultForJoint(size_t joint_index) {
  const bool global_fault = FaultManager::instance().hasFault();
  if (global_fault) {
    return 1;
  }
  if (joint_index < last_valid_mask.size() &&
      last_valid_mask[joint_index] == 0) {
    return 1;
  }
  return 0;
}

float measuredJointError(float measured, size_t joint_index) {
  if (!has_leader_state || joint_index >= last_leader_joints.size()) {
    return 0.0f;
  }
  if (last_valid_mask[joint_index] == 0) {
    return 0.0f;
  }
  return last_leader_joints[joint_index] - measured;
}

void updateLeaderStateCache(const JsonArrayConst &in_joints,
                            const JsonArrayConst &valid_mask, uint32_t seq) {
  for (size_t joint = 0; joint < TELEOP_INPUT_COUNT; ++joint) {
    if (joint < in_joints.size() && in_joints[joint].is<float>()) {
      last_leader_joints[joint] = in_joints[joint].as<float>();
    }
    last_valid_mask[joint] = isJointValid(valid_mask, joint) ? 1 : 0;
  }
  last_input_seq = seq;
  has_leader_state = true;
}

void applyJointState(const JsonArrayConst &in_joints,
                     const JsonArrayConst &valid_mask) {
  std::array<float, NUM_STEPPERS> motor_targets =
      MotorController::instance().getCurrentPositions();
  std::array<float, NUM_SERVOS> servo_targets =
      ServoManager::instance().getPositions();

  // --- Stepper mapping (6 teleop joints -> 4 motors) ---
  // Joint angles are absolute follower-joint angles in degrees.
  // NEMA17 motors drive 1:25 cycloidal stages, so convert to motor-shaft
  // degrees.
  if (JOINT_BASE < in_joints.size() && isJointValid(valid_mask, JOINT_BASE) &&
      in_joints[JOINT_BASE].is<float>()) {
    motor_targets[MOTOR_BASE] =
        in_joints[JOINT_BASE].as<float>() * STEPPER_GEAR_RATIO;
  }

  if (JOINT_SHOULDER < in_joints.size() &&
      isJointValid(valid_mask, JOINT_SHOULDER) &&
      in_joints[JOINT_SHOULDER].is<float>()) {
    const float shoulder_motor_deg =
        in_joints[JOINT_SHOULDER].as<float>() * STEPPER_GEAR_RATIO;
    // Dual shoulder motors are mirrored mechanically (face-to-face): equal
    // magnitude, opposite sign.
    motor_targets[MOTOR_SHOULDER_A] = shoulder_motor_deg;
    motor_targets[MOTOR_SHOULDER_B] = -shoulder_motor_deg;
  }

  if (JOINT_ELBOW < in_joints.size() && isJointValid(valid_mask, JOINT_ELBOW) &&
      in_joints[JOINT_ELBOW].is<float>()) {
    motor_targets[MOTOR_ELBOW] =
        in_joints[JOINT_ELBOW].as<float>() * STEPPER_GEAR_RATIO;
  }

  // --- Servo mapping (wrist pitch/yaw + gripper) ---
  if (JOINT_WRIST_PITCH < in_joints.size() &&
      isJointValid(valid_mask, JOINT_WRIST_PITCH) &&
      in_joints[JOINT_WRIST_PITCH].is<float>()) {
    servo_targets[0] = clampDegrees(in_joints[JOINT_WRIST_PITCH].as<float>());
  }
  if (JOINT_WRIST_YAW < in_joints.size() &&
      isJointValid(valid_mask, JOINT_WRIST_YAW) &&
      in_joints[JOINT_WRIST_YAW].is<float>()) {
    servo_targets[1] = clampDegrees(in_joints[JOINT_WRIST_YAW].as<float>());
  }
  if (JOINT_GRIPPER < in_joints.size() &&
      isJointValid(valid_mask, JOINT_GRIPPER) &&
      in_joints[JOINT_GRIPPER].is<float>()) {
    servo_targets[2] = clampDegrees(in_joints[JOINT_GRIPPER].as<float>());
  }

  MotorController::instance().setAbsoluteTargets(motor_targets);
  ServoManager::instance().setPositions(servo_targets);
}

void sendTelemetryState() {
  if (!telemetry_enabled || !has_leader_state) {
    return;
  }

  JsonDocument telemetry;
  telemetry["cmd"] = "TELEMETRY_STATE";
  telemetry["seq"] = last_input_seq;
  telemetry["ts"] = millis();

  JsonArray follower_joints = telemetry["follower_joints"].to<JsonArray>();
  JsonArray follower_joint_velocities =
      telemetry["follower_joint_velocities"].to<JsonArray>();
  JsonArray errors = telemetry["errors"].to<JsonArray>();
  JsonArray faults = telemetry["faults"].to<JsonArray>();

  JsonArray motor_positions_deg =
      telemetry["motor_positions_deg"].to<JsonArray>();
  JsonArray motor_velocities_deg_s =
      telemetry["motor_velocities_deg_s"].to<JsonArray>();
  JsonArray motor_errors_deg = telemetry["motor_errors_deg"].to<JsonArray>();
  JsonArray motor_currents_ma = telemetry["motor_currents_ma"].to<JsonArray>();

  const std::array<float, NUM_STEPPERS> current_motors =
      MotorController::instance().getCurrentPositions();
  const std::array<float, NUM_STEPPERS> current_motor_velocities =
      MotorController::instance().getCurrentVelocities();
  const std::array<float, NUM_STEPPERS> motor_errors =
      MotorController::instance().getErrors();
  const std::array<float, NUM_SERVOS> current_servos =
      ServoManager::instance().getPositions();

  for (size_t motor = 0; motor < NUM_STEPPERS; ++motor) {
    motor_positions_deg.add(current_motors[motor]);
    motor_velocities_deg_s.add(current_motor_velocities[motor]);
    motor_errors_deg.add(motor_errors[motor]);
    motor_currents_ma.add(
        TMC2209Driver::instance().getCurrent(static_cast<uint8_t>(motor)));
  }

  // Report follower joints in teleop-joint space (6 values), not raw 7-actuator
  // space.
  const float base_joint_deg = current_motors[MOTOR_BASE] / STEPPER_GEAR_RATIO;
  const float base_joint_vel_deg_s =
      current_motor_velocities[MOTOR_BASE] / STEPPER_GEAR_RATIO;
  const float shoulder_joint_deg =
      ((current_motors[MOTOR_SHOULDER_A] - current_motors[MOTOR_SHOULDER_B]) *
       0.5f) /
      STEPPER_GEAR_RATIO;
  const float shoulder_joint_vel_deg_s =
      ((current_motor_velocities[MOTOR_SHOULDER_A] -
        current_motor_velocities[MOTOR_SHOULDER_B]) *
       0.5f) /
      STEPPER_GEAR_RATIO;
  const float elbow_joint_deg =
      current_motors[MOTOR_ELBOW] / STEPPER_GEAR_RATIO;
  const float elbow_joint_vel_deg_s =
      current_motor_velocities[MOTOR_ELBOW] / STEPPER_GEAR_RATIO;

  follower_joints.add(base_joint_deg);
  follower_joint_velocities.add(base_joint_vel_deg_s);
  errors.add(measuredJointError(base_joint_deg, JOINT_BASE));
  faults.add(faultForJoint(JOINT_BASE));

  follower_joints.add(shoulder_joint_deg);
  follower_joint_velocities.add(shoulder_joint_vel_deg_s);
  errors.add(measuredJointError(shoulder_joint_deg, JOINT_SHOULDER));
  faults.add(faultForJoint(JOINT_SHOULDER));

  follower_joints.add(elbow_joint_deg);
  follower_joint_velocities.add(elbow_joint_vel_deg_s);
  errors.add(measuredJointError(elbow_joint_deg, JOINT_ELBOW));
  faults.add(faultForJoint(JOINT_ELBOW));

  follower_joints.add(current_servos[0]);
  follower_joint_velocities.add(0.0f);
  errors.add(measuredJointError(current_servos[0], JOINT_WRIST_PITCH));
  faults.add(faultForJoint(JOINT_WRIST_PITCH));

  follower_joints.add(current_servos[1]);
  follower_joint_velocities.add(0.0f);
  errors.add(measuredJointError(current_servos[1], JOINT_WRIST_YAW));
  faults.add(faultForJoint(JOINT_WRIST_YAW));

  follower_joints.add(current_servos[2]);
  follower_joint_velocities.add(0.0f);
  errors.add(measuredJointError(current_servos[2], JOINT_GRIPPER));
  faults.add(faultForJoint(JOINT_GRIPPER));

  telemetry["motors_en"] = MotorController::instance().motorsEnabled() ? 1 : 0;
  telemetry["fault_code"] =
      static_cast<uint32_t>(FaultManager::instance().current());
  telemetry["homing"] = MotorController::instance().isHoming() ? 1 : 0;
  telemetry["telemetry_rate_hz"] = telemetry_rate_hz;
  JsonArray out_valid_mask = telemetry["valid_mask"].to<JsonArray>();
  for (size_t joint = 0; joint < TELEOP_INPUT_COUNT; ++joint) {
    out_valid_mask.add(last_valid_mask[joint]);
  }

  serializeJson(telemetry, Serial);
  Serial.println();
  last_telemetry_ms = millis();
}

bool handleJointState(const JsonDocument &doc) {
  if (!doc["leader_joints"].is<JsonArray>()) {
    Logging::warn(
        "TeleoperationHandler: JOINT_STATE missing leader_joints array");
    return false;
  }

  const JsonArrayConst in_joints = doc["leader_joints"].as<JsonArrayConst>();
  const JsonArrayConst valid_mask = doc["valid_mask"].is<JsonArray>()
                                        ? doc["valid_mask"].as<JsonArrayConst>()
                                        : JsonArrayConst();

  const uint32_t seq = doc["seq"].is<uint32_t>() ? doc["seq"].as<uint32_t>()
                                                 : (last_input_seq + 1);

  updateLeaderStateCache(in_joints, valid_mask, seq);
  applyJointState(in_joints, valid_mask);
  sendTelemetryState();
  return true;
}
} // namespace

// File-scope callbacks for UartJsonParser (use anonymous-ns helpers via ADL)
namespace {
void handleJointStateMsg(const BaseMessage &base_msg) {
  const JointStateMessage &msg = static_cast<const JointStateMessage &>(base_msg);

  JsonDocument doc;
  JsonArray joints = doc["leader_joints"].to<JsonArray>();
  JsonArray mask = doc["valid_mask"].to<JsonArray>();
  for (uint8_t i = 0; i < msg.joint_count; ++i) {
    joints.add(msg.leader_joints[i]);
    mask.add(msg.valid_mask[i]);
  }
  doc["seq"] = msg.seq;

  const JsonArrayConst in_joints = doc["leader_joints"].as<JsonArrayConst>();
  const JsonArrayConst valid_mask_arr = doc["valid_mask"].as<JsonArrayConst>();
  updateLeaderStateCache(in_joints, valid_mask_arr, msg.seq);
  applyJointState(in_joints, valid_mask_arr);
}

void handleTeleParseError(const char *error, uint32_t line) {
  Logging::warnf("TeleoperationHandler: JSON parse error on line %lu",
                 static_cast<unsigned long>(line));
  Logging::warn(error);
}
} // namespace

void TeleoperationHandler::init() {
  telemetry_enabled = true;
  telemetry_rate_hz = DEFAULT_TELEMETRY_RATE_HZ;
  last_telemetry_ms = 0;
  has_leader_state = false;

  UartJsonParser &parser = UartJsonParser::instance();
  parser.registerHandler(MessageType::JOINT_STATE, handleJointStateMsg);
  parser.setErrorHandler(handleTeleParseError);

  Logging::info("TeleoperationHandler: Teleoperation mode initialized");
  Logging::info(
      "TeleoperationHandler: JOINT_STATE mapping + TELEMETRY_STATE enabled");
}

void TeleoperationHandler::update() {
  if (!telemetry_enabled || !has_leader_state) {
    return;
  }

  const uint16_t rate =
      std::max<uint16_t>(MIN_TELEMETRY_RATE_HZ, telemetry_rate_hz);
  const unsigned long period_ms = std::max<unsigned long>(1, 1000UL / rate);
  const unsigned long now_ms = millis();
  if ((now_ms - last_telemetry_ms) < period_ms) {
    return;
  }

  sendTelemetryState();
}

bool TeleoperationHandler::handleCommand(const char *cmd,
                                         const JsonDocument &doc) {
  (void)doc;

  if (!cmd) {
    Logging::warn("TeleoperationHandler: Null command ignored");
    return false;
  }

  if (strcmp(cmd, "JOINT_STATE") == 0) {
    return handleJointState(doc);
  }

  if (strcmp(cmd, "HOME_ZERO") == 0) {
    MotorController::instance().setCurrentPositionAsZero();
    Logging::info("TeleoperationHandler: HOME_ZERO applied");
    return true;
  }

  if (strcmp(cmd, "HOME_STALLGUARD") == 0) {
    const uint8_t motor_mask = doc["motor_mask"].is<uint8_t>()
                                   ? doc["motor_mask"].as<uint8_t>()
                                   : 0x0F;
    const uint8_t sensitivity = doc["sensitivity"].is<uint8_t>()
                                    ? doc["sensitivity"].as<uint8_t>()
                                    : TMC2209_SGTHRS_DEFAULT;
    if (!MotorController::instance().startStallGuardHoming(motor_mask,
                                                           sensitivity)) {
      Logging::warn("TeleoperationHandler: HOME_STALLGUARD failed to start");
      return false;
    }
    Logging::infof("TeleoperationHandler: HOME_STALLGUARD started mask=0x%02X "
                   "sensitivity=%u",
                   motor_mask, sensitivity);
    return true;
  }

  if (strcmp(cmd, "ENABLE_MOTORS") == 0) {
    const bool enable =
        doc["enable"].is<bool>() ? doc["enable"].as<bool>() : false;
    if (enable) {
      MotorController::instance().enableMotors();
      Logging::info("TeleoperationHandler: ENABLE_MOTORS=true");
    } else {
      MotorController::instance().disableMotors();
      Logging::info("TeleoperationHandler: ENABLE_MOTORS=false");
    }
    return true;
  }

  if (strcmp(cmd, "TELEMETRY_ENABLE") == 0) {
    telemetry_enabled =
        doc["enable"].is<bool>() ? doc["enable"].as<bool>() : true;
    Logging::infof("TeleoperationHandler: TELEMETRY_ENABLE=%u",
                   telemetry_enabled ? 1 : 0);
    return true;
  }

  if (strcmp(cmd, "TELEMETRY_RATE") == 0) {
    if (doc["hz"].is<uint16_t>()) {
      telemetry_rate_hz = std::max<uint16_t>(
          MIN_TELEMETRY_RATE_HZ,
          std::min<uint16_t>(MAX_TELEMETRY_RATE_HZ, doc["hz"].as<uint16_t>()));
    }
    Logging::infof("TeleoperationHandler: TELEMETRY_RATE=%u Hz",
                   telemetry_rate_hz);
    return true;
  }

  if (strcmp(cmd, "RESET_FAULT") == 0) {
    FaultManager::instance().clear();
    Logging::info("TeleoperationHandler: RESET_FAULT applied");
    return true;
  }

  Logging::warnf(
      "TeleoperationHandler: Unsupported command in teleoperation mode: %s",
      cmd);
  return false;
}
