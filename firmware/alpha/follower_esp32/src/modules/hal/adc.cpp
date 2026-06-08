#include "adc.h"
#include <Arduino.h>

void HAL_ADC::init() {
  // Platform-specific ADC init if needed
}

int HAL_ADC::readRaw(uint8_t channel) {
  // stub: use analogRead on channel
  return analogRead(channel);
}
