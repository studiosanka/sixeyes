#pragma once
#include <Arduino.h>
#include <cstddef>

class TelemetryFormatter {
public:
  static TelemetryFormatter &instance();
  void init();

  // Format a small telemetry JSON payload into the provided buffer.
  // Returns number of bytes written (not including null terminator).
  size_t formatBasicTelemetry(char *buf, size_t buf_len);

  // Format compact joint telemetry (4 steppers + 3 servos)
  size_t formatJointTelemetry(char *buf, size_t buf_len);

private:
  TelemetryFormatter();
};
