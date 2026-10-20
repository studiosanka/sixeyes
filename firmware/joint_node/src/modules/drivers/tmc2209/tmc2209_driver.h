#pragma once
#include "tmc2209_config.h"
#include <Arduino.h>
#include <TMCStepper.h>
#include <cstdint>

class TMC2209Driver {
public:
  static TMC2209Driver &instance();
  // Initialize UART and EN pin; uses Serial1 by default. The driver starts
  // disabled (EN high) -- call setEnabled(i, true) to energize the motor.
  // NOTE: adapted from legacy (ESP32-S3, which defaults to Serial2) --
  // ESP32-C6 only exposes Serial0/Serial1, not Serial2. This is a genuine
  // platform difference caught by build-testing this scaffold, not a design
  // choice; single-wire PDN_UART only needs one HardwareSerial instance
  // regardless of which one it is.
  void init(HardwareSerial &uart = Serial1);

  // === Enable ===
  // Drives the active-low EN pin. Safe to call from the safety path. Disabling always works. Enabling is
  // refused (driver stays off, reason printed) when UART is not answering and the module's VREF pot is
  // set above TMC2209_VREF_MAX_V, because the pot then sets the motor current.
  void setEnabled(uint8_t motor_index, bool enabled);

  // === UART / standalone mode ===
  // True when the driver answered on UART at init. If false the driver runs STEP/DIR only: the VREF pot sets
  // the current, MS1/MS2 (driven by this class) set the microstep size, and register access is disabled.
  bool uartAvailable() const { return uart_ok_; }
  // Microsteps: with UART any power of two 1..256 is written to the driver; without UART only 8 or 16 work
  // (MS1/MS2 low / both high). Returns false if the value is not possible in the current mode.
  bool setMicrosteps(uint16_t microsteps);
  uint16_t getMicrosteps() const { return microsteps_; }
  // VREF pad voltage read on the ADC (average of 32 samples), and what current that setting means.
  float readVrefVolts();
  float vrefToAmps(float vref_volts) const { return vref_volts * TMC2209_AMPS_PER_VREF_V; }
  bool vrefWithinLimit();

  // === Configuration ===
  void configureMotor(uint8_t motor_index, uint16_t rms_current_ma = TMC2209_DEFAULT_RUN_MA);
  void configureAllMotors(uint16_t rms_current_ma = TMC2209_DEFAULT_RUN_MA);

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
  bool uart_ok_ = false;
  uint16_t microsteps_ = 16;
  // TMCStepper driver instances (one per physical TMC2209)
  TMC2209Stepper *drivers[TMC2209_NUM_DRIVERS];
  uint16_t last_current_ma_[TMC2209_NUM_DRIVERS] =
      {}; // Track configured current
};
