/* GENERATED from protocol/messages.yaml by protocol/generate.py. Do not edit by hand. */
#ifndef SIXEYES_PROTOCOL_H
#define SIXEYES_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Little-endian host assumed (ESP32, x86, ARM). Floats are IEEE-754 binary32. */

#define SIXEYES_PROTOCOL_VERSION 1
#define SIXEYES_JOINT_COUNT 6
#define SIXEYES_WATCHDOG_MS 200
#define SIXEYES_STATE_RATE_HZ_MIN 50
#define SIXEYES_STATE_RATE_HZ_MAX 100
#define SIXEYES_GRIPPER_MAX 100
#define SIXEYES_SLOT_COUNT 4

#define SIXEYES_JOINT_BASE 0
#define SIXEYES_JOINT_SHOULDER 1
#define SIXEYES_JOINT_ELBOW 2
#define SIXEYES_JOINT_WRIST_PITCH 3
#define SIXEYES_JOINT_WRIST_ROLL 4
#define SIXEYES_JOINT_GRIPPER 5

#define SIXEYES_HOMING_STATE_UNHOMED 0
#define SIXEYES_HOMING_STATE_HOMING 1
#define SIXEYES_HOMING_STATE_READY 2
#define SIXEYES_HOMING_STATE_FAULT 3
#define SIXEYES_CONTROL_SOURCE_BLE 0
#define SIXEYES_CONTROL_SOURCE_USB 1
#define SIXEYES_STATUS_OK 0
#define SIXEYES_STATUS_BAD_VERSION 1
#define SIXEYES_STATUS_NOT_HOMED 2
#define SIXEYES_STATUS_NOT_IN_CONTROL 3
#define SIXEYES_STATUS_BAD_ARGS 4
#define SIXEYES_STATUS_FAULTED 5
#define SIXEYES_STATUS_UNSUPPORTED 6
#define SIXEYES_STATUS_BUSY 7
#define SIXEYES_CAP_HOMING (1u << 0)
#define SIXEYES_CAP_BLE (1u << 1)
#define SIXEYES_CAP_ECHO_TARGETS (1u << 2)
#define SIXEYES_CAP_MOTION_SLOTS (1u << 3)
#define SIXEYES_CAP_HW_ESTOP (1u << 4)
#define SIXEYES_FAULT_HW_ESTOP (1u << 0)
#define SIXEYES_FAULT_WATCHDOG (1u << 1)
#define SIXEYES_FAULT_JOINT_LIMIT (1u << 2)
#define SIXEYES_FAULT_STALL (1u << 3)
#define SIXEYES_FAULT_BUS_LOST (1u << 4)
#define SIXEYES_FAULT_HOMING_FAILED (1u << 5)

/* Message type bytes */
#define SIXEYES_MSG_HELLO 0x01
#define SIXEYES_MSG_SET_CONTROL_SOURCE 0x02
#define SIXEYES_MSG_HOME 0x03
#define SIXEYES_MSG_TARGETS 0x04
#define SIXEYES_MSG_STOP 0x05
#define SIXEYES_MSG_CLEAR_FAULT 0x06
#define SIXEYES_MSG_HEARTBEAT 0x07
#define SIXEYES_MSG_SLOT_PLAY 0x08
#define SIXEYES_MSG_SLOT_ERASE 0x09
#define SIXEYES_MSG_SLOT_WRITE 0x0A
#define SIXEYES_MSG_HELLO_ACK 0x80
#define SIXEYES_MSG_STATE 0x81
#define SIXEYES_MSG_ACK 0x82

/* Payload sizes in bytes */
#define SIXEYES_SIZE_HELLO 2
#define SIXEYES_SIZE_SET_CONTROL_SOURCE 1
#define SIXEYES_SIZE_HOME 0
#define SIXEYES_SIZE_TARGETS 24
#define SIXEYES_SIZE_STOP 0
#define SIXEYES_SIZE_CLEAR_FAULT 0
#define SIXEYES_SIZE_HEARTBEAT 0
#define SIXEYES_SIZE_SLOT_PLAY 1
#define SIXEYES_SIZE_SLOT_ERASE 1
#define SIXEYES_SIZE_SLOT_WRITE 29
#define SIXEYES_SIZE_HELLO_ACK 7
#define SIXEYES_SIZE_STATE 56
#define SIXEYES_SIZE_ACK 3

