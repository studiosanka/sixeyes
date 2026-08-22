// SixEyes v1 joint node -- one firmware image, four board roles.
// Role selected at build time via JOINT_NODE_ID (see modules/config/node_config.h).
//
// This is a scaffold: module wiring and boot sequence are believed correct
// per docs/protocols/CAN_MESSAGE_PROTOCOL.md, but several modules contain
// TODO stubs (encoder register-level driver, USB<->CAN translation, homing
// sequence, step generation) -- see docs/V1_TODO.md for what's outstanding
// before this is bring-up ready.

#include "modules/comms/can/can_driver.h"
#include "modules/comms/can/can_protocol.h"
#include "modules/config/board_config.h"
#include "modules/config/node_config.h"
#include "modules/drivers/encoder/encoder_driver.h"
#include "modules/motor_control/motor_controller.h"
#include "modules/safety/can_safety_task.h"
#include "modules/safety/estop_handler.h"
#include "modules/util/logging.h"
#include <Arduino.h>

#if JOINT_NODE_HAS_SERVOS
#include "modules/servo_control/servo_manager.h"
#endif

#if JOINT_NODE_IS_BASE
#include "modules/comms/usb_bridge/usb_bridge.h"
#endif

// Dispatches every received CAN frame to the right handler. Registered once
// with CanDriver; runs from pollReceive()'s call site (see loop() below),
// NOT from an actual hardware ISR yet -- true ISR-driven dispatch (per the
// latency budget in the protocol doc §5) needs this moved into TWAI's
// interrupt-driven RX path or a high-priority FreeRTOS task pinned close to
// it. Scaffold only; see docs/V1_TODO.md.
static void onCanFrame(uint32_t id, const uint8_t *data, uint8_t len) {
  if (id == CAN_ID_ESTOP) {
    EstopHandler::onEstopFrameReceived(data, len);
    return;
  }
  if (id == CAN_ID_BUS_HEARTBEAT) {
    CanSafetyTask::instance().onBusHeartbeatReceived();
    return;
  }
#if JOINT_NODE_IS_BASE
  if (id >= CAN_ID_NODE_STATUS && id < CAN_ID_NODE_STATUS + 8) {
    uint8_t node_id = id - CAN_ID_NODE_STATUS;
    CanSafetyTask::instance().onNodeStatusReceived(node_id);
    return;
  }
#else
  if (id == canIdFor(CAN_ID_MOTOR_TARGET, JOINT_NODE_ID)) {
    if (len >= sizeof(MotorTargetFrame)) {
      const auto *frame = reinterpret_cast<const MotorTargetFrame *>(data);
      MotorController::instance().setTargetPosition(frame->target_position);
    }
    return;
  }
#if JOINT_NODE_HAS_SERVOS
  if (id == canIdFor(CAN_ID_SERVO_TARGET, JOINT_NODE_ID)) {
    if (len >= sizeof(ServoTargetFrame)) {
      const auto *frame = reinterpret_cast<const ServoTargetFrame *>(data);
      ServoManager::instance().setPosition(frame->servo_index, frame->target_angle);
    }
    return;
  }
#endif
#endif
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Logging::infof("SixEyes v1 joint node starting -- role: %s (id=%d)",
                  jointNodeName(), JOINT_NODE_ID);

  if (!CanDriver::instance().init()) {
    Logging::error("CanDriver init failed -- halting, cannot operate safely without CAN");
    while (true) delay(1000);
  }
  CanDriver::instance().onReceive(onCanFrame);

  CanSafetyTask::instance().init();
  MotorController::instance().init();
  EncoderDriver::instance().init();

#if JOINT_NODE_HAS_SERVOS
  ServoManager::instance().init();
#endif

#if JOINT_NODE_IS_BASE
  UsbBridge::instance().init();
#endif

  Logging::info("v1 joint node init complete");
}

void loop() {
  CanDriver::instance().pollReceive();
  CanSafetyTask::instance().update();
  MotorController::instance().update();

#if JOINT_NODE_HAS_SERVOS
  ServoManager::instance().checkWatchdog();
#endif

#if JOINT_NODE_IS_BASE
  UsbBridge::instance().update();

  // Base originates BUS_HEARTBEAT -- see protocol doc §4.2, target >= 50 Hz.
  static unsigned long last_heartbeat_ms = 0;
  static uint32_t heartbeat_seq = 0;
  unsigned long now = millis();
  if (now - last_heartbeat_ms >= 20) { // 50 Hz
    BusHeartbeatFrame hb{heartbeat_seq++, 1 /* TODO: real ros2_alive from UsbBridge */};
    CanDriver::instance().send(CAN_ID_BUS_HEARTBEAT, hb);
    last_heartbeat_ms = now;
  }
#endif

  // TODO: control loop should run at CONTROL_LOOP_HZ on a dedicated
  // FreeRTOS task/timer, not a busy Arduino loop() -- legacy used
  // MotorControlScheduler for this (see firmware/legacy/beta's equivalent
  // module). Not yet ported; loop() here is a placeholder pace, not the
  // real 250 Hz timing guarantee described in board_config.h.
  delay(1000 / CONTROL_LOOP_HZ);
}
