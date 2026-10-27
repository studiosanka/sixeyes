// Base-only: the Bluetooth Low Energy link to a phone (Mode 1).
//
// It carries exactly the same frames as the USB link (comms/host_link), one
// frame per GATT value. This file is only the transport; the protocol rules,
// including control-source arbitration and the 200 ms watchdog, live in
// HostLink. Built only with -DSIXEYES_BLE (the Base environments).
//
// GATT layout (UUIDs are the contract with the phone page):
//   service  53495845-5945-5300-0001-000000000000
//   rx       53495845-5945-5300-0002-000000000000  phone -> arm, write / write without response
//   tx       53495845-5945-5300-0003-000000000000  arm -> phone, notify
// The arm advertises as "SixEyes-XXXX" (XXXX from its MAC address).
//
// Who may connect: one phone at a time, over an encrypted, bonded link.
//  - A phone that has paired before reconnects at any time.
//  - A NEW phone can pair only while the pairing window is open: for 60 s after
//    power-up, and for 60 s after the BOOT button on Base is held for 3 s. Outside
//    the window a new phone is disconnected at once.
//  - The arm remembers the last 3 phones; pairing a fourth replaces the oldest.
//
// Not bench-tested: written against NimBLE-Arduino 2.x and only compiled.

#pragma once
#include "modules/comms/host_link/host_link.h"
#include <cstdint>

#ifdef SIXEYES_BLE
namespace BleTransport {

constexpr uint32_t PAIRING_WINDOW_MS = 60000;
constexpr int MAX_BONDED_PHONES = 3;

void init(HostLink &link);
void update(uint32_t now_ms); // call every loop()

// Opens the pairing window for PAIRING_WINDOW_MS (called at power-up and by a
// 3 s hold of the BOOT button on Base).
void openPairing(uint32_t now_ms);
bool pairingOpen();

bool connected();
// A phone that said hello has sent something within the last second.
bool alive(uint32_t now_ms);

} // namespace BleTransport
#endif
