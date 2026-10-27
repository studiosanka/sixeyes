#include "host_link.h"
#include "modules/config/firmware_version.h"
#include "modules/config/joint_config.h"
#include <cmath>
#include <cstring>

namespace {

// Bits HostLink sets and clears itself; the arm reports all the others.
constexpr uint16_t kOwnFaults = SIXEYES_FAULT_WATCHDOG | SIXEYES_FAULT_JOINT_LIMIT;

// 50 Hz is the lowest rate the protocol allows. The stream is paced with an
// accumulating deadline so the average stays exactly on rate even though
// loop() only runs every few milliseconds.
constexpr uint32_t kStatePeriodMs = 20;

} // namespace

HostLink::HostLink(ArmInterface &arm, uint16_t capabilities)
    : arm_(arm),
      capabilities_(capabilities),
      // With BLE built in the phone is the default source (Mode 1); without
      // it USB is the only source there can be.
      control_((capabilities & SIXEYES_CAP_BLE) ? SIXEYES_CONTROL_SOURCE_BLE
                                                  : SIXEYES_CONTROL_SOURCE_USB) {}

void HostLink::setSlots(SlotInterface *slots) {
  slots_ = slots;
  if (slots_) {
    capabilities_ |= SIXEYES_CAP_MOTION_SLOTS;
  } else {
    capabilities_ &= static_cast<uint16_t>(~SIXEYES_CAP_MOTION_SLOTS);
  }
}

void HostLink::setSink(uint8_t source, Sink sink, void *ctx) {
  if (source >= 2) return;
  ports_[source].sink = sink;
  ports_[source].ctx = ctx;
}

// ---------------------------------------------------------------- sending

void HostLink::send(uint8_t source, uint8_t type, const uint8_t *payload, size_t n) {
  if (source >= 2 || !ports_[source].sink) return;
  uint8_t frame[HostCodec::kMaxPayload + 16];
  const size_t len = HostCodec::encodeFrame(type, ++out_seq_, payload, n, frame, sizeof(frame));
  if (len == 0) return;
  ports_[source].sink(frame, len, ports_[source].ctx);
}

void HostLink::sendAck(uint8_t source, uint8_t ref_type, uint8_t ref_seq, uint8_t status) {
  sixeyes_ack_t ack = {ref_type, ref_seq, status};
  uint8_t payload[SIXEYES_SIZE_ACK];
  sixeyes_ack_pack(&ack, payload);
  send(source, SIXEYES_MSG_ACK, payload, sizeof(payload));
}

void HostLink::sendState(uint32_t now_ms) {
  sixeyes_state_t st = {};
  st.t_ms = now_ms;
  st.homing_state = homing_;
  st.control_source = control_;
  st.faults = faults_;
  arm_.measured(st.measured);
  memcpy(st.target, target_, sizeof(target_));
  uint8_t payload[SIXEYES_SIZE_STATE];
  sixeyes_state_pack(&st, payload);
  // The stream goes to every transport whose host has said hello.
  for (uint8_t source = 0; source < 2; source++) {
    if (said_hello_[source]) send(source, SIXEYES_MSG_STATE, payload, sizeof(payload));
  }
}

// ---------------------------------------------------------------- commands

void HostLink::onFrame(uint8_t source, const HostCodec::Frame &f, uint32_t now_ms) {
  if (source >= 2) return;

  uint8_t status;
  switch (f.type) {
    case SIXEYES_MSG_HELLO:              status = handleHello(source, f, now_ms); break;
    case SIXEYES_MSG_SET_CONTROL_SOURCE: status = handleSetControlSource(f); break;
    case SIXEYES_MSG_HOME:               status = handleHome(source); break;
    case SIXEYES_MSG_TARGETS:            status = handleTargets(source, f, now_ms); break;
    case SIXEYES_MSG_STOP:               status = handleStop(); break;
    case SIXEYES_MSG_CLEAR_FAULT:        status = handleClearFault(); break;
    case SIXEYES_MSG_HEARTBEAT:          status = handleHeartbeat(source, now_ms); break;
    case SIXEYES_MSG_SLOT_WRITE:         status = handleSlotWrite(source, f); break;
    case SIXEYES_MSG_SLOT_ERASE:         status = handleSlotErase(source, f); break;
    case SIXEYES_MSG_SLOT_PLAY:          status = handleSlotPlay(source, f, now_ms); break;
    // Arm-to-host types are never valid from a host.
    default:                             status = SIXEYES_STATUS_UNSUPPORTED; break;
  }
  sendAck(source, f.type, f.seq, status);
}

