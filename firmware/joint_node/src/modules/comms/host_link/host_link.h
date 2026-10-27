// The Base board's side of the host protocol (version 1): the rules in
// sixeyes-host/protocol/PROTOCOL.md, written so they do not depend on USB,
// Bluetooth or the Arduino framework. A transport feeds frames in with
// onFrame() and gives HostLink a sink to write answers to; today the only
// transport is USB (usb_bridge), BLE is planned.
//
// The rules (one source in control, no motion until homed, 200 ms watchdog,
// ack for every command, state stream) follow the host simulator
// (sixeyes-host/python/src/sixeyes_host/sim.py) line by line, so a host
// tested against the simulator sees the same behaviour here.
//
// What the arm underneath can really do is reported through `capabilities`;
// set only what the firmware does, and the host degrades gracefully.

#pragma once
#include "host_codec.h"
#include "sixeyes_protocol.h"
#include <cstddef>
#include <cstdint>

// What HostLink needs to know about, and tell, the arm. Implemented by
// arm/arm_state on the Base board.
class ArmInterface {
public:
  virtual ~ArmInterface() = default;

  // Where the homing sequence stands.
  enum HomingStatus : uint8_t { HOMING_IDLE = 0, HOMING_RUNNING = 1, HOMING_DONE = 2, HOMING_FAILED = 3 };

  // The six host joints (radians at the output shaft, gripper 0..100).
  virtual void measured(float out[SIXEYES_JOINT_COUNT]) = 0;

  // Fault bits owned by the arm side (SIXEYES_FAULT_*): hw_estop, stall,
  // bus_lost, homing_failed. HostLink adds watchdog and joint_limit itself.
  virtual uint16_t faultBits() = 0;

  // True while any joint is still moving to a target (blocks switching source).
  virtual bool moving() = 0;

  // New targets, already clamped to the joint limits.
  virtual void applyTargets(const float targets[SIXEYES_JOINT_COUNT]) = 0;

  // Hold every joint where it is now.
  virtual void holdPosition() = 0;

  // Clear latched faults. Returns false if one is still asserted (for example
  // a held hardware e-stop button).
  virtual bool clearFaults() = 0;

  // Start the homing sequence over all joints. Returns false, with the reason
  // logged, if it cannot start (a node missing, a fault latched, ...).
  virtual bool startHoming() = 0;
  virtual uint8_t homingStatus() = 0; // a HomingStatus
  // Stop homing where it is; the arm goes back to unhomed.
  virtual void abortHoming() = 0;
};

// Where motions saved on the arm are kept (storage/motion_slots). A sample is
// the time since the previous sample plus the six joint values; the first
// sample's time is ignored.
class SlotInterface {
public:
  virtual ~SlotInterface() = default;
  // Writes sample `index`. It must be 0 (starts the slot afresh) or the next
  // free index. Returns a SIXEYES_STATUS_*: bad_args for a wrong index, a bad
  // time or a full slot; faulted if the flash could not be written.
  virtual uint8_t write(uint8_t slot, uint16_t index, uint16_t dt_ms,
                        const float joints[SIXEYES_JOINT_COUNT]) = 0;
  virtual uint8_t erase(uint8_t slot) = 0;
  virtual uint16_t count(uint8_t slot) = 0; // samples stored, 0 = empty
  virtual bool read(uint8_t slot, uint16_t index, uint16_t &dt_ms, float joints[SIXEYES_JOINT_COUNT]) = 0;
  virtual void closeWrite() = 0; // finish any file being written
};

class HostLink {
public:
  // Writes one complete encoded frame (including its trailing 0x00).
  // Returns false if the frame had to be dropped.
  using Sink = bool (*)(const uint8_t *data, size_t len, void *ctx);

  HostLink(ArmInterface &arm, uint16_t capabilities);

  // `source` is SIXEYES_CONTROL_SOURCE_BLE or SIXEYES_CONTROL_SOURCE_USB.
  void setSink(uint8_t source, Sink sink, void *ctx);

  // Motion slots: stores and plays motions on the arm. Also switches the
  // capability on; leave unset and the slot messages are answered `unsupported`.
  void setSlots(SlotInterface *slots);

  // A valid frame arrived from `source`.
  void onFrame(uint8_t source, const HostCodec::Frame &frame, uint32_t now_ms);

  // Call often (every loop): runs the watchdog and the state stream.
  void tick(uint32_t now_ms);

  // The link to `source` dropped (a BLE phone went away, or went out of range).
  // If it was the source in control, the arm holds where it is (and a homing
  // run in progress is aborted): losing the phone must stop the arm.
  void onSourceDisconnected(uint8_t source);

  bool sourceConnected(uint8_t source) const { return source < 2 && said_hello_[source]; }
  uint8_t homingState() const { return homing_; }
  uint8_t controlSource() const { return control_; }
  uint16_t faults() const { return faults_; }

private:
  struct Port {
    Sink sink = nullptr;
    void *ctx = nullptr;
  };

  uint8_t handleHello(uint8_t source, const HostCodec::Frame &f, uint32_t now_ms);
  uint8_t handleSetControlSource(const HostCodec::Frame &f);
  uint8_t handleHome(uint8_t source);
  uint8_t handleTargets(uint8_t source, const HostCodec::Frame &f, uint32_t now_ms);
  uint8_t handleStop();
  uint8_t handleClearFault();
  uint8_t handleHeartbeat(uint8_t source, uint32_t now_ms);
  uint8_t handleSlotWrite(uint8_t source, const HostCodec::Frame &f);
  uint8_t handleSlotErase(uint8_t source, const HostCodec::Frame &f);
  uint8_t handleSlotPlay(uint8_t source, const HostCodec::Frame &f, uint32_t now_ms);

  // Playback of a stored motion (arm-driven: the host is not sending targets).
  void stepPlayback(uint32_t now_ms);
  void stopPlayback();
  void sendPose(const float pose[SIXEYES_JOINT_COUNT], uint32_t now_ms, bool force);

  // Checks shared by commands that need control. Returns a status, or
  // SIXEYES_STATUS_OK if the command may go ahead.
  uint8_t guard(uint8_t source) const;

  void feedWatchdog(uint32_t now_ms);
  void setFault(uint16_t bit, bool on);
  void send(uint8_t source, uint8_t type, const uint8_t *payload, size_t n);
  void sendAck(uint8_t source, uint8_t ref_type, uint8_t ref_seq, uint8_t status);
  void sendState(uint32_t now_ms);

  ArmInterface &arm_;
  uint16_t capabilities_;
  Port ports_[2];
  bool said_hello_[2] = {false, false};

  uint8_t control_;
  uint8_t homing_ = SIXEYES_HOMING_STATE_UNHOMED;
  uint16_t faults_ = 0;
  float target_[SIXEYES_JOINT_COUNT] = {};

  SlotInterface *slots_ = nullptr;

  struct Playback {
    bool active = false;
    bool approaching = false; // slowly moving to the first pose, then playing
    uint8_t slot = 0;
    uint16_t count = 0;
    uint16_t next = 0;        // index of the sample being moved towards
    float from[SIXEYES_JOINT_COUNT] = {};
    float to[SIXEYES_JOINT_COUNT] = {};
    uint32_t start_ms = 0;    // start of the present approach or segment
    uint32_t duration_ms = 1;
    uint32_t last_send_ms = 0;
  } pb_;

  bool watchdog_armed_ = false;
  uint32_t last_feed_ms_ = 0;

  uint8_t out_seq_ = 0;
  uint32_t next_state_ms_ = 0;
};
