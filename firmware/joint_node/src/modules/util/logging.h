#pragma once
#include <Arduino.h>
#include <cstdarg>

namespace Logging {

// On the Base board the USB serial port carries the binary host protocol. Text
// mixed into that stream would corrupt frames, so once the first valid host
// frame arrives, text logging stops (UsbBridge calls setQuiet). Boot messages
// before that still appear in a serial monitor. For debugging without a host,
// build with -DHOST_LINK_TEXT_LOGS to keep logging always on (a host will then
// see a damaged frame now and then).
inline bool &quiet() {
  static bool value = false;
  return value;
}
inline void setQuiet(bool on) { quiet() = on; }
#ifdef HOST_LINK_TEXT_LOGS
inline bool muted() { return false; }
#else
inline bool muted() { return quiet(); }
#endif

inline void info(const char *msg) { if (!muted()) Serial.println(msg); }
inline void infof(const char *fmt, ...) {
  if (muted()) return;
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
}
inline void warn(const char *msg) {
  if (muted()) return;
  Serial.print("[WARN] ");
  Serial.println(msg);
}
inline void warnf(const char *fmt, ...) {
  if (muted()) return;
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print("[WARN] ");
  Serial.println(buf);
}
inline void error(const char *msg) {
  if (muted()) return;
  Serial.print("[ERROR] ");
  Serial.println(msg);
}
inline void errorf(const char *fmt, ...) {
  if (muted()) return;
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print("[ERROR] ");
  Serial.println(buf);
}
inline void debug(const char *msg) {
#ifdef SIXEYES_DEBUG
  if (muted()) return;
  Serial.print("[DBG] ");
  Serial.println(msg);
#endif
}
inline void debugf(const char *fmt, ...) {
#ifdef SIXEYES_DEBUG
  if (muted()) return;
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
