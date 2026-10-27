// Joint model: how encoder counts, node targets and the host's six joint
// values relate to each other. Everything that depends on the mechanical
// design lives here so it can be corrected in one place.
//
// Units: host joints are radians at the OUTPUT shaft (gripper 0..100), see
// the host protocol. CAN MOTOR_TARGET carries degrees at the OUTPUT shaft.
// Each node converts to its own motor shaft.

#pragma once
#include "node_config.h"
#include <cstdint>

// --- Encoder placement (decided 2026-10-27) --------------------------------
// The AS5600 magnet is glued to the REAR SHAFT END of the NEMA17, so the
// encoder turns with the MOTOR shaft, not the 1:25 gearbox output. One output
// turn is JOINT_GEAR_RATIO motor turns, i.e. 25 * 4096 = 102,400 counts.
// Consequences: the reading is relative to the first read after boot (the turn
// count is lost at power-off, so every boot starts unhomed), and backlash in the
// gearbox is not visible to the encoder.
static constexpr float JOINT_GEAR_RATIO = 25.0f; // nominal; set per joint if the real ratio differs
static constexpr int32_t ENCODER_COUNTS_PER_MOTOR_REV = 4096; // AS5600, 12 bit

static constexpr float PI_F = 3.14159265358979f;
static constexpr float RAD_TO_DEG_F = 180.0f / PI_F;
static constexpr float DEG_TO_RAD_F = PI_F / 180.0f;

// Shoulder R is mounted mirrored to Shoulder L on the shared gearbox, so its
// motor and encoder turn the other way for the same output direction.
// ASSUMPTION: confirm on the real arm. One sign is applied both to targets
// going in and to encoder counts coming out, so the pair stays consistent.
constexpr float directionSignForNode(uint8_t node_id) {
  return node_id == JOINT_NODE_SHOULDER_R ? -1.0f : 1.0f;
}
static constexpr float JOINT_DIRECTION_SIGN = directionSignForNode(JOINT_NODE_ID);

// The firmware does NOT yet know which way the encoder counts when the driver
// steps "forward". It is assumed to count up. Homing checks this and fails with a
// clear message if the encoder moves the other way; if so, flip this sign.
static constexpr int ENCODER_SIGN_VS_STEPS = 1;

// Encoder counts (motor shaft) -> radians at the output shaft, for a node.
inline float motorCountsToOutputRad(int32_t counts, uint8_t node_id) {
  return directionSignForNode(node_id) * ENCODER_SIGN_VS_STEPS * static_cast<float>(counts) *
         (2.0f * PI_F / ENCODER_COUNTS_PER_MOTOR_REV) / JOINT_GEAR_RATIO;
}

// The reverse: output-shaft angle (radians) -> encoder counts on the motor shaft.
inline int32_t outputRadToMotorCounts(float rad, uint8_t node_id) {
  const float counts = rad * JOINT_GEAR_RATIO * (ENCODER_COUNTS_PER_MOTOR_REV / (2.0f * PI_F)) *
                       directionSignForNode(node_id) * ENCODER_SIGN_VS_STEPS;
  return static_cast<int32_t>(counts >= 0.0f ? counts + 0.5f : counts - 0.5f);
}

// Output-shaft degrees (what MOTOR_TARGET carries) -> motor-shaft degrees.
inline float outputDegToMotorDeg(float output_deg) {
  return output_deg * JOINT_GEAR_RATIO * JOINT_DIRECTION_SIGN;
}

// --- Host joint order (matches sixeyes_protocol.h SIXEYES_JOINT_*) -----------
// base = node 1, shoulder = nodes 2 + 3 (one joint, two motors), elbow = node 4.
// The last three are the open-loop MG996R servos on the Elbow board.

// ASSUMPTION (from FIRMWARE_HANDOFF.md section 1): which servo header drives
// which joint. Confirm against the mechanical design, then fix here only.
static constexpr uint8_t SERVO_INDEX_WRIST_PITCH = 0; // SERVO_PIN_0
static constexpr uint8_t SERVO_INDEX_WRIST_ROLL  = 1; // SERVO_PIN_1
static constexpr uint8_t SERVO_INDEX_GRIPPER     = 2; // SERVO_PIN_2

// PLACEHOLDERS, not calibrated. A servo has no feedback, so these decide where
// the joint really ends up. Never drive an MG996R past the joint's mechanical
// limit: calibrate on the real arm before any servo is allowed to move it.
static constexpr float SERVO_CENTER_DEG = 90.0f;           // wrist pitch/roll: 0 rad
static constexpr float GRIPPER_SERVO_DEG_AT_0   = 0.0f;    // gripper value 0
static constexpr float GRIPPER_SERVO_DEG_AT_100 = 90.0f;   // gripper value 100

inline float wristRadToServoDeg(float rad) { return SERVO_CENTER_DEG + rad * RAD_TO_DEG_F; }
inline float servoDegToWristRad(float deg) { return (deg - SERVO_CENTER_DEG) * DEG_TO_RAD_F; }
inline float gripperToServoDeg(float g) {
  return GRIPPER_SERVO_DEG_AT_0 + (g / 100.0f) * (GRIPPER_SERVO_DEG_AT_100 - GRIPPER_SERVO_DEG_AT_0);
}
inline float servoDegToGripper(float deg) {
  return (deg - GRIPPER_SERVO_DEG_AT_0) * 100.0f / (GRIPPER_SERVO_DEG_AT_100 - GRIPPER_SERVO_DEG_AT_0);
}

// PLACEHOLDER joint limits, the same numbers the host simulator uses. Replace
// with the real mechanical limits (radians at the output shaft; gripper 0..100).
static constexpr float JOINT_LIMIT_LOW[6]  = {-3.0f, -1.5f, -2.5f, -1.5f, -3.0f, 0.0f};
static constexpr float JOINT_LIMIT_HIGH[6] = { 3.0f,  1.5f,  2.5f,  1.5f,  3.0f, 100.0f};

