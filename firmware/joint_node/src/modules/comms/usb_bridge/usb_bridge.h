// Base-only: the USB link to the laptop (or a bring-up tool).
//
// Speaks the SixEyes host protocol (version 1): binary frames, COBS-framed with a
// CRC-16, defined in the sixeyes-host repo (protocol/PROTOCOL.md). This replaces
// the old JSON/ASCII vocabulary that docs/reference/CAN_MESSAGE_PROTOCOL.md §1
// assumed. Base still TRANSLATES, not relays: it terminates the host protocol
// here and fans commands out to the joint nodes as CAN frames (see
// arm/arm_state.cpp); the CAN protocol itself is unchanged.
//
// This class is only the USB transport. The protocol rules live in
// comms/host_link/host_link.cpp so a BLE transport can reuse them.

#pragma once
#include "modules/comms/host_link/host_codec.h"
#include "modules/comms/host_link/host_link.h"
#include <cstdint>

class UsbBridge {
public:
  static UsbBridge &instance();

  void init();
  void update(); // Call from loop(), Base only.

  // True while a host (USB, or a phone over BLE) that said hello has sent
  // something in the last second. Base puts this in BUS_HEARTBEAT's ros2_alive field.
  bool hostAlive() const;

  // The protocol engine, shared with the BLE transport.
  HostLink &link() { return link_; }

private:
  UsbBridge();

  HostLink link_;
  HostCodec::FrameParser parser_;
  uint32_t last_rx_ms_ = 0;
  bool seen_frame_ = false;
};
