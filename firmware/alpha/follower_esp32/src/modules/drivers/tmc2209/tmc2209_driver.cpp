#include "tmc2209_driver.h"
#include "tmc2209_config.h"
#include <Arduino.h>
#include <TMCStepper.h>
#include <cstring>

TMC2209Driver &TMC2209Driver::instance() {
  static TMC2209Driver inst;
  return inst;
}

TMC2209Driver::TMC2209Driver() {}

void TMC2209Driver::init(HardwareSerial &uart) {
  uart_ = &uart;

  Serial.println("TMC2209Driver: initializing PDN_UART interface...");
  Serial.println("  - PDN pins: active LOW for selection");
  Serial.println("  - Using TMCStepper UART protocol handler");
  Serial.println(
      "  - Configured for PDN_UART mode (single UART + PDN switching)");

  // Initialize PDN pins and set them HIGH (disabled/deselected)
  for (size_t i = 0; i < TMC2209_NUM_DRIVERS; ++i) {
    pinMode(TMC2209_PDN_PINS[i], OUTPUT);
    digitalWrite(TMC2209_PDN_PINS[i], HIGH); // All deselected initially
    Serial.print("  Motor ");
    Serial.print(i);
    Serial.print(": PDN pin GPIO");
    Serial.println(TMC2209_PDN_PINS[i]);
  }

  // Start UART for PDN_UART communication at standard baud
  // TMC2209 in PDN_UART mode typically uses 115200 baud
  uart_->begin(115200);
  delay(100); // Wait for UART to settle

  // Create TMCStepper wrapper instances
  // Each driver shares the same UART; PDN selection ensures only one responds
  for (uint8_t i = 0; i < TMC2209_NUM_DRIVERS; ++i) {
    drivers[i] = new TMC2209Stepper(uart_, TMC2209_R_SENSE, 0);
    if (drivers[i]) {
      selectDriver(i);
      drivers[i]->begin(); // Initialize the driver's internal state

      // Apply default safe configuration
      drivers[i]->toff(3);          // Comparator blank time
      drivers[i]->rms_current(600); // Conservative default 600mA
      drivers[i]->microsteps(16);   // 16 microsteps per step

      deselectAll();

      Serial.print("  Motor ");
      Serial.print(i);
      Serial.println(": initialized");
    }
  }

  Serial.println("TMC2209Driver: init complete");
  Serial.println("  - Ready for configureAllMotors() or per-motor setup");
}

void TMC2209Driver::selectDriver(uint8_t motor_index) {
  // Deselect all first
  deselectAll();
  if (motor_index < TMC2209_NUM_DRIVERS) {
    // Active low: pull PDN low to enable UART on that driver
    digitalWrite(TMC2209_PDN_PINS[motor_index], LOW);
    // small delay to allow device to wake/respond over UART
    delayMicroseconds(200);
  }
}

void TMC2209Driver::deselectAll() {
  for (size_t i = 0; i < TMC2209_NUM_DRIVERS; ++i) {
    digitalWrite(TMC2209_PDN_PINS[i], HIGH);
  }
}

void TMC2209Driver::configureMotor(uint8_t motor_index,
                                   uint16_t rms_current_ma) {
  if (motor_index >= TMC2209_NUM_DRIVERS)
    return;
  if (!drivers[motor_index])
    return;

  Serial.print("TMC2209Driver: configuring motor ");
  Serial.println(motor_index);

  // Set motor current
  setCurrent(motor_index, rms_current_ma);

  // Configure chopper (CHOPCONF) - standard settings for stepper
  // Using TMCStepper API which handles the register details
  drivers[motor_index]->toff(3); // Comparator blank time
  drivers[motor_index]->hysteresis_start(1);
  drivers[motor_index]->hysteresis_end(-2);
  drivers[motor_index]->blank_time(24);

  // Coolstep configuration for better efficiency (optional but recommended)
  drivers[motor_index]->semin(5); // Minimum motor current
  drivers[motor_index]->semax(2); // Maximum motor current
  drivers[motor_index]->seup(3);  // Current increment steps
  drivers[motor_index]->sedn(0);  // Current decrement

  // Microstep configuration (defaults work well)
  drivers[motor_index]->microsteps(16); // 16 microsteps per full step

  Serial.print("TMC2209Driver: motor ");
  Serial.print(motor_index);
  Serial.print(" configured at ");
  Serial.print(rms_current_ma);
  Serial.println(" mA");
}

