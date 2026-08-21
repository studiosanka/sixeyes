// TMC2209 driver config: PDN pin assignments (PDN_UART mode)
// Pins from SixEyes Technical Reference, Section 3.2
#pragma once
#include <Arduino.h>

// Number of stepper drivers
#ifndef TMC2209_NUM_DRIVERS
#define TMC2209_NUM_DRIVERS 4
#endif

// PDN pins for each driver (active LOW to enable PDN_UART on that device)
// SixEyes Follower ESP32-S3 pin assignments:
// J1 (Base):       GPIO13
// J2 (Shoulder A): GPIO10
// J3 (Shoulder B): GPIO16
// J4 (Elbow):      GPIO6
static const uint8_t TMC2209_PDN_PINS[TMC2209_NUM_DRIVERS] = {13, 10, 16, 6};

// STEP / DIR / EN pins per motor channel (Follower ESP32-S3)
// Shared EN topology: all drivers tied to EN_ALL on GPIO14.
// J1 Base:       STEP=12, DIR=11, EN=GPIO14 (EN_ALL)
// J2 Shoulder A: STEP=9,  DIR=8,  EN=GPIO14 (EN_ALL)
// J3 Shoulder B: STEP=15, DIR=7,  EN=GPIO14 (EN_ALL)
// J4 Elbow:      STEP=5,  DIR=4,  EN=GPIO14 (EN_ALL)
static const uint8_t TMC2209_STEP_PINS[TMC2209_NUM_DRIVERS] = {12, 9, 15, 5};
static const uint8_t TMC2209_DIR_PINS[TMC2209_NUM_DRIVERS] = {11, 8, 7, 4};
static const uint8_t TMC2209_EN_PINS[TMC2209_NUM_DRIVERS] = {14, 14, 14, 14};

// TMC2209 EN is active LOW on common stepper carrier wiring.
static constexpr bool TMC2209_EN_ACTIVE_LOW = true;

// Motion conversion constants used by open-loop angle->step mapping.
static constexpr float TMC2209_FULL_STEPS_PER_REV = 200.0f;
static constexpr float TMC2209_MICROSTEPS = 16.0f;

// Sense resistor value used by TMC drivers (ohms)
// SixEyes uses 0.11 ohm sense resistors on all stepper channels
#ifndef TMC2209_R_SENSE
#define TMC2209_R_SENSE 0.11f
#endif

// === Official TMC2209 Register Addresses ===
// These are the documented register addresses per the TMC2209 datasheet
#define TMC2209_REG_GCONF 0x00     // Global configuration
#define TMC2209_REG_GSTAT 0x01     // Global status
#define TMC2209_REG_IFCNT 0x02     // Interface transmission counter
#define TMC2209_REG_SLAVECONF 0x03 // Slave configuration
#define TMC2209_REG_OTP_PROG 0x04  // OTP programming
#define TMC2209_REG_OTP_READ 0x05  // OTP read
#define TMC2209_REG_IOIN 0x06      // Inputs/Outputs
#define TMC2209_REG_FACTORY 0x07   // Factory configuration

#define TMC2209_REG_IHOLD_RUN 0x10  // Current setting (hold + run)
#define TMC2209_REG_TPOWERDOWN 0x11 // Power-down delay
#define TMC2209_REG_TSTEP 0x12      // Actual step time measured
#define TMC2209_REG_TPWM_THRS 0x13  // PWM threshold
#define TMC2209_REG_VDCMIN 0x14     // Minimum DC link voltage
#define TMC2209_REG_MSLUT0 0x60     // Lookup table (microstep)
#define TMC2209_REG_MSLUT1 0x61
#define TMC2209_REG_MSLUT2 0x62
#define TMC2209_REG_MSLUT3 0x63
#define TMC2209_REG_MSLUT4 0x64
#define TMC2209_REG_MSLUT5 0x65
#define TMC2209_REG_MSLUT6 0x66
#define TMC2209_REG_MSLUT7 0x67
#define TMC2209_REG_MSLUTSEL 0x68   // Lookup table selection
#define TMC2209_REG_MSLUTSTART 0x69 // Lookup table start
#define TMC2209_REG_MSCNT 0x6A      // Microstep counter
#define TMC2209_REG_MSCURACT 0x6B   // Microstep current

#define TMC2209_REG_CHOPCONF 0x6C // Chopper configuration
#define TMC2209_REG_COOLCONF 0x6D // Coolstep configuration & SG2 settings
#define TMC2209_REG_DRVSTATUS                                                  \
  0x6F // Driver status / DRV_STATUS (includes stall detection)

// StallGuard sensitivity (SGTHRS) is set in COOLCONF register (0x6D, bits 0-7)
// Higher values = more sensitive to stalls
#define TMC2209_SGTHRS_DEFAULT 100 // Default stall detection threshold (0-255)
