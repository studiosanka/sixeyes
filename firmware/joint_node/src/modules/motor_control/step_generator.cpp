#include "step_generator.h"
#include "modules/config/board_config.h"
#include <Arduino.h>
#include <cmath>

// DIR level that counts as the positive step direction. The whole firmware
// treats "positive steps" as the reference direction; which way the shaft
// really turns for a given level depends on the motor wiring (JDR3), and is
// settled by the encoder direction check during homing.
static constexpr uint8_t DIR_LEVEL_POSITIVE = HIGH;

static constexpr uint32_t TIMER_HZ = 1000000; // 1 us ticks

StepGenerator &StepGenerator::instance() {
  static StepGenerator inst;
  return inst;
}

void StepGenerator::init() {
  pinMode(TMC2209_STEP_PIN, OUTPUT);
  pinMode(TMC2209_DIR_PIN, OUTPUT);
  digitalWrite(TMC2209_STEP_PIN, LOW);
  digitalWrite(TMC2209_DIR_PIN, DIR_LEVEL_POSITIVE);
  dir_ = 1;

  timer_ = timerBegin(TIMER_HZ);
  timerAttachInterrupt(timer_, &StepGenerator::onTimer);
  timerStop(timer_);
  running_ = false;
  rate_ = 0.0f;
}

void IRAM_ATTR StepGenerator::onTimer() {
  StepGenerator &g = StepGenerator::instance();
  // One STEP pulse. The TMC2209 needs at least 100 ns high and low; a pair of
  // GPIO writes is well above that.
  digitalWrite(TMC2209_STEP_PIN, HIGH);
  digitalWrite(TMC2209_STEP_PIN, LOW);
  g.steps_ = g.steps_ + g.dir_;
}

void StepGenerator::setRate(float steps_per_s) {
  if (!timer_) return;

  const float mag = fabsf(steps_per_s);
  if (!(mag >= MIN_RATE_SPS)) { // also catches NaN
    if (running_) {
      timerStop(timer_);
      running_ = false;
    }
    rate_ = 0.0f;
    return;
  }

  const float capped = mag > MAX_RATE_SPS ? MAX_RATE_SPS : mag;
  const int8_t dir = steps_per_s > 0.0f ? 1 : -1;
  if (dir != dir_) {
    dir_ = dir;
    digitalWrite(TMC2209_DIR_PIN, dir > 0 ? DIR_LEVEL_POSITIVE : !DIR_LEVEL_POSITIVE);
  }
  rate_ = dir * capped;

  const uint32_t period = static_cast<uint32_t>(TIMER_HZ / capped);
  // Rewrite the alarm only when the period really changed (a few percent), so
  // a slowly changing rate does not restart the timer on every control tick.
  const uint32_t diff = period > period_us_ ? period - period_us_ : period_us_ - period;
  if (!running_ || diff * 50 > period_us_) {
    period_us_ = period;
    timerWrite(timer_, 0);
    timerAlarm(timer_, period, true, 0);
  }
  if (!running_) {
    timerStart(timer_);
    running_ = true;
  }
}
