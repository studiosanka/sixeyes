// Step pulses for the TMC2209, from a hardware timer.
//
// The control loop (motor_controller) only decides a signed step RATE; this
// class turns that rate into evenly spaced STEP pulses in a timer interrupt, so
// the pulses do not wobble with whatever else loop() is doing (CAN, I2C, UART).
// It also counts every step it sends, which is the position the controller
// works with. Steps are counted in the driver's microsteps.
//
// Not bench-tested: the timer set-up follows the arduino-esp32 3.x API and has
// only been compiled.

#pragma once
#include <cstdint>
#include <esp32-hal-timer.h>

class StepGenerator {
public:
  static StepGenerator &instance();

  // Sets up the STEP and DIR pins and the timer. No pulses until a rate is set.
  void init();

  // Signed steps per second; the sign picks the DIR pin. 0 (or anything below
  // MIN_RATE_SPS) stops the pulses. Change the sign only through 0 where you
  // can: the DIR pin is switched immediately.
  void setRate(float steps_per_s);
  float rate() const { return rate_; }

  // Steps sent since init() (signed), or since setPosition().
  int32_t position() const { return steps_; }
  void setPosition(int32_t steps) { steps_ = steps; }

  static constexpr float MIN_RATE_SPS = 20.0f;
  static constexpr float MAX_RATE_SPS = 20000.0f; // far above what the joints need

private:
  StepGenerator() = default;

  static void onTimer(); // timer interrupt: one STEP pulse

  hw_timer_t *timer_ = nullptr;
  volatile int32_t steps_ = 0;
  volatile int8_t dir_ = 1;
  float rate_ = 0.0f;
  uint32_t period_us_ = 0;
  bool running_ = false;
};
