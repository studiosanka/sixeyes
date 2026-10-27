#include "stop_button.h"
#include "modules/config/node_config.h"
#include "modules/safety/can_safety_task.h"
#include <Arduino.h>

#if JOINT_NODE_IS_BASE && defined(SIXEYES_BLE)
#include "modules/comms/ble/ble_transport.h"
#endif

namespace StopButton {

namespace {

constexpr uint8_t BUTTON_PIN = 9; // GPIO9: the Super Mini's BOOT button, active low
constexpr uint32_t DEBOUNCE_MS = 30;

bool pressed_ = false;       // debounced state
bool raw_last_ = false;      // last raw reading
uint32_t raw_change_ms_ = 0; // when the raw reading last changed
uint32_t press_start_ms_ = 0;
bool pair_opened_ = false;   // already opened the pairing window for this press

} // namespace

void init() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

bool pressed() { return pressed_; }

void update(uint32_t now_ms) {
  const bool raw = digitalRead(BUTTON_PIN) == LOW;
  if (raw != raw_last_) {
    raw_last_ = raw;
    raw_change_ms_ = now_ms;
  }
  if (static_cast<uint32_t>(now_ms - raw_change_ms_) < DEBOUNCE_MS || raw == pressed_) {
    // Still bouncing, or no change: only the hold timer needs attention.
  } else {
    pressed_ = raw;
    if (pressed_) {
      press_start_ms_ = now_ms;
      pair_opened_ = false;
      CanSafetyTask::instance().manualStop(); // a press always stops the arm first
    }
  }

#if JOINT_NODE_IS_BASE && defined(SIXEYES_BLE)
  if (pressed_ && !pair_opened_ && static_cast<uint32_t>(now_ms - press_start_ms_) >= PAIR_HOLD_MS) {
    pair_opened_ = true;
    BleTransport::openPairing(now_ms);
  }
#endif
}

} // namespace StopButton
