// Distributed safety/heartbeat for the v1 joint bus.
// Implements docs/protocols/CAN_MESSAGE_PROTOCOL.md §5 exactly:
//
//   1. Bus heartbeat timeout (every node): if BUS_HEARTBEAT isn't seen
//      within BUS_HEARTBEAT_TIMEOUT_MS, this node disables its own motors
//      and stops driving CAN -- independent of what other nodes do.
//   2. Node liveness timeout (Base only): if any other node's NODE_STATUS
//      goes silent for NODE_LIVENESS_TIMEOUT_MS, Base transmits E-STOP.
//
// E-STOP handling itself is NOT here -- it is wired directly into
// CanDriver::onReceive() from main.cpp so motor disable happens in the CAN
// RX callback path, not gated on this task's own poll rate. That is the
// decoupling described in the protocol doc §5 latency budget.
//
// This is a scaffold: state machine and public interface are final per the
// protocol doc, but the actual motor-disable call sites (motor_controller,
// servo_manager) are not yet wired in -- see TODOs in the .cpp.

#pragma once
#include <cstdint>

class CanSafetyTask {
public:
  static CanSafetyTask &instance();

  void init();

  // Call at CONTROL_LOOP_HZ (or faster -- this is deliberately independent
  // of the position-control tick, see board_config.h CONTROL_LOOP_HZ note).
  void update();

  // Called from CanDriver's RX callback when a BUS_HEARTBEAT frame arrives.
  void onBusHeartbeatReceived();

  // Base-only: called from CanDriver's RX callback when a NODE_STATUS frame
  // arrives from another node.
  void onNodeStatusReceived(uint8_t node_id);

  bool isSafeToOperate() const;

  // Directly disables this node's motors. Called both from update()'s
  // timeout logic and from the CAN RX E-STOP handler in main.cpp.
  void emergencyDisable();

private:
  CanSafetyTask() = default;

  unsigned long last_bus_heartbeat_ms_ = 0;
  unsigned long last_node_status_ms_[5] = {}; // indexed by node_id 1-4, Base-only use
  bool motors_enabled_ = false;
};