uint8_t HostLink::handleHello(uint8_t source, const HostCodec::Frame &f, uint32_t now_ms) {
  sixeyes_hello_t hello;
  if (!sixeyes_hello_unpack(f.payload, f.length, &hello)) return SIXEYES_STATUS_BAD_ARGS;

  // A lone 0x00 first: text printed before the host spoke (boot messages)
  // has no delimiter, and this lets the host drop it instead of gluing it
  // to the front of hello_ack.
  if (source < 2 && ports_[source].sink) {
    const uint8_t zero = 0;
    ports_[source].sink(&zero, 1, ports_[source].ctx);
  }

  // hello_ack is always sent, even when the versions differ, so the host can
  // tell the user which side to update.
  sixeyes_hello_ack_t ack = {};
  ack.protocol_version = SIXEYES_PROTOCOL_VERSION;
  ack.fw_major = FIRMWARE_VERSION_MAJOR;
  ack.fw_minor = FIRMWARE_VERSION_MINOR;
  ack.fw_patch = FIRMWARE_VERSION_PATCH;
  ack.capabilities = capabilities_;
  uint8_t payload[SIXEYES_SIZE_HELLO_ACK];
  sixeyes_hello_ack_pack(&ack, payload);
  send(source, SIXEYES_MSG_HELLO_ACK, payload, sizeof(payload));

  if (hello.protocol_version != SIXEYES_PROTOCOL_VERSION) return SIXEYES_STATUS_BAD_VERSION;
  said_hello_[source] = true;
  next_state_ms_ = now_ms; // first state frame goes out on the next tick
  return SIXEYES_STATUS_OK;
}

uint8_t HostLink::handleSetControlSource(const HostCodec::Frame &f) {
  sixeyes_set_control_source_t cmd;
  if (!sixeyes_set_control_source_unpack(f.payload, f.length, &cmd)) return SIXEYES_STATUS_BAD_ARGS;
  if (cmd.source != SIXEYES_CONTROL_SOURCE_BLE && cmd.source != SIXEYES_CONTROL_SOURCE_USB) {
    return SIXEYES_STATUS_BAD_ARGS;
  }
  if (cmd.source == control_) return SIXEYES_STATUS_OK;
  // BLE is not in this firmware yet, so it cannot be put in control.
  if (cmd.source == SIXEYES_CONTROL_SOURCE_BLE && !(capabilities_ & SIXEYES_CAP_BLE)) {
    return SIXEYES_STATUS_UNSUPPORTED;
  }
  // Switching is explicit and never happens mid-motion.
  if (homing_ == SIXEYES_HOMING_STATE_HOMING || arm_.moving() || pb_.active) return SIXEYES_STATUS_BUSY;
  control_ = cmd.source;
  watchdog_armed_ = false; // the new source arms it with its first targets or heartbeat
  return SIXEYES_STATUS_OK;
}

uint8_t HostLink::guard(uint8_t source) const {
  if (source != control_) return SIXEYES_STATUS_NOT_IN_CONTROL;
  if (homing_ == SIXEYES_HOMING_STATE_FAULT) return SIXEYES_STATUS_FAULTED;
  return SIXEYES_STATUS_OK;
}

uint8_t HostLink::handleHome(uint8_t source) {
  const uint8_t bad = guard(source);
  if (bad != SIXEYES_STATUS_OK) return bad;
  if (homing_ == SIXEYES_HOMING_STATE_HOMING) return SIXEYES_STATUS_BUSY;
  if (!(capabilities_ & SIXEYES_CAP_HOMING)) return SIXEYES_STATUS_UNSUPPORTED;
  // Homing can be asked for again at any time (also when ready), for example
  // after the arm was moved by hand. The arm drops back to unhomed at once.
  if (!arm_.startHoming()) return SIXEYES_STATUS_FAULTED; // the reason is in the log
  homing_ = SIXEYES_HOMING_STATE_HOMING;
  watchdog_armed_ = false;
  arm_.measured(target_); // the echoed target is "where we are" until targets arrive
  return SIXEYES_STATUS_OK;
}

