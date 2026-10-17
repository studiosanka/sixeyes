// Immediate E-STOP handling, decoupled from the control loop tick.
// Per docs/protocols/CAN_MESSAGE_PROTOCOL.md §5, E-STOP disable must not
// wait for the next control-loop iteration -- it is wired directly into
// CanDriver's RX callback from main.cpp, so this function runs as part of
// CAN frame reception, not the scheduled control task.

#pragma once
#include <cstdint>

namespace EstopHandler {

// Call from CanDriver::onReceive() for every frame with id == CAN_ID_ESTOP.
// Unconditionally disables this node's motors -- no acknowledgement, no
// conditional logic. Any node may have sent it; receipt alone is the action.
void onEstopFrameReceived(const uint8_t *data, uint8_t len);

} // namespace EstopHandler
