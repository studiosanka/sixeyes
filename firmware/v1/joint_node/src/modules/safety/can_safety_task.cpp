#include "can_safety_task.h"
#include "modules/comms/can/can_driver.h"
#include "modules/comms/can/can_protocol.h"
#include "modules/config/board_config.h"
#include "modules/config/node_config.h"
#include "modules/util/logging.h"
#include <Arduino.h>

CanSafetyTask &CanSafetyTask::instance() {
  static CanSafetyTask inst;
  return inst;
}

void CanSafetyTask::init() {
  // Boot state: motors disabled by default, identical to legacy -- a node
  // will not enable motors until it has seen at least one valid
  // BUS_HEARTBEAT (see protocol doc §5 "Boot state").
  motors_enabled_ = false;
  last_bus_heartbeat_ms_ = 0;
}

void CanSafetyTask::update() {
  unsigned long now = millis();

  // 1. Bus heartbeat timeout -- every node, including Base itself (Base
  // originates BUS_HEARTBEAT but still must notice if its own transmit path
  // stalls; treating it identically to every other node avoids a special
  // case that could mask a Base-side bug).
  if (last_bus_heartbeat_ms_ != 0 &&
      (now - last_bus_heartbeat_ms_) > BUS_HEARTBEAT_TIMEOUT_MS) {
    if (motors_enabled_) {
      Logging::error("CanSafetyTask: BUS_HEARTBEAT timeout, disabling motors");
      emergencyDisable();
    }
  }

#if JOINT_NODE_IS_BASE
  // 2. Node liveness timeout -- Base only. Any other node going silent
  // means Base fires a bus-wide E-STOP; it does not just disable its own
  // motors, since the other nodes are the ones actually at risk.
  for (uint8_t node_id = JOINT_NODE_SHOULDER_L; node_id <= JOINT_NODE_ELBOW; node_id++) {
    if (last_node_status_ms_[node_id] == 0) continue; // never heard from yet, not a fault
    if ((now - last_node_status_ms_[node_id]) > NODE_LIVENESS_TIMEOUT_MS) {
      Logging::warnf("CanSafetyTask: node %u status timeout, firing E-STOP", node_id);
      EstopFrame frame{static_cast<uint8_t>(EstopReason::HEARTBEAT_TIMEOUT)};
      CanDriver::instance().send(CAN_ID_ESTOP, frame);
      // Reset so we don't spam E-STOP every tick while the node stays dead --
      // one E-STOP is sufficient, every node latches motors-disabled on
      // receipt regardless of how many times it's sent.
      last_node_status_ms_[node_id] = now;
    }
  }
#endif
}

void CanSafetyTask::onBusHeartbeatReceived() {
  last_bus_heartbeat_ms_ = millis();
}

void CanSafetyTask::onNodeStatusReceived(uint8_t node_id) {
#if JOINT_NODE_IS_BASE
  if (node_id >= 1 && node_id <= 4) {
    last_node_status_ms_[node_id] = millis();
  }
#else
  (void)node_id; // non-Base nodes don't track other nodes' liveness
#endif
}

bool CanSafetyTask::isSafeToOperate() const { return motors_enabled_; }

void CanSafetyTask::emergencyDisable() {
  motors_enabled_ = false;
  // TODO: wire to MotorController::instance().disableAll() and (Elbow only)
  // ServoManager::instance().disableAll() once those modules exist -- see
  // docs/V1_TODO.md "Scaffold firmware/v1/joint_node/" checklist.
}
