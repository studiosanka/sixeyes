#include "estop_handler.h"
#include "can_safety_task.h"
#include "modules/comms/can/can_protocol.h"
#include "modules/util/logging.h"

namespace EstopHandler {

void onEstopFrameReceived(const uint8_t *data, uint8_t len) {
  if (len < sizeof(EstopFrame)) {
    // Malformed frame on a safety-critical ID -- still disable. A truncated
    // E-STOP is not a reason to assume safety; fail toward disabled.
    Logging::error("EstopHandler: malformed E-STOP frame, disabling anyway");
    CanSafetyTask::instance().emergencyDisable();
    return;
  }
  const auto *frame = reinterpret_cast<const EstopFrame *>(data);
  Logging::warnf("EstopHandler: E-STOP received, reason=%u", frame->reason);
  CanSafetyTask::instance().emergencyDisable();
}

} // namespace EstopHandler
