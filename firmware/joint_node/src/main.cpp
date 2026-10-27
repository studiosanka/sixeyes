// SixEyes v1 joint node -- one firmware image, four board roles.
// Role selected at build time via JOINT_NODE_ID (see modules/config/node_config.h).
//
// This is a scaffold: module wiring and boot sequence are believed correct
// per docs/reference/CAN_MESSAGE_PROTOCOL.md, but several modules contain
// TODO stubs (homing sequence, step generation, BLE) -- see
// firmware/joint_node/docs/README.md for what's outstanding before this is
// bring-up ready. The Base board's USB link to the laptop speaks the SixEyes
// host protocol (comms/usb_bridge, comms/host_link).

#include "modules/comms/can/can_driver.h"
#include "modules/comms/can/can_protocol.h"
#include "modules/bench/bench_console.h"
#include "modules/config/board_config.h"
#include "modules/config/node_config.h"
#include "modules/drivers/encoder/encoder_driver.h"
#include "modules/motor_control/motor_controller.h"
#include "modules/safety/can_safety_task.h"
#include "modules/safety/estop_handler.h"
#include "modules/safety/stop_button.h"
#include "modules/telemetry/node_telemetry.h"
#include "modules/util/logging.h"
#include <Arduino.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if JOINT_NODE_HAS_SERVOS
#include "modules/servo_control/servo_manager.h"
#endif

#if JOINT_NODE_IS_BASE
#include "modules/arm/arm_state.h"
#include "modules/comms/host_link/host_codec.h"
#include "modules/comms/usb_bridge/usb_bridge.h"
#endif

// Dispatches every received CAN frame to the right handler. Registered once
// with CanDriver; runs from pollReceive()'s call site (see loop() below),
// NOT from an actual hardware ISR yet -- true ISR-driven dispatch (per the
// latency budget in the protocol doc §5) needs this moved into TWAI's
// interrupt-driven RX path or a high-priority FreeRTOS task pinned close to
// it. Scaffold only; see firmware/joint_node/docs/README.md.
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
    if (len >= sizeof(NodeStatusFrame)) {
      NodeStatusFrame frame;
      memcpy(&frame, data, sizeof(frame));
      ArmState::instance().onNodeStatus(node_id, frame);
    }
    return;
  }
  if (id >= CAN_ID_ENCODER_TELEM && id < CAN_ID_ENCODER_TELEM + 8) {
    uint8_t node_id = id - CAN_ID_ENCODER_TELEM;
    if (len >= sizeof(EncoderTelemetryFrame)) {
      EncoderTelemetryFrame frame;
      memcpy(&frame, data, sizeof(frame));
      ArmState::instance().onEncoderTelemetry(node_id, frame);
    }
    return;
  }
#else
  if (id == canIdFor(CAN_ID_MOTOR_TARGET, JOINT_NODE_ID)) {
    if (len >= sizeof(MotorTargetFrame)) {
      MotorTargetFrame frame;
      memcpy(&frame, data, sizeof(frame));
      if (frame.flags & MOTOR_FLAG_RESET_FAULT) {
        MotorController::instance().resetFault();
        CanSafetyTask::instance().clearBusFault(); // lets this node be enabled again after a stop
      }
      if (frame.flags & MOTOR_FLAG_ABORT) {
        MotorController::instance().holdHere(); // also aborts homing
      } else if (frame.flags & MOTOR_FLAG_HOME) {
        // Base repeats HOME until it sees us start. The seq names the homing
        // run, so a repeat of a request we already handled is ignored (it
        // would otherwise restart homing after it had finished).
        static bool have_home_seq = false;
        static uint16_t last_home_seq = 0;
        if (!have_home_seq || frame.seq != last_home_seq) {
          have_home_seq = true;
          last_home_seq = frame.seq;
          MotorController::instance().startHoming();
        }
      } else {
        MotorController::instance().setTargetPosition(frame.target_position);
      }
    }
    return;
  }
