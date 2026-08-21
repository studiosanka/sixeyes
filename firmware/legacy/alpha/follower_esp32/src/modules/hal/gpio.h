#pragma once
#include <Arduino.h>

class HAL_GPIO {
public:
  static void init();
  static void setMotorEnable(bool enabled);
  static bool readPin(uint8_t pin);

private:
  HAL_GPIO() = delete;
};
