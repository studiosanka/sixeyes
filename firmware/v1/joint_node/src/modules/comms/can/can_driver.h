// TWAI (CAN) transport wrapper for the v1 joint bus.
// Thin layer over ESP-IDF's driver/twai.h — owns bus init, frame send, and
// RX dispatch. Protocol semantics (message meaning, safety model) live in
// can_safety_task.h / motor_control, not here.

#pragma once
#include <cstdint>
#include <functional>

class CanDriver {
public:
  static CanDriver &instance();

  // Configures TWAI at 1 Mbps on board_config.h's CAN_TX_PIN/CAN_RX_PIN.
  // Call once from setup(). Returns false if the peripheral failed to start.
  bool init();

  // Sends a standard-ID frame. Returns false if the TX queue is full or the
  // controller is in bus-off (see can_safety_task.h for bus-off handling —
  // this call does not attempt recovery).
  bool send(uint32_t id, const uint8_t *data, uint8_t len);

  template <typename T>
  bool send(uint32_t id, const T &payload) {
    static_assert(sizeof(T) <= 8, "CAN 2.0A payload must be <= 8 bytes");
    return send(id, reinterpret_cast<const uint8_t *>(&payload), sizeof(T));
  }

  // Registers a callback invoked from the RX task for every received frame.
  // The E-STOP handler (can_safety_task) registers here and must do its
  // motor-disable work directly in this callback path, not by deferring to
  // the control loop tick -- see docs/protocols/CAN_MESSAGE_PROTOCOL.md §5.
  using RxCallback = std::function<void(uint32_t id, const uint8_t *data, uint8_t len)>;
  void onReceive(RxCallback callback);

  // Must be called frequently (e.g. from a dedicated FreeRTOS task, not the
  // control loop) to pump received frames to the registered callback.
  void pollReceive();

  // True if the TWAI controller has entered bus-off. Per the resolved
  // decision in CAN_MESSAGE_PROTOCOL.md §6.4, recovery is manual only --
  // this driver does not attempt twai_initiate_recovery() automatically.
  bool isBusOff() const;

private:
  CanDriver() = default;
  RxCallback rx_callback_;
  bool initialized_ = false;
};
