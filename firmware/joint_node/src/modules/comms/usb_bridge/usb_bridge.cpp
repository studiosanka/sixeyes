#include "usb_bridge.h"
#include "modules/arm/arm_state.h"
#include "modules/comms/ble/ble_transport.h"
#include "modules/storage/motion_slots.h"
#include "modules/util/logging.h"
#include <Arduino.h>

// What this firmware really does, reported in hello_ack. Set a bit only when
// the feature works: hosts check these and degrade gracefully.
//   SIXEYES_CAP_HOMING        sensorless homing (work package 2): written, not run on hardware
//   SIXEYES_CAP_BLE           Bluetooth link to a phone (comms/ble), when built with -DSIXEYES_BLE
//   SIXEYES_CAP_MOTION_SLOTS  motions stored in flash (storage/motion_slots); HostLink switches it on
//                             only if the flash filesystem mounted
//   SIXEYES_CAP_ECHO_TARGETS  the state stream goes to every host that said hello, with the
//                             last accepted target, so a laptop listening on USB sees what a
//                             phone on BLE is commanding (Mode 2)
//   SIXEYES_CAP_HW_ESTOP      any board's BOOT button is a stop button (safety/stop_button.h)
static constexpr uint16_t HOST_CAPABILITIES =
    SIXEYES_CAP_HOMING | SIXEYES_CAP_ECHO_TARGETS | SIXEYES_CAP_HW_ESTOP
#ifdef SIXEYES_BLE
    | SIXEYES_CAP_BLE
#endif
    ;

// Most bytes read per loop(), so a flood of input cannot starve the control loop.
static constexpr int MAX_BYTES_PER_UPDATE = 256;

// A host counts as alive for this long after its last frame (BUS_HEARTBEAT's ros2_alive).
static constexpr uint32_t HOST_ALIVE_MS = 1000;

// Writes a frame to the USB serial port without ever blocking: if the host is
// not reading, or the buffer is full, the frame is dropped. A dropped state
// frame costs the host one sample; a stalled loop would cost the arm its timing.
static bool usbSink(const uint8_t *data, size_t len, void * /*ctx*/) {
  if (!Serial) return false;
  if (static_cast<size_t>(Serial.availableForWrite()) < len) return false;
  return Serial.write(data, len) == len;
}

UsbBridge &UsbBridge::instance() {
  static UsbBridge inst;
  return inst;
}

UsbBridge::UsbBridge() : link_(ArmState::instance(), HOST_CAPABILITIES) {}

void UsbBridge::init() {
  // Serial is already started in setup(); it is the board's native USB CDC port.
  Serial.setTxTimeoutMs(0); // never wait for a host that is not there
  link_.setSink(SIXEYES_CONTROL_SOURCE_USB, usbSink, nullptr);
  Logging::info("UsbBridge: host protocol v1 on USB");
  if (MotionSlots::instance().init()) link_.setSlots(&MotionSlots::instance());
#ifdef SIXEYES_BLE
  BleTransport::init(link_);
#endif
}

void UsbBridge::update() {
  const uint32_t now = millis();

  HostCodec::Frame frame;
  for (int i = 0; i < MAX_BYTES_PER_UPDATE && Serial.available() > 0; i++) {
    const int b = Serial.read();
    if (b < 0) break;
    if (parser_.feedByte(static_cast<uint8_t>(b), frame)) {
      if (!seen_frame_) {
        // From the first real frame on this port carries binary only.
        seen_frame_ = true;
        Logging::setQuiet(true);
      }
      last_rx_ms_ = now;
      link_.onFrame(SIXEYES_CONTROL_SOURCE_USB, frame, now);
    }
  }

#ifdef SIXEYES_BLE
  BleTransport::update(now);
#endif
  MotionSlots::instance().update(now);
  link_.tick(now);
}

bool UsbBridge::hostAlive() const {
  const bool usb = link_.sourceConnected(SIXEYES_CONTROL_SOURCE_USB) &&
                   static_cast<uint32_t>(millis() - last_rx_ms_) < HOST_ALIVE_MS;
#ifdef SIXEYES_BLE
  return usb || BleTransport::alive(millis());
#else
  return usb;
#endif
}