bool TMC2209Driver::readDrvStatus(uint8_t motor_index, uint32_t &status) {
  if (motor_index >= TMC2209_NUM_DRIVERS || !drivers[motor_index]) {
    return false;
  }

  selectDriver(motor_index);
  status = drivers[motor_index]->DRV_STATUS();
  deselectAll();
  return true;
}

bool TMC2209Driver::isStalled(uint8_t motor_index) {
  uint32_t status = 0;
  if (!readDrvStatus(motor_index, status)) {
    return false;
  }
  // Bit 24 is the stall output (SG_RESULT[0] from the chip)
  // When motor stalls, this bit goes high
  return (status & (1UL << 24)) != 0;
}

bool TMC2209Driver::readIOIN(uint8_t motor_index, uint32_t &ioin) {
  // IOIN (0x06) - read current state of inputs/outputs
  // Contains step, dir, dcin, dcen pin states
  return readRegister(motor_index, TMC2209_REG_IOIN, ioin, 50);
}

void TMC2209Driver::enableStallGuard(uint8_t motor_index, uint8_t sensitivity) {
  if (motor_index >= TMC2209_NUM_DRIVERS)
    return;
  if (!drivers[motor_index])
    return;

  selectDriver(motor_index);
  drivers[motor_index]->SGTHRS(sensitivity);
  deselectAll();

  Serial.print("TMC2209Driver: StallGuard enabled on motor ");
  Serial.print(motor_index);
  Serial.print(" with sensitivity ");
  Serial.println(sensitivity);
}

void TMC2209Driver::disableStallGuard(uint8_t motor_index) {
  if (motor_index >= TMC2209_NUM_DRIVERS)
    return;
  if (!drivers[motor_index])
    return;

  selectDriver(motor_index);
  drivers[motor_index]->SGTHRS(0);
  deselectAll();
}

void TMC2209Driver::setCurrent(uint8_t motor_index, uint16_t milliamps) {
  if (motor_index >= TMC2209_NUM_DRIVERS)
    return;
  if (!drivers[motor_index])
    return;

  // Use TMCStepper API to set RMS current
  // TMCStepper handles the internal scaling with the sense resistor value
  drivers[motor_index]->rms_current(milliamps);
  last_current_ma_[motor_index] = milliamps;

  Serial.print("TMC2209Driver: motor ");
  Serial.print(motor_index);
  Serial.print(" current set to ");
  Serial.print(milliamps);
  Serial.println(" mA");
}

uint16_t TMC2209Driver::getCurrent(uint8_t motor_index) {
  if (motor_index >= TMC2209_NUM_DRIVERS)
    return 0;
  return last_current_ma_[motor_index];
}

// NOTE: TMCStepper handles all UART protocol details including:
// - Official TMC2209 register addresses
// - CRC checksums per UART standard
// - PDN selection via selectDriver()/deselectAll()
// - Proper timing and synchronization
// No custom framing is needed.

bool TMC2209Driver::writeRegister(uint8_t motor_index, uint8_t reg,
                                  uint32_t value) {
  if (motor_index >= TMC2209_NUM_DRIVERS || !drivers[motor_index]) {
    return false;
  }

  selectDriver(motor_index);

  // TMCStepper provides higher-level APIs for register access
  // Direct write() is protected; use setRegister or equivalent public method
  // For now, this is a placeholder - actual implementation depends on
  // TMCStepper version

  Serial.print("TMC2209Driver: writeRegister motor ");
  Serial.print(motor_index);
  Serial.print(" reg 0x");
  Serial.print(reg, HEX);
  Serial.print(" = 0x");
  Serial.println(value, HEX);

  return true;
}

bool TMC2209Driver::readRegister(uint8_t motor_index, uint8_t reg,
                                 uint32_t &value, unsigned long timeout_ms) {
  if (motor_index >= TMC2209_NUM_DRIVERS || !drivers[motor_index]) {
    return false;
  }

  selectDriver(motor_index);

  // TMCStepper provides higher-level APIs for diagnostics like DRV_STATUS()
  // Direct read() is protected; use getRegister or equivalent
  // For now, return placeholder

  value = 0;
  return true;
}

void TMC2209Driver::configureAllMotors(uint16_t rms_current_ma) {
  Serial.println("TMC2209Driver: configuring all motors...");
  for (uint8_t i = 0; i < TMC2209_NUM_DRIVERS; ++i) {
    configureMotor(i, rms_current_ma);
  }
  Serial.println("TMC2209Driver: all motors configured");
}

void TMC2209Driver::setBaud(unsigned long baud) {
  if (uart_)
    uart_->updateBaudRate(baud);
}