uint8_t HostLink::handleTargets(uint8_t source, const HostCodec::Frame &f, uint32_t now_ms) {
  const uint8_t bad = guard(source);
  if (bad != SIXEYES_STATUS_OK) return bad;
  if (homing_ != SIXEYES_HOMING_STATE_READY) return SIXEYES_STATUS_NOT_HOMED;
  if (pb_.active) return SIXEYES_STATUS_BUSY; // the arm is playing a stored motion: Stop it first

  sixeyes_targets_t cmd;
  if (!sixeyes_targets_unpack(f.payload, f.length, &cmd)) return SIXEYES_STATUS_BAD_ARGS;
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) {
    if (!std::isfinite(cmd.joints[i])) return SIXEYES_STATUS_BAD_ARGS;
  }

  float clamped[SIXEYES_JOINT_COUNT];
  bool any_clamped = false;
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) {
    float v = cmd.joints[i];
    if (v < JOINT_LIMIT_LOW[i]) v = JOINT_LIMIT_LOW[i];
    if (v > JOINT_LIMIT_HIGH[i]) v = JOINT_LIMIT_HIGH[i];
    any_clamped |= (v != cmd.joints[i]);
    clamped[i] = v;
  }
  memcpy(target_, clamped, sizeof(target_));
  setFault(SIXEYES_FAULT_JOINT_LIMIT, any_clamped);
  arm_.applyTargets(clamped);
  feedWatchdog(now_ms);
  return SIXEYES_STATUS_OK;
}

uint8_t HostLink::handleHeartbeat(uint8_t source, uint32_t now_ms) {
  if (source != control_) return SIXEYES_STATUS_NOT_IN_CONTROL;
  feedWatchdog(now_ms);
  return SIXEYES_STATUS_OK;
}

uint8_t HostLink::handleStop() {
  stopPlayback();
  // Software e-stop: accepted from either source, always. Hold where we are;
  // a stop during homing aborts it and the arm is unhomed again.
  if (homing_ == SIXEYES_HOMING_STATE_HOMING) {
    arm_.abortHoming();
    homing_ = SIXEYES_HOMING_STATE_UNHOMED;
  } else {
    arm_.holdPosition();
  }
  arm_.measured(target_);
  return SIXEYES_STATUS_OK;
}

uint8_t HostLink::handleClearFault() {
  stopPlayback();
  if (!arm_.clearFaults()) return SIXEYES_STATUS_FAULTED; // for example the e-stop button is still held
  faults_ = 0;
  homing_ = SIXEYES_HOMING_STATE_UNHOMED; // after any fault the arm must be homed again
  watchdog_armed_ = false;
  return SIXEYES_STATUS_OK;
}

void HostLink::onSourceDisconnected(uint8_t source) {
  if (source >= 2) return;
  said_hello_[source] = false;
  if (source != control_) return;
  stopPlayback(); // losing the phone stops a motion it started
  if (homing_ == SIXEYES_HOMING_STATE_HOMING) {
    arm_.abortHoming();
    homing_ = SIXEYES_HOMING_STATE_UNHOMED;
  } else {
    arm_.holdPosition();
  }
  arm_.measured(target_);
  if (homing_ == SIXEYES_HOMING_STATE_READY) setFault(SIXEYES_FAULT_WATCHDOG, true); // the source went silent
  watchdog_armed_ = false;
}

// ---------------------------------------------------------------- time

void HostLink::feedWatchdog(uint32_t now_ms) {
  watchdog_armed_ = true;
  last_feed_ms_ = now_ms;
  setFault(SIXEYES_FAULT_WATCHDOG, false);
}

void HostLink::setFault(uint16_t bit, bool on) {
  if (on) {
    faults_ |= bit;
  } else {
    faults_ &= static_cast<uint16_t>(~bit);
  }
}

