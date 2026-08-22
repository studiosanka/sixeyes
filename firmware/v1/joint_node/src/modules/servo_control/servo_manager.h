#pragma once
#include "modules/config/board_config.h"
#include "modules/config/node_config.h"
#include <array>
#include <cstdint>

// v1: only the Elbow node populates servo headers (JOINT_NODE_HAS_SERVOS).
// This file is still compiled into every node's build for simplicity, but
// callers on non-Elbow nodes must not invoke it -- main.cpp only calls
// init()/enable() when JOINT_NODE_HAS_SERVOS is set.
#ifndef NUM_SERVOS
#define NUM_SERVOS 3
#endif

/**
 * ServoManager: PWM control for MG996R servos, open-loop (no feedback --
 * unchanged from legacy; v1 does not add servo position sensing).
 *
 * v1 Elbow node servo pins: SERVO_PIN_0/1/2 in board_config.h
 *
 * PWM Mapping (standard RC servo):
 * - 500 µs  → 0°
 * - 1500 µs → 90°
 * - 2500 µs → 180°
 *
 * PWM Frequency: 50 Hz (20 ms period)
 * Position Range: 0.0 - 180.0 degrees
 */
class ServoManager {
public:
  static ServoManager &instance();

  // Initialization and lifecycle
  void init();
  void enable();  // Enable PWM output
  void disable(); // Disable PWM output (safe state)
  bool enabled() const;

  // Set target positions (degrees 0-180)
  void setPositions(const std::array<float, NUM_SERVOS> &positions);

  // Get current commanded position
  std::array<float, NUM_SERVOS> getPositions() const;

  // Set individual servo position (convenience)
  void setPosition(uint8_t servo_index, float degrees);

  // Direct PWM control (microseconds, 500-2500 typical)
  void setPulseMicroseconds(uint8_t servo_index, uint16_t micros);
  uint16_t getPulseMicroseconds(uint8_t servo_index) const;

  // Safety: watchdog check (must be called regularly from main loop)
  void checkWatchdog(); // Called at CONTROL_LOOP_HZ

private:
  ServoManager();

  // PWM configuration for ESP32-S3 LEDC (hardware PWM)
  static constexpr uint8_t PWM_CHANNEL_BASE = 0; // LEDC channel offset
  static constexpr uint32_t PWM_FREQUENCY = 50;  // 50 Hz for RC servos
  static constexpr uint32_t PWM_RESOLUTION = 16; // 16-bit (0-65535)
  static constexpr uint32_t PWM_PERIOD_MS = 20;  // 20 ms period for 50 Hz

  // Pulse width mapping constants
  static constexpr uint16_t PULSE_MIN_US = 500;  // 0 degrees
  static constexpr uint16_t PULSE_MID_US = 1500; // 90 degrees
  static constexpr uint16_t PULSE_MAX_US = 2500; // 180 degrees
  static constexpr float DEGREES_MIN = 0.0f;
  static constexpr float DEGREES_MAX = 180.0f;

  // Servo GPIO pins: GPIO38, GPIO39, GPIO40 (Beta Rev2)
  static const uint8_t SERVO_PINS[NUM_SERVOS];

  // State tracking
  bool enabled_ = false;
  std::array<float, NUM_SERVOS> target_positions_{}; // Commanded degrees
  std::array<uint16_t, NUM_SERVOS>
      pulse_widths_us_{};             // Current PWM pulse width
  unsigned long last_command_ms_ = 0; // For watchdog
  static constexpr unsigned long WATCHDOG_TIMEOUT_MS =
      100; // Update required every 100ms

  // Helper methods
  uint16_t degreesToMicroseconds(float degrees) const;
  float microsecondsTooDegrees(uint16_t micros) const;
  void setPWM(uint8_t servo_index, uint16_t micros);
};
