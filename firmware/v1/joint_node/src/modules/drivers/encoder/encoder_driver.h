// SPI magnetic position encoder (MT6835 / AS5048A) -- new in v1.
// Alpha/Beta had no shaft position feedback on the follower side; open-loop
// stepper drift was mitigated with StallGuard homing only (see
// docs/firmware/legacy/OPEN_LOOP_STEPPER_STRATEGIES.md). v1 adds this
// encoder per node, used both for runtime stall/fault detection
// (encoder-vs-command divergence, see docs/protocols/CAN_MESSAGE_PROTOCOL.md
// §4.3a) and reported upstream via ENCODER_TELEMETRY.
//
// Exact part (MT6835 vs AS5048A) is not yet finalized -- see v1 hardware
// doc BOM entry UEC1. This interface is written against whichever part is
// chosen; only encoder_driver.cpp's register-level implementation differs
// between them.

#pragma once
#include <cstdint>

class EncoderDriver {
public:
  static EncoderDriver &instance();

  // Initializes SPI on board_config.h's ENCODER_SPI_* pins.
  bool init();

  // Raw position, encoder-native counts (not yet converted to degrees --
  // conversion factor depends on final part selection, TBD).
  int32_t readPosition();

  // True if the last SPI transaction failed (disconnected/miswired sensor).
  // A node should treat this as a fault condition, not silently keep using
  // stale position data for divergence checks.
  bool hasFault() const;

private:
  EncoderDriver() = default;
  bool fault_ = false;
};
