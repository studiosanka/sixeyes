// Board configuration — Beta Rev2
// Module: ESP32-S3-WROOM-1-N16R8 (16 MB Flash, 8 MB Octal PSRAM)
// PCB:    SixEyes Follower Beta Rev2, 60×60 mm 4-layer custom

#pragma once

// FORBIDDEN GPIOs — N16R8 octal PSRAM/flash silicon use only.
// Must not be connected to any external pad, pull resistor, via, or connector:
//   GPIO26  internal octal flash clock
//   GPIO33  Flash D4
//   GPIO34  Flash D5
//   GPIO35  Octal PSRAM D6 / WP
//   GPIO36  Octal PSRAM D7 / HOLD
//   GPIO37  Octal PSRAM CLK

// Safety
static constexpr unsigned long SAFETY_HEARTBEAT_TIMEOUT_MS = 500;

// Control loop — Beta runs at 400 Hz per PCB design spec
static constexpr int CONTROL_LOOP_HZ = 400;

// Actuator counts (unchanged from Alpha)
static constexpr int NUM_STEPPERS = 4;
static constexpr int NUM_SERVOS   = 3;

// Motor enable — shared, active LOW on Beta Rev2
static constexpr int DRV_EN_ALL_PIN = 4;

// Leader UART (Node1 ESP32-C6, 921600 baud, 8N1)
static constexpr int LEADER_UART_TX_PIN = 41; // LDR_UART_TX → J_LDR Pin 1
static constexpr int LEADER_UART_RX_PIN = 42; // LDR_UART_RX ← J_LDR Pin 2

// SD card SPI (Beta Rev2 — different from Alpha)
static constexpr int SD_CS_PIN   = 10;
static constexpr int SD_MOSI_PIN = 11;
static constexpr int SD_SCK_PIN  = 12;
static constexpr int SD_MISO_PIN = 13;

// Servo PWM outputs (Beta Rev2)
static constexpr int SERVO_PIN_0 = 38; // Wrist Pitch → J_SV1
static constexpr int SERVO_PIN_1 = 39; // Wrist Yaw   → J_SV2
static constexpr int SERVO_PIN_2 = 40; // Gripper     → J_SV3

// Boot strapping (Beta Rev2)
// GPIO0  = MCU_BOOT      10 kΩ pull-up to 3.3V; tactile switch to GND
// GPIO46 = MCU_STRAP_LOW 10 kΩ pull-down to GND; required by ESP32-S3 spec
// GPIO45 = leave floating (internal OTP default)

// Dual USB-C (Beta Rev2)
// GPIO19 = USB_D-  Native USB-OTG → J_USB1 (ROS2 bridge / telemetry)
// GPIO20 = USB_D+  Native USB-OTG → J_USB1
// GPIO43 = UART0_TXD  CH340K debug/flash → J_USB2
// GPIO44 = UART0_RXD  CH340K debug/flash → J_USB2