/* First message after connecting. Tells the arm which protocol the host speaks. */
typedef struct {
    uint16_t protocol_version;
} sixeyes_hello_t;
static inline size_t sixeyes_hello_pack(const sixeyes_hello_t *m, uint8_t *out) {
    memcpy(out + 0, &m->protocol_version, 2);
    return SIXEYES_SIZE_HELLO;
}
static inline bool sixeyes_hello_unpack(const uint8_t *in, size_t len, sixeyes_hello_t *m) {
    if (len != SIXEYES_SIZE_HELLO) return false;
    memcpy(&m->protocol_version, in + 0, 2);
    return true;
}

/* Choose who may move the arm (ble or usb). Refused while the arm is moving. */
typedef struct {
    uint8_t source;
} sixeyes_set_control_source_t;
static inline size_t sixeyes_set_control_source_pack(const sixeyes_set_control_source_t *m, uint8_t *out) {
    memcpy(out + 0, &m->source, 1);
    return SIXEYES_SIZE_SET_CONTROL_SOURCE;
}
static inline bool sixeyes_set_control_source_unpack(const uint8_t *in, size_t len, sixeyes_set_control_source_t *m) {
    if (len != SIXEYES_SIZE_SET_CONTROL_SOURCE) return false;
    memcpy(&m->source, in + 0, 1);
    return true;
}

/* Joint targets. Counts as a heartbeat for the watchdog. */
typedef struct {
    float joints[6];
} sixeyes_targets_t;
static inline size_t sixeyes_targets_pack(const sixeyes_targets_t *m, uint8_t *out) {
    memcpy(out + 0, &m->joints, 24);
    return SIXEYES_SIZE_TARGETS;
}
static inline bool sixeyes_targets_unpack(const uint8_t *in, size_t len, sixeyes_targets_t *m) {
    if (len != SIXEYES_SIZE_TARGETS) return false;
    memcpy(&m->joints, in + 0, 24);
    return true;
}

/* (draft, needs capability motion_slots) Play a motion stored on the arm. */
typedef struct {
    uint8_t slot;
} sixeyes_slot_play_t;
static inline size_t sixeyes_slot_play_pack(const sixeyes_slot_play_t *m, uint8_t *out) {
    memcpy(out + 0, &m->slot, 1);
    return SIXEYES_SIZE_SLOT_PLAY;
}
static inline bool sixeyes_slot_play_unpack(const uint8_t *in, size_t len, sixeyes_slot_play_t *m) {
    if (len != SIXEYES_SIZE_SLOT_PLAY) return false;
    memcpy(&m->slot, in + 0, 1);
    return true;
}

/* (draft, needs capability motion_slots) Erase a stored motion. */
typedef struct {
    uint8_t slot;
} sixeyes_slot_erase_t;
static inline size_t sixeyes_slot_erase_pack(const sixeyes_slot_erase_t *m, uint8_t *out) {
    memcpy(out + 0, &m->slot, 1);
    return SIXEYES_SIZE_SLOT_ERASE;
}
static inline bool sixeyes_slot_erase_unpack(const uint8_t *in, size_t len, sixeyes_slot_erase_t *m) {
    if (len != SIXEYES_SIZE_SLOT_ERASE) return false;
    memcpy(&m->slot, in + 0, 1);
    return true;
}

/* (draft, needs capability motion_slots) Write one sample of a stored motion. */
typedef struct {
    uint8_t slot;
    uint16_t index;
    uint16_t dt_ms;
    float joints[6];
} sixeyes_slot_write_t;
static inline size_t sixeyes_slot_write_pack(const sixeyes_slot_write_t *m, uint8_t *out) {
    memcpy(out + 0, &m->slot, 1);
    memcpy(out + 1, &m->index, 2);
    memcpy(out + 3, &m->dt_ms, 2);
    memcpy(out + 5, &m->joints, 24);
    return SIXEYES_SIZE_SLOT_WRITE;
}
static inline bool sixeyes_slot_write_unpack(const uint8_t *in, size_t len, sixeyes_slot_write_t *m) {
    if (len != SIXEYES_SIZE_SLOT_WRITE) return false;
    memcpy(&m->slot, in + 0, 1);
    memcpy(&m->index, in + 1, 2);
    memcpy(&m->dt_ms, in + 3, 2);
    memcpy(&m->joints, in + 5, 24);
    return true;
}

