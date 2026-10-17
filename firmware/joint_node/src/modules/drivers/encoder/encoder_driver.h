// AS5600 magnetic position encoder (I2C module) -- new in v1.
// Alpha/Beta had no shaft position feedback on the follower side; open-loop
// stepper drift was mitigated with StallGuard homing only (see
// OPEN_LOOP_STEPPER_STRATEGIES.md on the archive/dev-2026-10 branch). v1 adds this
// encoder per node, used both for runtime stall/fault detection
// (encoder-vs-command divergence, see docs/reference/CAN_MESSAGE_PROTOCOL.md
// §4.3a) and reported upstream via ENCODER_TELEMETRY.
//
// v1.2 uses an AS5600 module on I2C (fixed address 0x36, 12-bit, 4096 counts
// per turn) instead of the MT6835 / AS5048A SPI part planned in v1.1. See
// docs/reference/v1.2_PCB_Design_Reference.md §6.

#pragma once
#include <cstdint>

class EncoderDriver {
public:
  static constexpr int32_t COUNTS_PER_REV = 4096; // AS5600: 12-bit

  static EncoderDriver &instance();

  // Initializes I2C on board_config.h's ENCODER_I2C_* pins and checks that the
  // sensor answers and sees a magnet. Returns false (and latches the fault
  // flag) if it does not.
  bool init();

  // Continuous (multi-turn, unwrapped) position in encoder counts, relative to
  // the first successful read. The AS5600 itself is single-turn absolute;
  // unwrapping assumes less than half a turn of motion between calls (at
  // 250 Hz that is far beyond any joint speed). On a failed read this returns
  // the last good value and latches the fault flag.
  int32_t readPosition();

  // Counts -> degrees of whatever shaft the magnet is on. Whether that is the
  // motor shaft or the 1:25 gearbox output (and therefore any gear-ratio
  // factor) is a mechanical decision that is not made yet.
  static float countsToDegrees(int32_t counts);

  // True if the last I2C transaction failed or the magnet is not detected
  // (disconnected/miswired sensor, missing or misplaced magnet). A node should
  // treat this as a fault condition, not silently keep using stale position
  // data for divergence checks.
  bool hasFault() const;

private:
  EncoderDriver() = default;
  bool read(uint8_t &status, uint16_t &raw);

  bool fault_ = true; // faulted until a good read proves otherwise
  bool have_last_ = false;
  uint16_t last_raw_ = 0;
  int32_t position_ = 0;
};
