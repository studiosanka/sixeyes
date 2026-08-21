#include "heartbeat_monitor.h"
#include <Arduino.h>

HeartbeatMonitor &HeartbeatMonitor::instance() {
  static HeartbeatMonitor inst;
  return inst;
}

HeartbeatMonitor::HeartbeatMonitor() {}

void HeartbeatMonitor::init(unsigned long timeout_ms) {
  timeout_ms_ = timeout_ms;
  last_feed_ms_ = millis();
}

void HeartbeatMonitor::feed(uint32_t /*source_id*/, uint32_t seq) {
  last_seq_ = seq;
  last_feed_ms_ = millis();
}

bool HeartbeatMonitor::isHealthy() const {
  return (millis() - last_feed_ms_) <= timeout_ms_;
}

void HeartbeatMonitor::check() {
  if (!isHealthy()) {
    Serial.println("HeartbeatMonitor: heartbeat timeout");
  }
}
