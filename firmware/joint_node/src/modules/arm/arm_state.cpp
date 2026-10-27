#include "arm_state.h"
#include "modules/comms/can/can_driver.h"
#include "modules/config/board_config.h"
#include "modules/config/joint_config.h"
#include "modules/config/node_config.h"
#include "modules/motor_control/motor_controller.h"
#include "modules/safety/can_safety_task.h"
#include "modules/safety/stop_button.h"
#include "modules/telemetry/node_telemetry.h"
#include "modules/util/logging.h"
#include <Arduino.h>
#include <cmath>

// The Elbow turns its servos off after 100 ms without a command (ServoManager),
// so they are re-sent well inside that.
static constexpr uint32_t SERVO_REFRESH_MS = 50;
// While a servo ramps, send a new angle this often (smooth enough for a slow move).
static constexpr uint32_t SERVO_RAMP_STEP_MS = 40;
// HOME is repeated until the node reports it started: a lost frame must not stall homing.
static constexpr uint32_t HOME_RESEND_MS = 200;
// Slack on top of the node's own homing timeout, for the status round trip.
static constexpr uint32_t HOMING_STEP_SLACK_MS = 5000;
// A "homing failed" flag older than this is not mistaken for a previous run's.
static constexpr uint32_t STALE_FAILED_FLAG_MS = 500;

// gripper, wrist roll, wrist pitch, elbow, shoulder (both nodes together), base
const ArmState::HomeStep ArmState::kSequence[6] = {
    {true, SERVO_INDEX_GRIPPER, 0},
    {true, SERVO_INDEX_WRIST_ROLL, 0},
    {true, SERVO_INDEX_WRIST_PITCH, 0},
    {false, 0, static_cast<uint8_t>(1u << JOINT_NODE_ELBOW)},
    {false, 0, static_cast<uint8_t>((1u << JOINT_NODE_SHOULDER_L) | (1u << JOINT_NODE_SHOULDER_R))},
    {false, 0, static_cast<uint8_t>(1u << JOINT_NODE_BASE)},
};

ArmState &ArmState::instance() {
  static ArmState inst;
  return inst;
}

// ---------------------------------------------------------------- CAN in

void ArmState::onEncoderTelemetry(uint8_t node_id, const EncoderTelemetryFrame &frame) {
  if (node_id < JOINT_NODE_SHOULDER_L || node_id > JOINT_NODE_ELBOW) return;
  nodes_[node_id].counts = frame.position;
  nodes_[node_id].seen = true;
}

void ArmState::onNodeStatus(uint8_t node_id, const NodeStatusFrame &frame) {
  if (node_id < JOINT_NODE_SHOULDER_L || node_id > JOINT_NODE_ELBOW) return;
  Node &n = nodes_[node_id];
  n.status_seen = true;
  n.stall = frame.stall_detected != 0;
  n.encoder_fault = (frame.fault_bitmask & NODE_FAULT_ENCODER) != 0;
  n.homing_failed = (frame.fault_bitmask & NODE_FAULT_HOMING_FAILED) != 0;
  n.homed = (frame.fault_bitmask & NODE_STATE_HOMED) != 0;
  n.homing = (frame.fault_bitmask & NODE_STATE_HOMING) != 0;
}

bool ArmState::nodeHomed(uint8_t id) const {
  return id == JOINT_NODE_BASE ? MotorController::instance().isHomed() : nodes_[id].homed;
}
bool ArmState::nodeHoming(uint8_t id) const {
  return id == JOINT_NODE_BASE ? MotorController::instance().isHoming() : nodes_[id].homing;
}
bool ArmState::nodeHomingFailed(uint8_t id) const {
  return id == JOINT_NODE_BASE ? MotorController::instance().homingFailed() : nodes_[id].homing_failed;
}

// ---------------------------------------------------------------- CAN out

void ArmState::sendMotorFrame(uint8_t node_id, float output_deg, uint8_t flags) {
  MotorTargetFrame frame = {};
  frame.target_position = output_deg;
  // A HOME frame carries the homing run's number, so a repeated frame is not mistaken for a new request.
  frame.seq = (flags & MOTOR_FLAG_HOME) ? home_session_ : motor_seq_++;
  frame.flags = flags;
  CanDriver::instance().send(canIdFor(CAN_ID_MOTOR_TARGET, node_id), frame);
}

