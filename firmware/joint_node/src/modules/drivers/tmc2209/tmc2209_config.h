// TMC2209 driver config — v1 Universal Joint PCB.
// Unlike legacy Alpha/Beta (one follower MCU driving 4 steppers), each v1
// node drives exactly one stepper. tmc2209_driver.{h,cpp} are copied
// from the archived Beta firmware (branch archive/dev-2026-10), then adapted for v1.2: PDN_UART is a real
// single-wire UART here (TX via 1k, RX direct), not a per-driver chip-select.

#pragma once
#include "modules/config/board_config.h"
#include <Arduino.h>

#define TMC2209_NUM_DRIVERS 1

static const uint8_t TMC2209_STEP_PINS[TMC2209_NUM_DRIVERS] = { TMC2209_STEP_PIN };
static const uint8_t TMC2209_DIR_PINS[TMC2209_NUM_DRIVERS]  = { TMC2209_DIR_PIN };

// Active-low enable, pulled up on the PCB (RDR2) so the driver stays disabled
// until firmware drives it low. See TMC2209Driver::setEnabled().
static const uint8_t TMC2209_EN_PINS[TMC2209_NUM_DRIVERS] = { TMC2209_EN_PIN };

// No DIAG pin routed for runtime use in v1 -- StallGuard is homing-only
// (see docs/reference/CAN_MESSAGE_PROTOCOL.md §4.3a) and only needs to be
// enabled transiently during the homing sequence via UART register access,
// not a dedicated GPIO. Runtime stall detection is encoder-vs-command
// divergence instead (AS5600, see encoder_driver.h). 255 = not routed.
static const uint8_t TMC2209_DIAG_PINS[TMC2209_NUM_DRIVERS] = { 255 };

static constexpr bool TMC2209_EN_ACTIVE_LOW = true;

static constexpr float TMC2209_FULL_STEPS_PER_REV = 200.0f;
static constexpr float TMC2209_MICROSTEPS = 16.0f;

// Without UART the module's VREF pot sets the motor current, so the firmware reads VREF on an ADC pin
// and refuses to enable the driver if it is above this limit. Default 1.4 V is about 1.0 A RMS
// (Vref = 1.41 x I_rms for the 0.11 ohm sense resistors). Set it to your motor's rating; never above 2.2 V.
// With UART answering, current is digital and this check is skipped.
static constexpr float TMC2209_VREF_MAX_V = 1.4f;
static constexpr float TMC2209_AMPS_PER_VREF_V = 0.71f; // I_rms ~ Vref / 1.41

// Run current (RMS) the driver starts at in UART mode. Deliberately low and safe for a first power-up:
// raise it in small steps, and never above the motor's rated current (a generic 40 mm NEMA17 is ~1.5 A).
static constexpr uint16_t TMC2209_DEFAULT_RUN_MA = 500;

// Microsteps used without UART: only 1/8 (MS1=MS2=low) and 1/16 (both high) are used, the two settings the
// Trinamic datasheet and BigTreeTech's own table agree on.
static constexpr uint16_t TMC2209_STANDALONE_MICROSTEPS = 16;

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

// Sensorless homing (StallGuard). PLACEHOLDERS: StallGuard depends on motor, current, speed and the
// gearbox load, so both numbers must be tuned on the real joint. A stall is reported when SG_RESULT
// falls to 2 x SGTHRS or below (datasheet); a higher SGTHRS stalls earlier.
static constexpr uint16_t TMC2209_HOMING_RUN_MA = 400; // gentle on the hard stop; below the run current
static constexpr uint8_t TMC2209_SGTHRS_HOMING = 60;

// Backlash compensation: microsteps to inject on direction reversal.
// For 3D-printed cycloidal gearboxes tune empirically.
#define TMC2209_BACKLASH_STEPS 50
