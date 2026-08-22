// Board configuration — v1 Universal Joint PCB
// Module: ESP32-C6-MINI-1
// PCB:    ~42x42mm, 4-layer, one design deployed to all joint nodes
// Source: docs/hardware/v1/v1_PCB_Design_Reference.md

#pragma once
#include "node_config.h"

// Control loop — decided 250 Hz; see docs/protocols/CAN_MESSAGE_PROTOCOL.md §5.
// CAN bus bandwidth (~27% util at this rate) and single-core ESP32-C6 headroom
// drove this down from the legacy Alpha/Beta 400-500 Hz precedent. E-stop
// disable is handled in the CAN RX ISR, not gated on this loop's tick —
// see modules/safety/can_safety_task.h.
#ifndef CONTROL_LOOP_HZ
#define CONTROL_LOOP_HZ 250
#endif

static constexpr unsigned long BUS_HEARTBEAT_TIMEOUT_MS = 500;
static constexpr unsigned long NODE_LIVENESS_TIMEOUT_MS = 500; // Base-side only

// TMC2209 — single stepper per node (v1 config differs from Alpha/Beta's
// TMC2209_NUM_DRIVERS=4; each joint node here drives exactly one motor).
static constexpr int TMC2209_STEP_PIN = 0;
static constexpr int TMC2209_DIR_PIN  = 1;
static constexpr int TMC2209_UART_PIN = 2; // PDN_UART, single-wire

// SPI magnetic encoder (MT6835 / AS5048A) — new in v1, no equivalent on
// Alpha/Beta follower.
static constexpr int ENCODER_SPI_SCK_PIN  = 18;
static constexpr int ENCODER_SPI_MISO_PIN = 19;
static constexpr int ENCODER_SPI_MOSI_PIN = 20;
static constexpr int ENCODER_SPI_CS_PIN   = 21;

// CAN (TWAI) — GPIO4/5 are ESP32-C6 strapping pins.
// ⚠ Verify against the ESP32-C6 TRM boot-config table before PCB layout;
// flagged as open in the v1 hardware doc §3, not yet re-verified here.
static constexpr int CAN_TX_PIN = 4;
static constexpr int CAN_RX_PIN = 5;

// Servo PWM (open-loop) — Elbow only (JOINT_NODE_HAS_SERVOS).
static constexpr int SERVO_PIN_0 = 14;
static constexpr int SERVO_PIN_1 = 22;
static constexpr int SERVO_PIN_2 = 23;

// USB Serial/JTAG — Base only, bridges to laptop/ROS2.
static constexpr int USB_DM_PIN = 12;
static constexpr int USB_DP_PIN = 13;

// Reserved / do not route externally (strapping or flash use on this module):
// GPIO4, 5 (also used for CAN above — see TRM caveat), 8, 9, 15, 24-30.