void ArmState::sendServo(uint8_t servo_index, float deg) {
  if (servo_index >= 3) return;
  if (deg < 0.0f) deg = 0.0f;
  if (deg > 180.0f) deg = 180.0f;
  ServoTargetFrame frame = {};
  frame.servo_index = servo_index;
  frame.target_angle = deg;
  frame.seq = servo_seq_++;
  CanDriver::instance().send(canIdFor(CAN_ID_SERVO_TARGET, JOINT_NODE_ELBOW), frame);
  servo_deg_[servo_index] = deg;
  servo_commanded_[servo_index] = true;
  last_servo_refresh_ms_ = millis();
}

// ---------------------------------------------------------------- measured

bool ArmState::nodeRad(uint8_t node_id, float &rad) const {
  if (node_id == JOINT_NODE_BASE) {
    // Base reads its own encoder directly; a faulted one has nothing to report.
    if (NodeTelemetry::instance().encoderFault()) return false;
    rad = motorCountsToOutputRad(NodeTelemetry::instance().counts(), node_id);
    return true;
  }
  const Node &n = nodes_[node_id];
  if (!n.seen || n.encoder_fault) return false;
  rad = motorCountsToOutputRad(n.counts, node_id);
  return true;
}

void ArmState::measured(float out[SIXEYES_JOINT_COUNT]) {
  // Joints with nothing to report yet read 0. The state's homing_state says
  // whether the numbers mean anything: until homed they are only relative to
  // power-on, because the encoder sits on the motor shaft (see joint_config.h).
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) out[i] = 0.0f;

  float rad = 0.0f;
  if (nodeRad(JOINT_NODE_BASE, rad)) out[SIXEYES_JOINT_BASE] = rad;
  if (nodeRad(JOINT_NODE_ELBOW, rad)) out[SIXEYES_JOINT_ELBOW] = rad;

  // Shoulder: two motors on one gearbox, reported as one joint. Average the
  // two; if only one has reported, use it.
  float l = 0.0f, r = 0.0f;
  const bool have_l = nodeRad(JOINT_NODE_SHOULDER_L, l);
  const bool have_r = nodeRad(JOINT_NODE_SHOULDER_R, r);
  if (have_l && have_r) {
    out[SIXEYES_JOINT_SHOULDER] = 0.5f * (l + r);
  } else if (have_l) {
    out[SIXEYES_JOINT_SHOULDER] = l;
  } else if (have_r) {
    out[SIXEYES_JOINT_SHOULDER] = r;
  }
  // TODO(shoulder): flag a fault if L and R disagree by more than a bench-tuned
  // amount (they share a gearbox and must move together).

  // Servos have no feedback: report what they were last commanded.
  if (servo_commanded_[SERVO_INDEX_WRIST_PITCH]) {
    out[SIXEYES_JOINT_WRIST_PITCH] = servoDegToWristRad(servo_deg_[SERVO_INDEX_WRIST_PITCH]);
  }
  if (servo_commanded_[SERVO_INDEX_WRIST_ROLL]) {
    out[SIXEYES_JOINT_WRIST_ROLL] = servoDegToWristRad(servo_deg_[SERVO_INDEX_WRIST_ROLL]);
  }
  if (servo_commanded_[SERVO_INDEX_GRIPPER]) {
    out[SIXEYES_JOINT_GRIPPER] = servoDegToGripper(servo_deg_[SERVO_INDEX_GRIPPER]);
  }
}

uint16_t ArmState::faultBits() {
  uint16_t bits = 0;
  if (CanSafetyTask::instance().hasManualStop()) {
    bits |= SIXEYES_FAULT_HW_ESTOP; // someone pressed a stop button on some board
  } else if (CanSafetyTask::instance().hasBusFault()) {
    bits |= SIXEYES_FAULT_BUS_LOST;
  }
  if (MotorController::instance().isStallDetected()) bits |= SIXEYES_FAULT_STALL;
  for (uint8_t id = JOINT_NODE_SHOULDER_L; id <= JOINT_NODE_ELBOW; id++) {
    if (nodes_[id].stall) bits |= SIXEYES_FAULT_STALL;
  }
  if (homing_status_ == HOMING_FAILED) bits |= SIXEYES_FAULT_HOMING_FAILED;
  return bits;
}

bool ArmState::moving() {
  if (homing_status_ == HOMING_RUNNING) return true;
  if (!have_applied_) return false;
  float now[SIXEYES_JOINT_COUNT];
  measured(now);
  for (int i = 0; i < 3; i++) { // the three stepper joints; servos have no feedback
    if (fabsf(now[i] - applied_[i]) > 0.02f) return true;
  }
  return false;
}

// ---------------------------------------------------------------- targets

