// TMC2209 driver config — Beta Rev2 PCB
// GPIO assignments from SixEyes_Node0.pdf, Section 7 (GPIO Allocation Table)
// DIAG pins are new in Beta Rev2 (routed for StallGuard2 / CoolStep)

#pragma once
#include <Arduino.h>

#ifndef TMC2209_NUM_DRIVERS
#define TMC2209_NUM_DRIVERS 4
#endif

// PDN — single-wire UART; 1 kΩ series + 4.7 kΩ pull-up per line
// J1 Base: GPIO7  | J2 Shoulder A: GPIO14 | J3 Shoulder B: GPIO17 | J4 Elbow: GPIO47
static const uint8_t TMC2209_PDN_PINS[TMC2209_NUM_DRIVERS]  = { 7, 14, 17, 47};

// STEP pins
// J1 Base: GPIO5  | J2 Shoulder A: GPIO8  | J3 Shoulder B: GPIO15 | J4 Elbow: GPIO18
static const uint8_t TMC2209_STEP_PINS[TMC2209_NUM_DRIVERS] = { 5,  8, 15, 18};

// DIR pins
// J1 Base: GPIO6  | J2 Shoulder A: GPIO9  | J3 Shoulder B: GPIO16 | J4 Elbow: GPIO21
static const uint8_t TMC2209_DIR_PINS[TMC2209_NUM_DRIVERS]  = { 6,  9, 16, 21};

// EN — shared DRV_EN_ALL on GPIO4 (active LOW); see board_config.h DRV_EN_ALL_PIN
static const uint8_t TMC2209_EN_PINS[TMC2209_NUM_DRIVERS]   = { 4,  4,  4,  4};

// DIAG — StallGuard2 / fault flag; open-drain with 47 kΩ pull-up to 3.3V per line
// J1 Base: GPIO1  | J2 Shoulder A: GPIO2  | J3 Shoulder B: GPIO3  | J4 Elbow: GPIO48
static const uint8_t TMC2209_DIAG_PINS[TMC2209_NUM_DRIVERS] = { 1,  2,  3, 48};

static constexpr bool TMC2209_EN_ACTIVE_LOW = true;

static constexpr float TMC2209_FULL_STEPS_PER_REV = 200.0f;
static constexpr float TMC2209_MICROSTEPS = 16.0f;

#ifndef TMC2209_R_SENSE
#define TMC2209_R_SENSE 0.11f
#endif

// TMC2209 Register Addresses (unchanged — same IC as Alpha)
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

#define TMC2209_SGTHRS_DEFAULT 100
