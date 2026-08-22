// Base-only: laptop/ROS2-facing USB link, translates to CAN fan-out.
// Per docs/protocols/CAN_MESSAGE_PROTOCOL.md §1, Base is assumed to
// TRANSLATE rather than transparently relay: ROS2 keeps speaking the
// existing JSON/ASCII vocabulary over USB (reusing the legacy
// usb_bridge_node's message shapes), and this module fans commands out to
// the appropriate per-node CAN frame.
//
// Not implemented -- the ROS2-side usb_bridge_node also needs a
// corresponding rework (see docs/V1_TODO.md), and until that's designed in
// detail this is a placeholder that proves out compile-time role gating
// (only Base builds this in) rather than real framing logic.

#pragma once

class UsbBridge {
public:
  static UsbBridge &instance();

  void init();
  void update(); // Call from loop(), Base only.

private:
  UsbBridge() = default;
};