void ArmState::applyTargets(const float t[SIXEYES_JOINT_COUNT]) {
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) applied_[i] = t[i];
  have_applied_ = true;

  // Steppers: MOTOR_TARGET carries degrees at the output shaft.
  MotorController::instance().setTargetPosition(t[SIXEYES_JOINT_BASE] * RAD_TO_DEG_F); // Base itself
  const float shoulder_deg = t[SIXEYES_JOINT_SHOULDER] * RAD_TO_DEG_F;
  sendMotorFrame(JOINT_NODE_SHOULDER_L, shoulder_deg, 0); // both shoulder nodes get the
  sendMotorFrame(JOINT_NODE_SHOULDER_R, shoulder_deg, 0); // same target and move together
  sendMotorFrame(JOINT_NODE_ELBOW, t[SIXEYES_JOINT_ELBOW] * RAD_TO_DEG_F, 0);

  // Servos on the Elbow board: SERVO_TARGET carries degrees at the servo.
  sendServo(SERVO_INDEX_WRIST_PITCH, wristRadToServoDeg(t[SIXEYES_JOINT_WRIST_PITCH]));
  sendServo(SERVO_INDEX_WRIST_ROLL, wristRadToServoDeg(t[SIXEYES_JOINT_WRIST_ROLL]));
  sendServo(SERVO_INDEX_GRIPPER, gripperToServoDeg(t[SIXEYES_JOINT_GRIPPER]));
}

// Every stepper decelerates and holds where it is. Servos keep their last command.
void ArmState::stopAllMotors() {
  MotorController::instance().holdHere(); // also aborts homing on Base
  for (uint8_t id = JOINT_NODE_SHOULDER_L; id <= JOINT_NODE_ELBOW; id++) {
    sendMotorFrame(id, 0.0f, MOTOR_FLAG_ABORT);
  }
}

void ArmState::holdPosition() {
  stopAllMotors();
  // The targets we sent are now stale: forget them so moving() is judged afresh.
  have_applied_ = false;
}

bool ArmState::clearFaults() {
  // Refused while a stop button is still held on Base. A button held on another
  // board cannot be seen from here; that board just stops the arm again.
  if (StopButton::pressed()) return false;
  CanSafetyTask::instance().clearBusFault();
  MotorController::instance().resetFault();
  for (uint8_t id = JOINT_NODE_SHOULDER_L; id <= JOINT_NODE_ELBOW; id++) {
    sendMotorFrame(id, 0.0f, MOTOR_FLAG_RESET_FAULT);
    nodes_[id].stall = false;
    nodes_[id].homing_failed = false;
  }
  if (homing_status_ == HOMING_FAILED) homing_status_ = HOMING_IDLE;
  return true;
}

// ---------------------------------------------------------------- homing

bool ArmState::startHoming() {
  if (homing_status_ == HOMING_RUNNING) return true;

  // Everything must be in place before anything moves.
  const char *why = nullptr;
  if (CanSafetyTask::instance().hasBusFault()) {
    why = "a bus fault is latched";
  } else if (!CanSafetyTask::instance().busHeartbeatAlive()) {
    why = "Base's own BUS_HEARTBEAT is not running";
  } else if (NodeTelemetry::instance().encoderFault()) {
    why = "Base's encoder is not working";
  } else {
    for (uint8_t id = JOINT_NODE_SHOULDER_L; id <= JOINT_NODE_ELBOW; id++) {
      if (!nodes_[id].status_seen || !nodes_[id].seen) {
        Logging::errorf("ArmState: cannot home: node %u has not reported yet", id);
        return false;
      }
      if (nodes_[id].encoder_fault) {
        Logging::errorf("ArmState: cannot home: node %u has an encoder fault", id);
        return false;
      }
      if (nodes_[id].homing) {
        Logging::errorf("ArmState: cannot home: node %u is already homing", id);
        return false;
      }
    }
  }
  if (why) {
    Logging::errorf("ArmState: cannot home: %s", why);
    return false;
  }

  have_applied_ = false;
  home_session_++;
  step_ = 0;
  homing_status_ = HOMING_RUNNING;
  Logging::info("ArmState: homing sequence started");
  beginStep(millis());
  return homing_status_ == HOMING_RUNNING; // beginStep() may fail the run at once
}

void ArmState::beginStep(uint32_t now_ms) {
  const HomeStep &s = kSequence[step_];
  step_start_ms_ = now_ms;
  last_home_send_ms_ = now_ms;
  if (s.is_servo) {
    servo_ramping_ = true;
    ramp_from_deg_ = servo_commanded_[s.servo_index] ? servo_deg_[s.servo_index] : SERVO_CENTER_DEG;
    last_ramp_send_ms_ = 0;
    return;
  }
  for (uint8_t id = JOINT_NODE_BASE; id <= JOINT_NODE_ELBOW; id++) {
    if (!(s.node_mask & (1u << id))) continue;
    node_started_[id] = false;
    if (id == JOINT_NODE_BASE) {
      if (!MotorController::instance().startHoming()) {
        failHoming("Base could not start homing");
        return;
      }
    } else {
      nodes_[id].homed = false; // forget the last run's flags
      nodes_[id].homing_failed = false;
      sendMotorFrame(id, 0.0f, MOTOR_FLAG_HOME);
    }
  }
}