/* Reply to hello. Carries the arm's protocol and firmware versions and what it can do. */
typedef struct {
    uint16_t protocol_version;
    uint8_t fw_major;
    uint8_t fw_minor;
    uint8_t fw_patch;
    uint16_t capabilities;
} sixeyes_hello_ack_t;
static inline size_t sixeyes_hello_ack_pack(const sixeyes_hello_ack_t *m, uint8_t *out) {
    memcpy(out + 0, &m->protocol_version, 2);
    memcpy(out + 2, &m->fw_major, 1);
    memcpy(out + 3, &m->fw_minor, 1);
    memcpy(out + 4, &m->fw_patch, 1);
    memcpy(out + 5, &m->capabilities, 2);
    return SIXEYES_SIZE_HELLO_ACK;
}
static inline bool sixeyes_hello_ack_unpack(const uint8_t *in, size_t len, sixeyes_hello_ack_t *m) {
    if (len != SIXEYES_SIZE_HELLO_ACK) return false;
    memcpy(&m->protocol_version, in + 0, 2);
    memcpy(&m->fw_major, in + 2, 1);
    memcpy(&m->fw_minor, in + 3, 1);
    memcpy(&m->fw_patch, in + 4, 1);
    memcpy(&m->capabilities, in + 5, 2);
    return true;
}

/* Streamed 50-100 times a second. measured = what the encoders say. target = the last target accepted from the source in control (this is the "echo" Mode 2 records). */
typedef struct {
    uint32_t t_ms;
    uint8_t homing_state;
    uint8_t control_source;
    uint16_t faults;
    float measured[6];
    float target[6];
} sixeyes_state_t;
static inline size_t sixeyes_state_pack(const sixeyes_state_t *m, uint8_t *out) {
    memcpy(out + 0, &m->t_ms, 4);
    memcpy(out + 4, &m->homing_state, 1);
    memcpy(out + 5, &m->control_source, 1);
    memcpy(out + 6, &m->faults, 2);
    memcpy(out + 8, &m->measured, 24);
    memcpy(out + 32, &m->target, 24);
    return SIXEYES_SIZE_STATE;
}
static inline bool sixeyes_state_unpack(const uint8_t *in, size_t len, sixeyes_state_t *m) {
    if (len != SIXEYES_SIZE_STATE) return false;
    memcpy(&m->t_ms, in + 0, 4);
    memcpy(&m->homing_state, in + 4, 1);
    memcpy(&m->control_source, in + 5, 1);
    memcpy(&m->faults, in + 6, 2);
    memcpy(&m->measured, in + 8, 24);
    memcpy(&m->target, in + 32, 24);
    return true;
}

/* Answer to every command. status 0 means done, anything else says why not. */
typedef struct {
    uint8_t ref_type;
    uint8_t ref_seq;
    uint8_t status;
} sixeyes_ack_t;
static inline size_t sixeyes_ack_pack(const sixeyes_ack_t *m, uint8_t *out) {
    memcpy(out + 0, &m->ref_type, 1);
    memcpy(out + 1, &m->ref_seq, 1);
    memcpy(out + 2, &m->status, 1);
    return SIXEYES_SIZE_ACK;
}
static inline bool sixeyes_ack_unpack(const uint8_t *in, size_t len, sixeyes_ack_t *m) {
    if (len != SIXEYES_SIZE_ACK) return false;
    memcpy(&m->ref_type, in + 0, 1);
    memcpy(&m->ref_seq, in + 1, 1);
    memcpy(&m->status, in + 2, 1);
    return true;
}

#endif /* SIXEYES_PROTOCOL_H */
