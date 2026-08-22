#pragma once
#include "tmc2209_config.h"
#include <Arduino.h>
#include <TMCStepper.h>
#include <cstdint>

class TMC2209Driver {
public:
  static TMC2209Driver &instance();
  // Initialize UART and PDN pins; uses Serial1 by default.
  // NOTE: adapted from legacy (ESP32-S3, which defaults to Serial2) --
  // ESP32-C6 only exposes Serial0/Serial1, not Serial2. This is a genuine
  // platform difference caught by build-testing this scaffold, not a design
  // choice; single-wire PDN_UART only needs one HardwareSerial instance
  // regardless of which one it is.
  void init(HardwareSerial &uart = Serial1);

  // === Configuration ===
  void configureMotor(uint8_t motor_index, uint16_t rms_current_ma = 800);
  void configureAllMotors(uint16_t rms_current_ma = 800);

  // === Diagnostics ===
  // DRV_STATUS (0x6F) - contains stall detection flag and other diagnostic bits
  bool readDrvStatus(uint8_t motor_index, uint32_t &status);

  // Check if motor has stalled via DRV_STATUS bit 24 (UART, use for homing)
  bool isStalled(uint8_t motor_index);

  // Fast stall check via DIAG GPIO — no UART overhead, use in control loop
  bool isDiagAsserted(uint8_t motor_index);

  // MSCNT (0x6A) — microstep counter, 0–1023 within one electrical cycle
  bool readMSCNT(uint8_t motor_index, uint16_t &mscnt);

  // SG_RESULT — StallGuard2 load value 0–1023 (0 = stall) from DRV_STATUS[9:0]
  bool readSGResult(uint8_t motor_index, uint16_t &sg_result);

  // IOIN (0x06) - read input/output pins state
  bool readIOIN(uint8_t motor_index, uint32_t &ioin);

  // === StallGuard Homing ===
  // Enable StallGuard detection on motor with given sensitivity (0-255)
  void enableStallGuard(uint8_t motor_index,
                        uint8_t sensitivity = TMC2209_SGTHRS_DEFAULT);
  void disableStallGuard(uint8_t motor_index);

  // === Current Control ===
  void setCurrent(uint8_t motor_index, uint16_t milliamps);
  uint16_t getCurrent(uint8_t motor_index);

  // === Low-level Register Access ===
  // Direct register I/O using TMCStepper's UART protocol handling
  bool writeRegister(uint8_t motor_index, uint8_t reg, uint32_t value);
  bool readRegister(uint8_t motor_index, uint8_t reg, uint32_t &value,
                    unsigned long timeout_ms = 50);

  void setBaud(unsigned long baud);

private:
  TMC2209Driver();
  HardwareSerial *uart_ = nullptr;
  // TMCStepper driver instances (one per physical TMC2209)
  TMC2209Stepper *drivers[TMC2209_NUM_DRIVERS];
  uint16_t last_current_ma_[TMC2209_NUM_DRIVERS] =
      {}; // Track configured current

  void selectDriver(uint8_t motor_index);
  void deselectAll();
};