void ArmState::failHoming(const char *reason) {
  Logging::errorf("ArmState: homing failed: %s", reason);
  stopAllMotors();
  servo_ramping_ = false;
  homing_status_ = HOMING_FAILED;
}

void ArmState::abortHoming() {
  if (homing_status_ != HOMING_RUNNING) return;
  stopAllMotors();
  servo_ramping_ = false;
  homing_status_ = HOMING_IDLE;
  step_ = 0;
  Logging::info("ArmState: homing aborted");
}

void ArmState::update(uint32_t now_ms) {
  // Keep the servos alive: the Elbow turns them off if it hears nothing for 100 ms.
  const bool any_servo = servo_commanded_[0] || servo_commanded_[1] || servo_commanded_[2];
  if (any_servo && !servo_ramping_ &&
      static_cast<uint32_t>(now_ms - last_servo_refresh_ms_) >= SERVO_REFRESH_MS) {
    for (uint8_t i = 0; i < 3; i++) {
      if (servo_commanded_[i]) sendServo(i, servo_deg_[i]);
    }
  }

  if (homing_status_ != HOMING_RUNNING) return;

  if (CanSafetyTask::instance().hasBusFault()) {
    failHoming("a bus fault was latched during homing");
    return;
  }

  const HomeStep &s = kSequence[step_];
  const uint32_t elapsed = static_cast<uint32_t>(now_ms - step_start_ms_);
  bool step_done = false;

  if (s.is_servo) {
    const float home = servoHomeDeg(s.servo_index);
    if (elapsed < SERVO_HOMING_RAMP_MS) {
      // Slow ramp, so the servo never slams into its end of travel.
      if (static_cast<uint32_t>(now_ms - last_ramp_send_ms_) >= SERVO_RAMP_STEP_MS) {
        last_ramp_send_ms_ = now_ms;
        const float frac = static_cast<float>(elapsed) / static_cast<float>(SERVO_HOMING_RAMP_MS);
        sendServo(s.servo_index, ramp_from_deg_ + (home - ramp_from_deg_) * frac);
      }
    } else {
      if (servo_ramping_) {
        sendServo(s.servo_index, home);
        servo_ramping_ = false;
      }
      step_done = elapsed >= SERVO_HOMING_RAMP_MS + SERVO_HOMING_SETTLE_MS;
    }
  } else {
    bool all_started = true;
    bool all_homed = true;
    for (uint8_t id = JOINT_NODE_BASE; id <= JOINT_NODE_ELBOW; id++) {
      if (!(s.node_mask & (1u << id))) continue;
      if (nodeHoming(id)) node_started_[id] = true;
      if (nodeHomingFailed(id) && (node_started_[id] || elapsed > STALE_FAILED_FLAG_MS)) {
        failHoming("a joint reported that it could not find its stop");
        return;
      }
      if (!node_started_[id]) all_started = false;
      if (!(node_started_[id] && nodeHomed(id))) all_homed = false;
    }
    // Resend HOME until every node in this step has said it started.
    if (!all_started && static_cast<uint32_t>(now_ms - last_home_send_ms_) >= HOME_RESEND_MS) {
      last_home_send_ms_ = now_ms;
      for (uint8_t id = JOINT_NODE_SHOULDER_L; id <= JOINT_NODE_ELBOW; id++) {
        if ((s.node_mask & (1u << id)) && !node_started_[id]) sendMotorFrame(id, 0.0f, MOTOR_FLAG_HOME);
      }
    }
    if (elapsed > HOMING_TIMEOUT_MS + HOMING_STEP_SLACK_MS) {
      failHoming("timed out waiting for a joint to finish homing");
      return;
    }
    step_done = all_homed;
  }

  if (!step_done) return;
  step_++;
  if (step_ >= static_cast<int>(sizeof(kSequence) / sizeof(kSequence[0]))) {
    homing_status_ = HOMING_DONE;
    // Hold every joint where homing left it.
    float rad[SIXEYES_JOINT_COUNT];
    measured(rad);
    for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) applied_[i] = rad[i];
    have_applied_ = true;
    Logging::info("ArmState: homing sequence finished");
    return;
  }
  beginStep(now_ms);
}
