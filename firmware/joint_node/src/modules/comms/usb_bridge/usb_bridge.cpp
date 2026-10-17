#include "usb_bridge.h"
#include "modules/util/logging.h"

UsbBridge &UsbBridge::instance() {
  static UsbBridge inst;
  return inst;
}

void UsbBridge::init() {
  Serial.begin(115200);
  Logging::warn("UsbBridge: init() is a stub -- no JSON<->CAN translation implemented yet");
}

void UsbBridge::update() {
  // TODO: not implemented. See header comment and docs/V1_TODO.md
  // "usb_bridge_node (ROS2): rework to a Base-only USB link".
}
