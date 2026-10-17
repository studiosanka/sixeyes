#include "can_driver.h"
#include "modules/config/board_config.h"
#include "modules/util/logging.h"
#include <driver/twai.h>

CanDriver &CanDriver::instance() {
  static CanDriver inst;
  return inst;
}

bool CanDriver::init() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
      static_cast<gpio_num_t>(CAN_TX_PIN), static_cast<gpio_num_t>(CAN_RX_PIN),
      TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
  // Accept all standard IDs; protocol-level filtering happens in the RX
  // callback rather than hardware acceptance filters, since every node needs
  // E-STOP (0x000) plus its own addressed messages.
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    Logging::error("CanDriver: twai_driver_install failed");
    return false;
  }
  if (twai_start() != ESP_OK) {
    Logging::error("CanDriver: twai_start failed");
    return false;
  }
  initialized_ = true;
  return true;
}

bool CanDriver::send(uint32_t id, const uint8_t *data, uint8_t len) {
  if (!initialized_ || len > 8) return false;

  twai_message_t msg = {};
  msg.identifier = id;
  msg.data_length_code = len;
  msg.extd = 0; // 11-bit standard ID only, per protocol doc
  for (uint8_t i = 0; i < len; i++) msg.data[i] = data[i];

  // 0 ticks: non-blocking. Safety-critical callers (E-STOP) should not stall
  // waiting on a full TX queue; a dropped E-STOP frame is still safe because
  // every node also independently times out on BUS_HEARTBEAT loss.
  return twai_transmit(&msg, 0) == ESP_OK;
}

void CanDriver::onReceive(RxCallback callback) { rx_callback_ = callback; }

void CanDriver::pollReceive() {
  if (!initialized_ || !rx_callback_) return;

  twai_message_t msg;
  // 0 ticks: caller is expected to poll this frequently from its own task,
  // not block here waiting for a frame.
  while (twai_receive(&msg, 0) == ESP_OK) {
    rx_callback_(msg.identifier, msg.data, msg.data_length_code);
  }
}

bool CanDriver::isBusOff() const {
  twai_status_info_t status;
  if (twai_get_status_info(&status) != ESP_OK) return false;
  return status.state == TWAI_STATE_BUS_OFF;
}
