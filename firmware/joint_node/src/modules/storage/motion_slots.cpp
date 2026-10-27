#include "motion_slots.h"
#include "modules/config/joint_config.h"
#include "modules/util/logging.h"
#include <Arduino.h>
#include <LittleFS.h>

namespace {

constexpr size_t HEADER_BYTES = 4;
constexpr uint8_t HEADER[HEADER_BYTES] = {'S', 'X', 'S', 1};

#pragma pack(push, 1)
struct Record {
  uint16_t dt_ms;
  float joints[SIXEYES_JOINT_COUNT];
};
#pragma pack(pop)
constexpr size_t RECORD_BYTES = sizeof(Record); // 26

constexpr uint32_t IDLE_CLOSE_MS = 1000;
constexpr uint16_t FLUSH_EVERY = 16;
constexpr size_t MIN_FREE_BYTES = 8192; // keep some room so the filesystem never fills up completely

// The writer keeps its file open between samples, so a long motion is not
// reopened for each one. Only one file is open for writing at a time.
File writer;

void pathFor(uint8_t slot, char *out, size_t cap) { snprintf(out, cap, "/slot%u", static_cast<unsigned>(slot)); }

} // namespace

MotionSlots &MotionSlots::instance() {
  static MotionSlots inst;
  return inst;
}

bool MotionSlots::init() {
  if (!LittleFS.begin(true)) { // true: format on first use
    Logging::error("MotionSlots: the flash filesystem would not mount, motions cannot be saved");
    mounted_ = false;
    return false;
  }
  mounted_ = true;
  for (uint8_t slot = 0; slot < SIXEYES_SLOT_COUNT; slot++) {
    char path[12];
    pathFor(slot, path, sizeof(path));
    counts_[slot] = 0;
    if (!LittleFS.exists(path)) continue;
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    const size_t size = f.size();
    uint8_t head[HEADER_BYTES] = {};
    if (size >= HEADER_BYTES && f.read(head, HEADER_BYTES) == HEADER_BYTES && memcmp(head, HEADER, HEADER_BYTES) == 0) {
      const size_t n = (size - HEADER_BYTES) / RECORD_BYTES;
      counts_[slot] = n > SLOT_MAX_SAMPLES ? SLOT_MAX_SAMPLES : static_cast<uint16_t>(n);
    }
    f.close();
  }
  Logging::infof("MotionSlots: flash filesystem ready, %lu of %lu bytes used; samples per slot %u/%u/%u/%u",
                 static_cast<unsigned long>(LittleFS.usedBytes()), static_cast<unsigned long>(LittleFS.totalBytes()),
                 counts_[0], counts_[1], counts_[2], counts_[3]);
  return true;
}

void MotionSlots::closeWrite() {
  if (writing_slot_ < 0) return;
  writer.flush();
  writer.close();
  writing_slot_ = -1;
  unflushed_ = 0;
}

void MotionSlots::update(uint32_t now_ms) {
  if (writing_slot_ >= 0 && static_cast<uint32_t>(now_ms - last_write_ms_) >= IDLE_CLOSE_MS) closeWrite();
}

uint8_t MotionSlots::write(uint8_t slot, uint16_t index, uint16_t dt_ms, const float joints[SIXEYES_JOINT_COUNT]) {
  if (!mounted_) return SIXEYES_STATUS_FAULTED;
  if (slot >= SIXEYES_SLOT_COUNT || index >= SLOT_MAX_SAMPLES) return SIXEYES_STATUS_BAD_ARGS;
  if (index > 0 && dt_ms == 0) return SIXEYES_STATUS_BAD_ARGS; // samples must be spaced in time

  char path[12];
  pathFor(slot, path, sizeof(path));

  if (index == 0) {
    // A new motion: start the slot afresh.
    closeWrite();
    LittleFS.remove(path);
    writer = LittleFS.open(path, "w");
    if (!writer) return SIXEYES_STATUS_FAULTED;
    if (writer.write(HEADER, HEADER_BYTES) != HEADER_BYTES) {
      writer.close();
      return SIXEYES_STATUS_FAULTED;
    }
    writing_slot_ = static_cast<int8_t>(slot);
    counts_[slot] = 0;
  } else {
    if (index != counts_[slot]) return SIXEYES_STATUS_BAD_ARGS; // samples must arrive in order, none skipped
    if (writing_slot_ != static_cast<int8_t>(slot)) {
      closeWrite();
      writer = LittleFS.open(path, "a");
      if (!writer) return SIXEYES_STATUS_FAULTED;
      writing_slot_ = static_cast<int8_t>(slot);
    }
  }

  if (LittleFS.totalBytes() - LittleFS.usedBytes() < MIN_FREE_BYTES) {
    Logging::error("MotionSlots: flash is full");
    closeWrite();
    return SIXEYES_STATUS_FAULTED;
  }

  Record rec;
  rec.dt_ms = dt_ms;
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) rec.joints[i] = joints[i];
  if (writer.write(reinterpret_cast<const uint8_t *>(&rec), RECORD_BYTES) != RECORD_BYTES) {
    closeWrite();
    return SIXEYES_STATUS_FAULTED;
  }
  counts_[slot]++;
  last_write_ms_ = millis();
  if (++unflushed_ >= FLUSH_EVERY) {
    writer.flush();
    unflushed_ = 0;
  }
  return SIXEYES_STATUS_OK;
}

uint8_t MotionSlots::erase(uint8_t slot) {
  if (!mounted_) return SIXEYES_STATUS_FAULTED;
  if (slot >= SIXEYES_SLOT_COUNT) return SIXEYES_STATUS_BAD_ARGS;
  if (writing_slot_ == static_cast<int8_t>(slot)) closeWrite();
  char path[12];
  pathFor(slot, path, sizeof(path));
  if (LittleFS.exists(path) && !LittleFS.remove(path)) return SIXEYES_STATUS_FAULTED;
  counts_[slot] = 0;
  return SIXEYES_STATUS_OK;
}

uint16_t MotionSlots::count(uint8_t slot) {
  return (mounted_ && slot < SIXEYES_SLOT_COUNT) ? counts_[slot] : 0;
}

bool MotionSlots::read(uint8_t slot, uint16_t index, uint16_t &dt_ms, float joints[SIXEYES_JOINT_COUNT]) {
  if (!mounted_ || slot >= SIXEYES_SLOT_COUNT || index >= counts_[slot]) return false;
  if (writing_slot_ == static_cast<int8_t>(slot)) closeWrite(); // make what was written readable
  char path[12];
  pathFor(slot, path, sizeof(path));
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  Record rec;
  const bool ok = f.seek(HEADER_BYTES + static_cast<size_t>(index) * RECORD_BYTES) &&
                  f.read(reinterpret_cast<uint8_t *>(&rec), RECORD_BYTES) == RECORD_BYTES;
  f.close();
  if (!ok) return false;
  dt_ms = rec.dt_ms;
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) joints[i] = rec.joints[i];
  return true;
}
