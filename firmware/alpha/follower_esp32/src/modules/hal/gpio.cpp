#include "gpio.h"
#include "modules/drivers/tmc2209/tmc2209_config.h"
#include <Arduino.h>

void HAL_GPIO::init() {
  for (size_t i = 0; i < TMC2209_NUM_DRIVERS; ++i) {
    pinMode(TMC2209_EN_PINS[i], OUTPUT);
  }

  setMotorEnable(false);
  Serial.println("HAL_GPIO: initialized, all motor EN pins disabled");
}

void HAL_GPIO::setMotorEnable(bool enabled) {
  const uint8_t level =
      TMC2209_EN_ACTIVE_LOW ? (enabled ? LOW : HIGH) : (enabled ? HIGH : LOW);

  for (size_t i = 0; i < TMC2209_NUM_DRIVERS; ++i) {
    digitalWrite(TMC2209_EN_PINS[i], level);
  }
}

bool HAL_GPIO::readPin(uint8_t pin) { return digitalRead(pin) == HIGH; }
