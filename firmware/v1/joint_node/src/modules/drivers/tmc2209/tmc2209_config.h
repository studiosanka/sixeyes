// TMC2209 driver config — v1 Universal Joint PCB.
// Unlike legacy Alpha/Beta (one follower MCU driving 4 steppers), each v1
// node drives exactly one stepper. tmc2209_driver.{h,cpp} are copied
// unchanged from firmware/legacy/beta -- they're already generic over
// TMC2209_NUM_DRIVERS, so only this config file differs.

#pragma once
#include "modules/config/board_config.h"
#include <Arduino.h>

#define TMC2209_NUM_DRIVERS 1

static const uint8_t TMC2209_PDN_PINS[TMC2209_NUM_DRIVERS]  = { TMC2209_UART_PIN };
static const uint8_t TMC2209_STEP_PINS[TMC2209_NUM_DRIVERS] = { TMC2209_STEP_PIN };
static const uint8_t TMC2209_DIR_PINS[TMC2209_NUM_DRIVERS]  = { TMC2209_DIR_PIN };

// v1 hardware doc does not define a separate driver-enable pin in the
// ESP32-C6-MINI-1 pin map (§3) -- TBD once the KiCad schematic exists.
// Placeholder pin 255 makes an unresolved EN line loud instead of silently
// wrong; fix before bring-up.
static const uint8_t TMC2209_EN_PINS[TMC2209_NUM_DRIVERS] = { 255 };

// No DIAG pin routed for runtime use in v1 -- StallGuard is homing-only
// (see docs/protocols/CAN_MESSAGE_PROTOCOL.md §4.3a) and only needs to be
// enabled transiently during the homing sequence via UART register access,
// not a dedicated GPIO. Runtime stall detection is encoder-vs-command
// divergence instead, computed against ENCODER_SPI_* in board_config.h.
static const uint8_t TMC2209_DIAG_PINS[TMC2209_NUM_DRIVERS] = { 255 };

static constexpr bool TMC2209_EN_ACTIVE_LOW = true;

static constexpr float TMC2209_FULL_STEPS_PER_REV = 200.0f;
static constexpr float TMC2209_MICROSTEPS = 16.0f;

#ifndef TMC2209_R_SENSE
#define TMC2209_R_SENSE 0.11f
#endif

// TMC2209 Register Addresses (unchanged -- same IC as legacy)
#define TMC2209_REG_GCONF      0x00
#define TMC2209_REG_GSTAT      0x01
#define TMC2209_REG_IFCNT      0x02
#define TMC2209_REG_SLAVECONF  0x03
#define TMC2209_REG_OTP_PROG   0x04
#define TMC2209_REG_OTP_READ   0x05
#define TMC2209_REG_IOIN       0x06
#define TMC2209_REG_FACTORY    0x07

#define TMC2209_REG_IHOLD_RUN  0x10
#define TMC2209_REG_TPOWERDOWN 0x11
#define TMC2209_REG_TSTEP      0x12
#define TMC2209_REG_TPWM_THRS  0x13
#define TMC2209_REG_VDCMIN     0x14
#define TMC2209_REG_MSLUT0     0x60
#define TMC2209_REG_MSLUT1     0x61
#define TMC2209_REG_MSLUT2     0x62
#define TMC2209_REG_MSLUT3     0x63
#define TMC2209_REG_MSLUT4     0x64
#define TMC2209_REG_MSLUT5     0x65
#define TMC2209_REG_MSLUT6     0x66
#define TMC2209_REG_MSLUT7     0x67
#define TMC2209_REG_MSLUTSEL   0x68
#define TMC2209_REG_MSLUTSTART 0x69
#define TMC2209_REG_MSCNT      0x6A
#define TMC2209_REG_MSCURACT   0x6B

#define TMC2209_REG_CHOPCONF   0x6C
#define TMC2209_REG_COOLCONF   0x6D
#define TMC2209_REG_DRVSTATUS  0x6F

#define TMC2209_REG_TCOOLTHRS  0x14
#define TMC2209_REG_SGTHRS     0x40
#define TMC2209_REG_SGRESULT   0x41

#define TMC2209_SGTHRS_DEFAULT 100

// Backlash compensation: microsteps to inject on direction reversal.
// For 3D-printed cycloidal gearboxes tune empirically.
#define TMC2209_BACKLASH_STEPS 50
