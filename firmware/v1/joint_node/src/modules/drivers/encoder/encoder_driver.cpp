#include "encoder_driver.h"
#include "modules/config/board_config.h"
#include "modules/util/logging.h"
#include <Arduino.h>
#include <Wire.h>

// AS5600 register map (datasheet): STATUS at 0x0B, RAW ANGLE at 0x0C/0x0D.
// The three registers are contiguous, so one 3-byte read gets both.
static constexpr uint8_t AS5600_ADDR       = 0x36;
static constexpr uint8_t AS5600_REG_STATUS = 0x0B;
static constexpr uint8_t AS5600_STATUS_MH  = 1 << 3; // magnet too strong
static constexpr uint8_t AS5600_STATUS_ML  = 1 << 4; // magnet too weak
static constexpr uint8_t AS5600_STATUS_MD  = 1 << 5; // magnet detected

static constexpr uint32_t I2C_CLOCK_HZ   = 400000;
static constexpr uint16_t I2C_TIMEOUT_MS = 5; // never block the control loop long

EncoderDriver &EncoderDriver::instance() {
  static EncoderDriver inst;
  return inst;
}

bool EncoderDriver::read(uint8_t &status, uint16_t &raw) {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(AS5600_REG_STATUS);
  if (Wire.endTransmission(false) != 0) return false; // repeated start

  if (Wire.requestFrom(static_cast<int>(AS5600_ADDR), 3) != 3) return false;
  status = Wire.read();
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  raw = (static_cast<uint16_t>(hi & 0x0F) << 8) | lo; // 12-bit
  return true;
}

bool EncoderDriver::init() {
  Wire.begin(ENCODER_I2C_SDA_PIN, ENCODER_I2C_SCL_PIN, I2C_CLOCK_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);

  uint8_t status = 0;
  uint16_t raw = 0;
  if (!read(status, raw)) {
    fault_ = true;
    Logging::error("EncoderDriver: AS5600 did not answer on I2C (0x36) -- "
                   "check module, wiring and the module's 3.3V jumper");
    return false;
  }
  if (!(status & AS5600_STATUS_MD)) {
    fault_ = true;
    Logging::error("EncoderDriver: AS5600 sees no magnet -- check magnet "
                   "presence, centring and air gap (0.5-3 mm)");
    return false;
  }
  if (status & AS5600_STATUS_MH) {
    Logging::warn("EncoderDriver: magnet too strong (MH) -- increase air gap");
  }
  if (status & AS5600_STATUS_ML) {
    Logging::warn("EncoderDriver: magnet too weak (ML) -- decrease air gap");
  }

  last_raw_ = raw;
  have_last_ = true;
  position_ = 0;
  fault_ = false;
  Logging::infof("EncoderDriver: AS5600 ready, raw=%u", raw);
  return true;
}

int32_t EncoderDriver::readPosition() {
  uint8_t status = 0;
  uint16_t raw = 0;
  if (!read(status, raw) || !(status & AS5600_STATUS_MD)) {
    fault_ = true; // callers must not trust the (stale) value
    return position_;
  }
  fault_ = false;

  if (!have_last_) {
    // First good read after a faulted init: establish the reference.
    last_raw_ = raw;
    have_last_ = true;
    position_ = 0;
    return position_;
  }

  // Unwrap the 0..4095 single-turn reading into a continuous count. Assumes
  // |true delta| < COUNTS_PER_REV / 2 between calls.
  int32_t delta = static_cast<int32_t>(raw) - static_cast<int32_t>(last_raw_);
  if (delta > COUNTS_PER_REV / 2) delta -= COUNTS_PER_REV;
  else if (delta < -COUNTS_PER_REV / 2) delta += COUNTS_PER_REV;
  position_ += delta;
  last_raw_ = raw;
  return position_;
}

float EncoderDriver::countsToDegrees(int32_t counts) {
  return static_cast<float>(counts) * 360.0f / static_cast<float>(COUNTS_PER_REV);
}

bool EncoderDriver::hasFault() const { return fault_; }
