// Board configuration — v1.2 Universal Joint PCB
// Module: ESP32-C6 Super Mini (ESP32-C6FH4), socketed -- only the 2x10 header
//         pins are used (GPIO 0-7, 14-20 plus 5V/GND/3V3); the underside pads
//         (GPIO12/13 USB, 21/22/23) are NOT used.
// PCB:    4-layer, one design deployed to all joint nodes, through-hole
// Source: docs/reference/v1.2_PCB_Design_Reference.md (§3 pin map)

#pragma once
#include "node_config.h"

// Control loop — decided 250 Hz; see docs/reference/CAN_MESSAGE_PROTOCOL.md §5.
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
// PDN_UART is a real single-wire UART, not a chip-select: UART TX reaches
// PDN_UART through a 1k series resistor (RDR1), UART RX is wired straight to
// PDN_UART. EN is active low with a 10k pull-up to 3V3 (RDR2), so the driver
// is disabled until firmware drives it low.
// MS1/MS2 are driven by the ESP32: both LOW = UART address 0 (and 1/8 microstep
// without UART); both HIGH = 1/16 microstep without UART. VREF (the module's
// current-setting pot) is read on an ADC pin through a 10k series resistor (RDR3).
static constexpr int TMC2209_STEP_PIN    = 0;
static constexpr int TMC2209_DIR_PIN     = 1;
static constexpr int TMC2209_UART_TX_PIN = 5;  // MTDI strapping pin: SDIO timing only, no effect here
static constexpr int TMC2209_UART_RX_PIN = 17; // header pad labelled "RX"
static constexpr int TMC2209_EN_PIN      = 3;
static constexpr int TMC2209_MS1_PIN     = 15; // JTAG_SEL strapping pin: ignored with default eFuses
static constexpr int TMC2209_MS2_PIN     = 16; // UART0 TX (ROM boot log only; Serial is on USB CDC)
static constexpr int TMC2209_VREF_ADC_PIN = 2; // ADC1_CH2

// AS5600 magnetic encoder module (I2C, fixed address 0x36) -- replaces the
// MT6835 / AS5048A SPI encoder planned in v1.1. Pull-ups live on the module.
static constexpr int ENCODER_I2C_SDA_PIN = 18;
static constexpr int ENCODER_I2C_SCL_PIN = 19;

// CAN (TWAI) via MCP2562-E/P (VDD 5V, VIO 3V3). Both pins are "safe" GPIOs on
// the Super Mini; v1.1's GPIO4/5 were strapping pins and are no longer used.
static constexpr int CAN_TX_PIN = 14;
static constexpr int CAN_RX_PIN = 20;

// Servo PWM (open-loop) — Elbow only (JOINT_NODE_HAS_SERVOS). 1k series
// resistor on each line. GPIO4 is MTMS (strapping, SDIO timing only),
// GPIO6/7 are MTCK/MTDO (JTAG pins, free for GPIO use while JTAG_SEL on
// GPIO15 selects the internal USB-JTAG).
static constexpr int SERVO_PIN_0 = 6;
static constexpr int SERVO_PIN_1 = 7;
static constexpr int SERVO_PIN_2 = 4;

// USB Serial/JTAG (GPIO12/13) lives on the module's own USB-C connector, not
// on a header -- there is no board-level USB circuit. Serial must be mapped to
// USB CDC (platformio.ini sets ARDUINO_USB_CDC_ON_BOOT=1) because UART0's
// default pads, GPIO16/17, would collide with TMC2209_UART_RX_PIN.

// Not used / do not route: GPIO8 (RGB LED + strapping) and GPIO9 (BOOT button).
// Every other header GPIO is assigned (see the design reference, section 3).
