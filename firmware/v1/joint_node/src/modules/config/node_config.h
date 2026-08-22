// Node role selection — Universal Joint PCB, one image / four roles.
// Set at compile time via platformio.ini: build_flags = -DJOINT_NODE_ID=N
// See docs/protocols/CAN_MESSAGE_PROTOCOL.md §2 (Node Addressing).

#pragma once

#define JOINT_NODE_BASE       1
#define JOINT_NODE_SHOULDER_L 2
#define JOINT_NODE_SHOULDER_R 3
#define JOINT_NODE_ELBOW      4

#ifndef JOINT_NODE_ID
#error "JOINT_NODE_ID not set. Build via one of the v1_base/v1_shoulder_l/v1_shoulder_r/v1_elbow environments."
#endif

#if JOINT_NODE_ID < JOINT_NODE_BASE || JOINT_NODE_ID > JOINT_NODE_ELBOW
#error "JOINT_NODE_ID must be 1 (Base), 2 (Shoulder L), 3 (Shoulder R), or 4 (Elbow)."
#endif

// Only Base bridges to the laptop/ROS2 side over USB and originates BUS_HEARTBEAT.
#define JOINT_NODE_IS_BASE (JOINT_NODE_ID == JOINT_NODE_BASE)

// Only Elbow populates servo headers (open-loop, 3x).
#define JOINT_NODE_HAS_SERVOS (JOINT_NODE_ID == JOINT_NODE_ELBOW)

// Shoulder R runs inverse-direction logic relative to Shoulder L (see v1 hardware doc §1).
#define JOINT_NODE_INVERT_DIRECTION (JOINT_NODE_ID == JOINT_NODE_SHOULDER_R)

// 120 Ohm CAN termination is populated at Base and Elbow only (physical bus ends).
// This is a hardware BOM fact (DNP on Shoulder L/R), not firmware-configurable —
// documented here so it's visible next to the other per-role differences.
#define JOINT_NODE_HAS_CAN_TERMINATION \
  (JOINT_NODE_ID == JOINT_NODE_BASE || JOINT_NODE_ID == JOINT_NODE_ELBOW)

inline const char *jointNodeName() {
  switch (JOINT_NODE_ID) {
    case JOINT_NODE_BASE:       return "Base";
    case JOINT_NODE_SHOULDER_L: return "Shoulder L";
    case JOINT_NODE_SHOULDER_R: return "Shoulder R";
    case JOINT_NODE_ELBOW:      return "Elbow";
    default:                    return "Unknown";
  }
}