void HostLink::tick(uint32_t now_ms) {
  // Faults the arm owns follow the arm; the two HostLink owns are kept.
  const uint16_t arm_faults = arm_.faultBits();
  faults_ = static_cast<uint16_t>((faults_ & kOwnFaults) | (arm_faults & ~kOwnFaults));

  // A hardware e-stop or a lost bus stops everything and needs clear_fault.
  if ((arm_faults & (SIXEYES_FAULT_HW_ESTOP | SIXEYES_FAULT_BUS_LOST)) &&
      homing_ != SIXEYES_HOMING_STATE_FAULT) {
    homing_ = SIXEYES_HOMING_STATE_FAULT;
    watchdog_armed_ = false;
    stopPlayback();
    arm_.holdPosition();
    arm_.measured(target_);
  }

  // Homing runs on the arm; follow its progress.
  if (homing_ == SIXEYES_HOMING_STATE_HOMING) {
    const uint8_t status = arm_.homingStatus();
    if (status == ArmInterface::HOMING_DONE) {
      homing_ = SIXEYES_HOMING_STATE_READY;
      arm_.measured(target_); // the arm holds where homing left it
      watchdog_armed_ = false; // armed by the first targets or heartbeat
    } else if (status == ArmInterface::HOMING_FAILED) {
      homing_ = SIXEYES_HOMING_STATE_FAULT; // the arm reports fault bit homing_failed
      watchdog_armed_ = false;
    }
  }

  // Watchdog: once the source in control has sent targets or a heartbeat, 200 ms
  // of silence makes the arm hold its position. It stops motion; it does not
  // raise the bus-wide E-STOP.
  // A stored motion plays on the arm, with no host sending targets.
  if (pb_.active) stepPlayback(now_ms);

  if (homing_ == SIXEYES_HOMING_STATE_READY && watchdog_armed_ && !pb_.active &&
      static_cast<uint32_t>(now_ms - last_feed_ms_) > SIXEYES_WATCHDOG_MS) {
    arm_.holdPosition();
    arm_.measured(target_);
    setFault(SIXEYES_FAULT_WATCHDOG, true);
    watchdog_armed_ = false;
  }

  // State stream to every host that said hello.
  bool any_host = said_hello_[0] || said_hello_[1];
  if (any_host && static_cast<int32_t>(now_ms - next_state_ms_) >= 0) {
    sendState(now_ms);
    next_state_ms_ += kStatePeriodMs;
    if (static_cast<int32_t>(now_ms - next_state_ms_) > static_cast<int32_t>(3 * kStatePeriodMs)) {
      next_state_ms_ = now_ms + kStatePeriodMs; // far behind (host was stalled): resync
    }
  }
}

// ---------------------------------------------------------------- motion slots

namespace {

void clampPose(float pose[SIXEYES_JOINT_COUNT]) {
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) {
    if (pose[i] < JOINT_LIMIT_LOW[i]) pose[i] = JOINT_LIMIT_LOW[i];
    if (pose[i] > JOINT_LIMIT_HIGH[i]) pose[i] = JOINT_LIMIT_HIGH[i];
  }
}

// Same 50 Hz as the host's own target stream: each pose is up to six CAN frames.
constexpr uint32_t kPoseSendPeriodMs = 20;

} // namespace

uint8_t HostLink::handleSlotWrite(uint8_t source, const HostCodec::Frame &f) {
  const uint8_t bad = guard(source);
  if (bad != SIXEYES_STATUS_OK) return bad;
  if (!slots_) return SIXEYES_STATUS_UNSUPPORTED;
  sixeyes_slot_write_t cmd;
  if (!sixeyes_slot_write_unpack(f.payload, f.length, &cmd)) return SIXEYES_STATUS_BAD_ARGS;
  if (cmd.slot >= SIXEYES_SLOT_COUNT) return SIXEYES_STATUS_BAD_ARGS;
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) {
    if (!std::isfinite(cmd.joints[i])) return SIXEYES_STATUS_BAD_ARGS;
  }
  if (pb_.active) return SIXEYES_STATUS_BUSY;
  return slots_->write(cmd.slot, cmd.index, cmd.dt_ms, cmd.joints);
}

uint8_t HostLink::handleSlotErase(uint8_t source, const HostCodec::Frame &f) {
  const uint8_t bad = guard(source);
  if (bad != SIXEYES_STATUS_OK) return bad;
  if (!slots_) return SIXEYES_STATUS_UNSUPPORTED;
  sixeyes_slot_erase_t cmd;
  if (!sixeyes_slot_erase_unpack(f.payload, f.length, &cmd)) return SIXEYES_STATUS_BAD_ARGS;
  if (cmd.slot >= SIXEYES_SLOT_COUNT) return SIXEYES_STATUS_BAD_ARGS;
  if (pb_.active) return SIXEYES_STATUS_BUSY;
  return slots_->erase(cmd.slot);
}

