#include "encoder_driver.h"
#include "modules/config/board_config.h"
#include "modules/util/logging.h"
#include <SPI.h>

// TODO: implementation blocked on final part selection (MT6835 vs AS5048A --
// see v1 hardware doc BOM entry UEC1). Both are SPI angle encoders but use
// different read command framing and resolution; do not guess at register
// layout here. Stub below only proves out pin wiring and SPI bus bring-up.

EncoderDriver &EncoderDriver::instance() {
  static EncoderDriver inst;
  return inst;
}

bool EncoderDriver::init() {
  pinMode(ENCODER_SPI_CS_PIN, OUTPUT);
  digitalWrite(ENCODER_SPI_CS_PIN, HIGH);
  SPI.begin(ENCODER_SPI_SCK_PIN, ENCODER_SPI_MISO_PIN, ENCODER_SPI_MOSI_PIN,
            ENCODER_SPI_CS_PIN);
  Logging::warn("EncoderDriver: init() is a stub pending part selection "
                "(MT6835 vs AS5048A) -- readPosition() is not implemented");
  return true;
}

int32_t EncoderDriver::readPosition() {
  fault_ = true; // Not implemented -- callers must not trust this value yet.
  return 0;
}

bool EncoderDriver::hasFault() const { return fault_; }