#if JOINT_NODE_HAS_SERVOS
  if (id == canIdFor(CAN_ID_SERVO_TARGET, JOINT_NODE_ID)) {
    if (len >= sizeof(ServoTargetFrame)) {
      ServoTargetFrame frame;
      memcpy(&frame, data, sizeof(frame));
      ServoManager::instance().setPosition(frame.servo_index, frame.target_angle);
      // The servos only drive their outputs once enabled, and only while the
      // bus heartbeat is alive and no bus fault is latched.
      if (!ServoManager::instance().enabled() && CanSafetyTask::instance().busHeartbeatAlive() &&
          !CanSafetyTask::instance().hasBusFault()) {
        ServoManager::instance().enable();
      }
    }
    return;
  }
#endif
#endif
}

void setup() {
  Serial.begin(115200);
  delay(100);
#if JOINT_NODE_IS_BASE && defined(HOST_LINK_SELFTEST)
  // Checks the host-protocol framing against the golden vectors from the
  // sixeyes-host repo and prints the result (build the v1_base_selftest env).
  HostCodec::selfTest();
#endif
  Logging::infof("SixEyes v1 joint node starting -- role: %s (id=%d)",
                  jointNodeName(), JOINT_NODE_ID);

  if (!CanDriver::instance().init()) {
    Logging::error("CanDriver init failed -- halting, cannot operate safely without CAN");
    while (true) delay(1000);
  }
  CanDriver::instance().onReceive(onCanFrame);

  CanSafetyTask::instance().init();
  StopButton::init();
  MotorController::instance().init();
  EncoderDriver::instance().init();

#if JOINT_NODE_HAS_SERVOS
  ServoManager::instance().init();
#endif

#if JOINT_NODE_IS_BASE
  UsbBridge::instance().init();
#endif

#ifdef BENCH_CONSOLE
  BenchConsole::init();
#endif

  Logging::info("v1 joint node init complete");
}

void loop() {
  const uint32_t now = millis();
  CanDriver::instance().pollReceive();
  StopButton::update(now);
  CanSafetyTask::instance().update();
  NodeTelemetry::instance().update(now);   // read the encoder once, share it
  MotorController::instance().update(now);
  NodeTelemetry::instance().publish(now);  // non-Base nodes: telemetry onto CAN

#if JOINT_NODE_HAS_SERVOS
  ServoManager::instance().checkWatchdog();
#endif

#if JOINT_NODE_IS_BASE
  UsbBridge::instance().update();
  ArmState::instance().update(now); // homing sequence, servo keep-alive

  // Base originates BUS_HEARTBEAT -- see protocol doc §4.2, target >= 50 Hz.
  static unsigned long last_heartbeat_ms = 0;
  static uint32_t heartbeat_seq = 0;
  if (now - last_heartbeat_ms >= 20) { // 50 Hz
    BusHeartbeatFrame hb{heartbeat_seq++, static_cast<uint8_t>(UsbBridge::instance().hostAlive() ? 1 : 0)};
    // Base never receives its own frame, so it feeds its own heartbeat watchdog
    // when the frame went out. Without it Base could never enable its motor.
    if (CanDriver::instance().send(CAN_ID_BUS_HEARTBEAT, hb)) {
      CanSafetyTask::instance().onBusHeartbeatReceived();
    }
    last_heartbeat_ms = now;
  }
#endif

#ifdef BENCH_CONSOLE
  BenchConsole::update();
#endif

  // Pace the loop at CONTROL_LOOP_HZ with a fixed period, not "work, then wait a
  // fixed time": the loop's own work (CAN, I2C, UART) no longer stretches the
  // period, so the motion profile gets a steady tick. The STEP pulses themselves
  // come from a hardware timer (step_generator), so they do not depend on this
  // loop's timing at all. Needs a FreeRTOS tick of 1 ms (the arduino-esp32 default).
  static TickType_t last_wake = xTaskGetTickCount();
  vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / CONTROL_LOOP_HZ));
}