uint8_t HostLink::handleSlotPlay(uint8_t source, const HostCodec::Frame &f, uint32_t now_ms) {
  const uint8_t bad = guard(source);
  if (bad != SIXEYES_STATUS_OK) return bad;
  if (!slots_) return SIXEYES_STATUS_UNSUPPORTED;
  sixeyes_slot_play_t cmd;
  if (!sixeyes_slot_play_unpack(f.payload, f.length, &cmd)) return SIXEYES_STATUS_BAD_ARGS;
  if (cmd.slot >= SIXEYES_SLOT_COUNT) return SIXEYES_STATUS_BAD_ARGS;
  if (homing_ != SIXEYES_HOMING_STATE_READY) return SIXEYES_STATUS_NOT_HOMED;
  if (pb_.active) return SIXEYES_STATUS_BUSY;

  slots_->closeWrite();
  const uint16_t count = slots_->count(cmd.slot);
  if (count < 2) return SIXEYES_STATUS_BAD_ARGS; // empty, or too short to be a motion

  uint16_t dt = 0;
  float first[SIXEYES_JOINT_COUNT];
  if (!slots_->read(cmd.slot, 0, dt, first)) return SIXEYES_STATUS_FAULTED;
  clampPose(first);

  // Approach: a slow move from where the arm is to the first pose of the
  // motion. It takes as long as the slowest joint needs at SLOT_APPROACH_SPEED.
  pb_ = Playback();
  arm_.measured(pb_.from);
  float longest_s = 0.0f;
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) {
    pb_.to[i] = first[i];
    const float s = fabsf(first[i] - pb_.from[i]) / SLOT_APPROACH_SPEED[i];
    if (s > longest_s) longest_s = s;
  }
  uint32_t duration = static_cast<uint32_t>(longest_s * 1000.0f) + 1;
  if (duration < SLOT_APPROACH_MIN_MS) duration = SLOT_APPROACH_MIN_MS;
  pb_.active = true;
  pb_.approaching = true;
  pb_.slot = cmd.slot;
  pb_.count = count;
  pb_.next = 0;
  pb_.start_ms = now_ms;
  pb_.duration_ms = duration;
  pb_.last_send_ms = now_ms - kPoseSendPeriodMs;
  watchdog_armed_ = false; // the arm drives itself now, no host targets are expected
  return SIXEYES_STATUS_OK;
}

void HostLink::sendPose(const float pose[SIXEYES_JOINT_COUNT], uint32_t now_ms, bool force) {
  if (!force && static_cast<uint32_t>(now_ms - pb_.last_send_ms) < kPoseSendPeriodMs) return;
  pb_.last_send_ms = now_ms;
  float p[SIXEYES_JOINT_COUNT];
  memcpy(p, pose, sizeof(p));
  clampPose(p);
  memcpy(target_, p, sizeof(target_)); // the echoed target follows the motion
  arm_.applyTargets(p);
}

void HostLink::stopPlayback() {
  if (!pb_.active) return;
  pb_.active = false;
  arm_.holdPosition();
  arm_.measured(target_);
}

void HostLink::stepPlayback(uint32_t now_ms) {
  float pose[SIXEYES_JOINT_COUNT];

  if (pb_.approaching) {
    const uint32_t elapsed = static_cast<uint32_t>(now_ms - pb_.start_ms);
    const float frac = elapsed >= pb_.duration_ms ? 1.0f
                                                  : static_cast<float>(elapsed) / static_cast<float>(pb_.duration_ms);
    for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) pose[i] = pb_.from[i] + (pb_.to[i] - pb_.from[i]) * frac;
    sendPose(pose, now_ms, frac >= 1.0f);
    if (frac < 1.0f) return;
    // At the first pose: start the motion; the second sample is next.
    memcpy(pb_.from, pb_.to, sizeof(pb_.from));
    pb_.approaching = false;
    pb_.next = 1;
    pb_.start_ms = now_ms;
    uint16_t dt = 0;
    if (!slots_->read(pb_.slot, pb_.next, dt, pb_.to)) {
      stopPlayback();
      return;
    }
    pb_.duration_ms = dt ? dt : 1;
    return;
  }

  // Playing: move on through the samples that are already due (a stalled loop
  // may be several behind), then place the arm between the last two.
  for (int guard_n = 0; guard_n < 16; guard_n++) {
    const uint32_t elapsed = static_cast<uint32_t>(now_ms - pb_.start_ms);
    if (elapsed < pb_.duration_ms) break;
    memcpy(pb_.from, pb_.to, sizeof(pb_.from));
    pb_.start_ms += pb_.duration_ms; // keep the recorded timing, do not drift
    pb_.next++;
    if (pb_.next >= pb_.count) { // finished: end exactly on the last pose
      sendPose(pb_.from, now_ms, true);
      pb_.active = false;
      return;
    }
    uint16_t dt = 0;
    if (!slots_->read(pb_.slot, pb_.next, dt, pb_.to)) {
      stopPlayback();
      return;
    }
    pb_.duration_ms = dt ? dt : 1;
  }
  const uint32_t elapsed = static_cast<uint32_t>(now_ms - pb_.start_ms);
  const float frac = static_cast<float>(elapsed) / static_cast<float>(pb_.duration_ms);
  for (int i = 0; i < SIXEYES_JOINT_COUNT; i++) pose[i] = pb_.from[i] + (pb_.to[i] - pb_.from[i]) * frac;
  sendPose(pose, now_ms, false);
}
