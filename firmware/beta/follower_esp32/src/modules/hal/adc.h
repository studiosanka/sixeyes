#pragma once
#include <cstdint>

class HAL_ADC {
public:
  static void init();
  static int readRaw(uint8_t channel);

private:
  HAL_ADC() = delete;
};