// --- Which host joint each stepper node drives ---------------------------------

// Index into JOINT_LIMIT_* and the host's six values for a stepper node
// (base = 0, shoulder L and R = 1, elbow = 2).
constexpr int jointIndexForNode(uint8_t node_id) {
  return node_id == JOINT_NODE_BASE ? 0 : (node_id == JOINT_NODE_ELBOW ? 2 : 1);
}

// --- Motion ----------------------------------------------------------------------
// PLACEHOLDER caps for a first, gentle trapezoid move at the OUTPUT shaft, per
// stepper joint (base, shoulder, elbow). The shoulder carries the most load, so it
// is the slowest. The real caps come from the torque budget and the printed
// gearbox; tune on the bench.
static constexpr float MOTION_MAX_SPEED_OUTPUT_DEG_S_BY_JOINT[3]  = {30.0f, 20.0f, 30.0f};
static constexpr float MOTION_MAX_ACCEL_OUTPUT_DEG_S2_BY_JOINT[3] = {90.0f, 60.0f, 90.0f};
static constexpr float MOTION_MAX_SPEED_OUTPUT_DEG_S =
    MOTION_MAX_SPEED_OUTPUT_DEG_S_BY_JOINT[jointIndexForNode(JOINT_NODE_ID)];
static constexpr float MOTION_MAX_ACCEL_OUTPUT_DEG_S2 =
    MOTION_MAX_ACCEL_OUTPUT_DEG_S2_BY_JOINT[jointIndexForNode(JOINT_NODE_ID)];

// Settle trim (position control against the encoder): when the shaft has been
// at rest at its target for TRIM_SETTLE_MS and the AS5600 says it is further off
// than TRIM_DEADBAND_COUNTS (more than a full step of rotor lag, so a loaded
// rotor does not make it hunt), the node re-does the missing steps. At most
// TRIM_MAX_ATTEMPTS per rest; bigger errors than TRIM_MAX_COUNTS are a stall, not a trim.
static constexpr uint32_t TRIM_SETTLE_MS = 200;
static constexpr int32_t TRIM_DEADBAND_COUNTS = 24; // 24 counts = 2.1 deg at the motor, 0.08 deg at the output
static constexpr int32_t TRIM_MAX_COUNTS = 400;
static constexpr int TRIM_MAX_ATTEMPTS = 3;

// --- Homing (sensorless, steppers) -------------------------------------------------
// Each stepper drives slowly towards its hard stop until it stalls, backs off,
// and takes that stop as its reference. See motor_controller.cpp and
// FIRMWARE_HANDOFF.md section 5.
//
// !!! PLACEHOLDERS. Homing drives the arm into a mechanical stop. The stop angle
// and direction below are NOT the real arm's: they come from the placeholder
// joint limits. Do not run homing on a real arm until they are set from the CAD,
// and start with the gearbox disconnected from the arm structure. !!!

// Output-shaft direction towards the hard stop: -1 = towards the low limit.
static constexpr int HOMING_DIRECTION_OUTPUT = -1;
// Output-shaft angle (radians) of that hard stop, for this node's joint.
static constexpr float HOMING_STOP_ANGLE_RAD = JOINT_LIMIT_LOW[jointIndexForNode(JOINT_NODE_ID)];

static constexpr float HOMING_SPEED_OUTPUT_DEG_S  = 8.0f;   // slow, so a stall is clean and the hit is gentle
static constexpr float HOMING_ACCEL_OUTPUT_DEG_S2 = 30.0f;
static constexpr float HOMING_BACKOFF_OUTPUT_DEG  = 3.0f;   // away from the stop once it is found
static constexpr uint32_t HOMING_BLANKING_MS = 600;         // ignore stall signals while still accelerating
static constexpr uint32_t HOMING_TIMEOUT_MS  = 60000;       // longer than a full sweep at the homing speed

// Servos (Elbow board). A servo has no feedback, so "homing" drives each one
// slowly to a reference angle just inside its joint's mechanical limit. PLACEHOLDERS.
static constexpr uint32_t SERVO_HOMING_RAMP_MS = 1000; // about one second, so it never slams
static constexpr uint32_t SERVO_HOMING_SETTLE_MS = 300;

// Servo reference angle for homing: just inside the low limit of the joint the
// servo drives. PLACEHOLDER (from the placeholder joint limits), calibrate on the real arm.
inline float servoHomeDeg(uint8_t servo_index) {
  switch (servo_index) {
    case SERVO_INDEX_WRIST_PITCH: return wristRadToServoDeg(JOINT_LIMIT_LOW[3]);
    case SERVO_INDEX_WRIST_ROLL:  return wristRadToServoDeg(JOINT_LIMIT_LOW[4]);
    default:                      return gripperToServoDeg(JOINT_LIMIT_LOW[5]);
  }
}

// --- Motion slots (motions stored on the arm) ----------------------------------
// Most samples one slot can hold (6000 at 50 Hz is two minutes; four slots use
// about 620 KB of the 1.4 MB flash filesystem).
static constexpr uint16_t SLOT_MAX_SAMPLES = 6000;

// How fast a replay moves the arm from where it is to the motion's first pose,
// per joint (radians per second; gripper units per second). Slow on purpose:
// the arm may be anywhere. PLACEHOLDERS, tune on the real arm.
static constexpr float SLOT_APPROACH_SPEED[6] = {0.35f, 0.25f, 0.35f, 0.5f, 0.5f, 30.0f};
static constexpr uint32_t SLOT_APPROACH_MIN_MS = 1000;
