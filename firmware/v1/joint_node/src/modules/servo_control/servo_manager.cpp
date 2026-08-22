#include "servo_manager.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>

// v1 Elbow node servo pins -- see board_config.h SERVO_PIN_0/1/2
const uint8_t ServoManager::SERVO_PINS[NUM_SERVOS] = {SERVO_PIN_0, SERVO_PIN_1, SERVO_PIN_2};
constexpr uint16_t ServoManager::PULSE_MIN_US;
constexpr uint16_t ServoManager::PULSE_MID_US;
constexpr uint16_t ServoManager::PULSE_MAX_US;

ServoManager &ServoManager::instance() {
  static ServoManager inst;
  return inst;
}

ServoManager::ServoManager() {
  // Initialize pulse width array to mid-position (1500 µs = 90°)
  pulse_widths_us_.fill(PULSE_MID_US);
  target_positions_.fill(90.0f); // Default to 90 degrees
  last_command_ms_ = millis();
}

void ServoManager::init() {
  Serial.println("ServoManager: initializing MG996R servos");
  Serial.println("  - Frequency: 50 Hz (20 ms period)");
  Serial.println("  - Resolution: 16-bit PWM");
  Serial.println("  - Pulse range: 500-2500 µs (0-180°)");

  // Initialize LEDC (PWM). NOTE: adapted from legacy's ESP32-S3 core, which
  // used the old channel-based API (ledcSetup + ledcAttachPin). This core
  // version (arduino-esp32 3.x on ESP32-C6) uses the newer pin-based API
  // (ledcAttach per pin, ledcWrite keyed by pin not channel) -- caught by
  // build-testing this scaffold, not a design choice.
  for (uint8_t i = 0; i < NUM_SERVOS; ++i) {
    ledcAttach(SERVO_PINS[i], PWM_FREQUENCY, PWM_RESOLUTION);
    Serial.print("  Servo ");
    Serial.print(i);
    Serial.print(": GPIO");
    Serial.println(SERVO_PINS[i]);

    // Set to safe mid-position (1500 µs)
    setPWM(i, PULSE_MID_US);
  }

  Serial.println("ServoManager: init complete (PWM configured, servos at 90°)");
  last_command_ms_ = millis();
}

uint16_t ServoManager::degreesToMicroseconds(float degrees) const {
  // Clamp input to valid range
  degrees = std::max(DEGREES_MIN, std::min(DEGREES_MAX, degrees));

  // Linear interpolation: map 0-180° to 500-2500 µs
  float ratio = (degrees - DEGREES_MIN) / (DEGREES_MAX - DEGREES_MIN);
  uint16_t micros = PULSE_MIN_US + static_cast<uint16_t>(
                                       ratio * (PULSE_MAX_US - PULSE_MIN_US));

  return micros;
}

float ServoManager::microsecondsTooDegrees(uint16_t micros) const {
  // Clamp to valid range
  micros = std::max(PULSE_MIN_US, std::min(PULSE_MAX_US, micros));

  // Inverse: map 500-2500 µs to 0-180°
  float ratio =
      static_cast<float>(micros - PULSE_MIN_US) / (PULSE_MAX_US - PULSE_MIN_US);
  float degrees = DEGREES_MIN + ratio * (DEGREES_MAX - DEGREES_MIN);

  return degrees;
}

void ServoManager::setPWM(uint8_t servo_index, uint16_t micros) {
  if (servo_index >= NUM_SERVOS)
    return;

  // Convert microseconds to LEDC duty cycle
  // PWM period = 20 ms = 20000 µs
  // duty = (micros / 20000) * 65535
  uint32_t duty = (micros * 65535UL) / 20000UL;
  if (duty > 65535U)
    duty = 65535U;
  else if (duty < 0U)
    duty = 0U;

  ledcWrite(SERVO_PINS[servo_index], duty); // keyed by pin, not channel, in this core version
  pulse_widths_us_[servo_index] = micros;
}

void ServoManager::setPositions(
    const std::array<float, NUM_SERVOS> &positions) {
  target_positions_ = positions;
  last_command_ms_ = millis();

  if (enabled_) {
    for (uint8_t i = 0; i < NUM_SERVOS; ++i) {
      uint16_t micros = degreesToMicroseconds(positions[i]);
      setPWM(i, micros);
    }

    // Debug output (sparse to avoid serial spam)
    static unsigned long last_logged_ms = 0;
    if ((millis() - last_logged_ms) > 500) {
      Serial.print("ServoManager: positions [°] ");
      for (uint8_t i = 0; i < NUM_SERVOS; ++i) {
        Serial.print(positions[i], 1);
        Serial.print(" ");
      }
      Serial.println();
      last_logged_ms = millis();
    }
  }
}

std::array<float, NUM_SERVOS> ServoManager::getPositions() const {
  return target_positions_;
}

void ServoManager::setPosition(uint8_t servo_index, float degrees) {
  if (servo_index >= NUM_SERVOS)
    return;

  target_positions_[servo_index] = degrees;
  last_command_ms_ = millis();

  if (enabled_) {
    uint16_t micros = degreesToMicroseconds(degrees);
    setPWM(servo_index, micros);
  }
}

void ServoManager::setPulseMicroseconds(uint8_t servo_index, uint16_t micros) {
  if (servo_index >= NUM_SERVOS)
    return;

  // Clamp to valid servo range
  micros = std::max(PULSE_MIN_US, std::min(PULSE_MAX_US, micros));

  last_command_ms_ = millis();
  target_positions_[servo_index] = microsecondsTooDegrees(micros);

  if (enabled_) {
    setPWM(servo_index, micros);
  }
}

uint16_t ServoManager::getPulseMicroseconds(uint8_t servo_index) const {
  if (servo_index >= NUM_SERVOS)
    return PULSE_MID_US;
  return pulse_widths_us_[servo_index];
}

void ServoManager::enable() {
  if (enabled_)
    return; // Already enabled

  // Safe power-up: apply current target positions via PWM
  Serial.println("ServoManager: enabling servos...");

  for (uint8_t i = 0; i < NUM_SERVOS; ++i) {
    uint16_t micros = degreesToMicroseconds(target_positions_[i]);
    setPWM(i, micros);
  }

  enabled_ = true;
  last_command_ms_ = millis();
  Serial.println("ServoManager: servos enabled (PWM active)");
}

void ServoManager::disable() {
  if (!enabled_)
    return; // Already disabled

  // Safe shutdown: set all servos to mid-position before disabling PWM
  Serial.println("ServoManager: disabling servos...");

  for (uint8_t i = 0; i < NUM_SERVOS; ++i) {
    setPWM(i, PULSE_MID_US); // Safe neutral position
  }

  enabled_ = false;
  Serial.println("ServoManager: servos disabled (PWM off, set to 90°)");
}

bool ServoManager::enabled() const { return enabled_; }

void ServoManager::checkWatchdog() {
  if (!enabled_)
    return;

  unsigned long now_ms = millis();
  unsigned long time_since_command = now_ms - last_command_ms_;

  // If no command received for WATCHDOG_TIMEOUT_MS, disable servos
  if (time_since_command > WATCHDOG_TIMEOUT_MS) {
    Serial.println("ServoManager: watchdog timeout - disabling servos");
    disable();
  }
}
