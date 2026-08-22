#pragma once
#include <Arduino.h>
#include <cstdarg>

namespace Logging {
inline void info(const char *msg) { Serial.println(msg); }
inline void infof(const char *fmt, ...) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
}
inline void warn(const char *msg) {
  Serial.print("[WARN] ");
  Serial.println(msg);
}
inline void warnf(const char *fmt, ...) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print("[WARN] ");
  Serial.println(buf);
}
inline void error(const char *msg) {
  Serial.print("[ERROR] ");
  Serial.println(msg);
}
inline void debug(const char *msg) {
#ifdef SIXEYES_DEBUG
  Serial.print("[DBG] ");
  Serial.println(msg);
#endif
}
inline void debugf(const char *fmt, ...) {
#ifdef SIXEYES_DEBUG
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print("[DBG] ");
  Serial.println(buf);
#endif
}
} // namespace Logging
