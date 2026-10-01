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

  Serial.println("TMC2209Driver: initializing single-wire PDN_UART interface...");
  Serial.println("  - UART TX via 1k series resistor, RX direct to PDN_UART");
  Serial.println("  - Using TMCStepper UART protocol handler");

  // DIAG is not routed in v1 (255) -- only touch it if a board variant wires it.
  for (size_t i = 0; i < TMC2209_NUM_DRIVERS; ++i) {
    if (TMC2209_DIAG_PINS[i] != 255) pinMode(TMC2209_DIAG_PINS[i], INPUT);
  }

  // EN is active low and pulled up on the PCB; assert "disabled" explicitly
  // before anything else so a reset never leaves the motor energized.
  for (size_t i = 0; i < TMC2209_NUM_DRIVERS; ++i) {
    pinMode(TMC2209_EN_PINS[i], OUTPUT);
    setEnabled(i, false);
  }

  // PDN_UART at 115200 baud. TX and RX are separate GPIOs (board_config.h);
  // the PCB joins them at the TMC2209 end, so TMCStepper reads back its own
  // echoed bytes -- this is the standard TMCStepper single-wire wiring.
  uart_->begin(115200, SERIAL_8N1, TMC2209_UART_RX_PIN, TMC2209_UART_TX_PIN);
  delay(100); // Wait for UART to settle

  // MS1/MS2 are strapped to GND on the PCB -> UART address 0.
  for (uint8_t i = 0; i < TMC2209_NUM_DRIVERS; ++i) {
    drivers[i] = new TMC2209Stepper(uart_, TMC2209_R_SENSE, 0);
    if (drivers[i]) {
      drivers[i]->begin(); // Initialize the driver's internal state

      // Apply default safe configuration
      drivers[i]->toff(3);          // Comparator blank time
      drivers[i]->rms_current(600); // Conservative default 600mA
      drivers[i]->microsteps(16);   // 16 microsteps per step

      Serial.print("  Motor ");
      Serial.print(i);
      Serial.println(": initialized (disabled)");
    }
  }

  Serial.println("TMC2209Driver: init complete");
  Serial.println("  - Ready for configureAllMotors() or per-motor setup");
}

void TMC2209Driver::setEnabled(uint8_t motor_index, bool enabled) {
  if (motor_index >= TMC2209_NUM_DRIVERS) return;
  // EN is active low: LOW energizes the driver, HIGH disables it.
  const bool level_high = TMC2209_EN_ACTIVE_LOW ? !enabled : enabled;
  digitalWrite(TMC2209_EN_PINS[motor_index], level_high ? HIGH : LOW);
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

  status = drivers[motor_index]->DRV_STATUS();
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

bool TMC2209Driver::isDiagAsserted(uint8_t motor_index) {
  if (motor_index >= TMC2209_NUM_DRIVERS) return false;
  if (TMC2209_DIAG_PINS[motor_index] == 255) return false; // DIAG not routed in v1
  return digitalRead(TMC2209_DIAG_PINS[motor_index]) == HIGH;
}

bool TMC2209Driver::readMSCNT(uint8_t motor_index, uint16_t &mscnt) {
  if (motor_index >= TMC2209_NUM_DRIVERS || !drivers[motor_index]) return false;
  mscnt = drivers[motor_index]->MSCNT();
  return true;
}

bool TMC2209Driver::readSGResult(uint8_t motor_index, uint16_t &sg_result) {
  uint32_t status = 0;
  if (!readDrvStatus(motor_index, status)) return false;
  sg_result = static_cast<uint16_t>(status & 0x3FFU); // bits [9:0]
  return true;
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

  drivers[motor_index]->SGTHRS(sensitivity);

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

  drivers[motor_index]->SGTHRS(0);
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
// - Proper timing and synchronization
// No custom framing is needed.

bool TMC2209Driver::writeRegister(uint8_t motor_index, uint8_t reg,
                                  uint32_t value) {
  if (motor_index >= TMC2209_NUM_DRIVERS || !drivers[motor_index]) return false;

  bool ok = true;
  switch (reg) {
    case TMC2209_REG_GCONF:    drivers[motor_index]->GCONF(value);                          break;
    case TMC2209_REG_SGTHRS:   drivers[motor_index]->SGTHRS(static_cast<uint8_t>(value));   break;
    case TMC2209_REG_CHOPCONF: drivers[motor_index]->CHOPCONF(value);                       break;
    case TMC2209_REG_COOLCONF: drivers[motor_index]->COOLCONF(value);                       break;
    case TMC2209_REG_TCOOLTHRS:drivers[motor_index]->TCOOLTHRS(value);                      break;
    default: ok = false; break;
  }
  return ok;
}

bool TMC2209Driver::readRegister(uint8_t motor_index, uint8_t reg,
                                 uint32_t &value, unsigned long timeout_ms) {
  (void)timeout_ms;
  if (motor_index >= TMC2209_NUM_DRIVERS || !drivers[motor_index]) return false;

  bool ok = true;
  switch (reg) {
    case TMC2209_REG_GCONF:    value = drivers[motor_index]->GCONF();      break;
    case TMC2209_REG_GSTAT:    value = drivers[motor_index]->GSTAT();      break;
    case TMC2209_REG_IOIN:     value = drivers[motor_index]->IOIN();       break;
    case TMC2209_REG_TSTEP:    value = drivers[motor_index]->TSTEP();      break;
    case TMC2209_REG_MSCNT:    value = drivers[motor_index]->MSCNT();      break;
    case TMC2209_REG_CHOPCONF: value = drivers[motor_index]->CHOPCONF();   break;
    case TMC2209_REG_DRVSTATUS:value = drivers[motor_index]->DRV_STATUS(); break;
    case TMC2209_REG_SGRESULT: value = drivers[motor_index]->DRV_STATUS() & 0x3FFU; break;
    default: value = 0; ok = false; break;
  }
  return ok;
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
