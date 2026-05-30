#pragma once
#include <Arduino.h>

namespace Timekeeping {
inline unsigned long nowMs() { return millis(); }
} // namespace Timekeeping
