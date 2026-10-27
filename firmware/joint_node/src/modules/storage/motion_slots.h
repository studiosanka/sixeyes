// Motions stored on the arm (Base only), in the flash filesystem.
//
// Four slots, one file each (/slot0 .. /slot3). A file is a 4-byte header
// ("SXS" and a version) followed by 26-byte samples: the time since the previous
// sample (u16, ms) and the six joint values (f32 each, as in the host protocol:
// radians at the output shaft, gripper 0..100). The number of samples is
// worked out from the file size, so a power cut in the middle of saving a
// motion leaves a shorter, still valid motion and not a broken file.
//
// Samples are appended as the host sends them (slot_write, index 0, 1, 2, ...).
// A file is closed after a second without writes, and before it is played.
//
// Not bench-tested: written against the arduino-esp32 LittleFS library and
// only compiled. The filesystem lives in the 1.4 MB "spiffs" partition of the
// default partition table; init() reports the real size.

#pragma once
#include "modules/comms/host_link/host_link.h"
#include <cstdint>

class MotionSlots : public SlotInterface {
public:
  static MotionSlots &instance();

  // Mounts the filesystem (formatting it the first time) and counts the saved
  // samples. Returns false if the flash could not be used.
  bool init();
  bool mounted() const { return mounted_; }

  // Call from loop(): closes a file that has been idle for a second.
  void update(uint32_t now_ms);

  // SlotInterface
  uint8_t write(uint8_t slot, uint16_t index, uint16_t dt_ms, const float joints[SIXEYES_JOINT_COUNT]) override;
  uint8_t erase(uint8_t slot) override;
  uint16_t count(uint8_t slot) override;
  bool read(uint8_t slot, uint16_t index, uint16_t &dt_ms, float joints[SIXEYES_JOINT_COUNT]) override;
  void closeWrite() override;

private:
  MotionSlots() = default;

  bool mounted_ = false;
  uint16_t counts_[SIXEYES_SLOT_COUNT] = {};
  int8_t writing_slot_ = -1; // slot whose file is open for writing, or -1
  uint32_t last_write_ms_ = 0;
  uint16_t unflushed_ = 0;
};
