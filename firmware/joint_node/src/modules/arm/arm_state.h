// Base only: the whole arm as the host sees it.
//
// The host talks in six joints (base, shoulder, elbow, wrist pitch, wrist roll,
// gripper); the arm is four CAN nodes (shoulder = two of them) plus three
// open-loop servos on the Elbow board. This class turns the CAN telemetry into
// the six measured values, the host's six targets into per-node CAN frames,
// and runs the homing sequence across all of them.
// See modules/config/joint_config.h for the mapping and the unit conversions.

#pragma once
#include "modules/comms/can/can_protocol.h"
#include "modules/comms/host_link/host_link.h"

class ArmState : public ArmInterface {
public:
  static ArmState &instance();

  // CAN receive hooks, called from main.cpp for frames from the other nodes.
  void onEncoderTelemetry(uint8_t node_id, const EncoderTelemetryFrame &frame);
  void onNodeStatus(uint8_t node_id, const NodeStatusFrame &frame);

  // Call every loop: runs the homing sequence and keeps the servos fed.
  void update(uint32_t now_ms);

  // ArmInterface
  void measured(float out[SIXEYES_JOINT_COUNT]) override;
  uint16_t faultBits() override;
  bool moving() override;
  void applyTargets(const float targets[SIXEYES_JOINT_COUNT]) override;
  void holdPosition() override;
  bool clearFaults() override;
  bool startHoming() override;
  uint8_t homingStatus() override { return homing_status_; }
  void abortHoming() override;

private:
  ArmState() = default;

  struct Node {
    int32_t counts = 0;   // motor-shaft encoder counts (true joint angle once homed)
    bool seen = false;    // telemetry heard since boot
    bool status_seen = false;
    bool stall = false;
    bool encoder_fault = false;
    bool homed = false;
    bool homing = false;
    bool homing_failed = false;
  };

  // The homing order, so the arm folds up instead of swinging out:
  // gripper, wrist roll, wrist pitch, elbow, shoulder, base.
  struct HomeStep {
    bool is_servo;
    uint8_t servo_index; // for servo steps
    uint8_t node_mask;   // for stepper steps: bit n set = node n
  };
  static const HomeStep kSequence[6];

  bool nodeRad(uint8_t node_id, float &rad) const;
  bool nodeHomed(uint8_t node_id) const;
  bool nodeHoming(uint8_t node_id) const;
  bool nodeHomingFailed(uint8_t node_id) const;
  void sendMotorFrame(uint8_t node_id, float output_deg, uint8_t flags);
  void sendServo(uint8_t servo_index, float deg);
  void beginStep(uint32_t now_ms);
  void failHoming(const char *reason);
  void stopAllMotors();

  Node nodes_[5]; // indexed by node id 1..4; index 0 unused (node 1 is read locally)

  // What the servos were last told. They have no feedback, so this is also
  // what the state stream reports for them. The Elbow disables its servos if
  // it hears nothing for 100 ms, so update() re-sends them every 50 ms.
  float servo_deg_[3] = {};
  bool servo_commanded_[3] = {};
  uint32_t last_servo_refresh_ms_ = 0;

  // The last targets the host sent (radians; gripper 0..100), for moving().
  float applied_[SIXEYES_JOINT_COUNT] = {};
  bool have_applied_ = false;

  // Homing sequence.
  uint8_t homing_status_ = HOMING_IDLE;
  int step_ = 0;
  uint32_t step_start_ms_ = 0;
  uint32_t last_home_send_ms_ = 0;
  float ramp_from_deg_ = 0.0f;
  uint16_t home_session_ = 0; // changes every homing run; the nodes ignore a repeated HOME with the same seq
  bool node_started_[5] = {};
  bool servo_ramping_ = false;
  uint32_t last_ramp_send_ms_ = 0;

  uint16_t motor_seq_ = 0;
  uint16_t servo_seq_ = 0;
};
