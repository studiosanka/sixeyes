#pragma once
#include <cstdint>

class HeartbeatMonitor {
public:
  static HeartbeatMonitor &instance();
  void init(unsigned long timeout_ms);
  void feed(uint32_t source_id, uint32_t seq);
  bool isHealthy() const;
  void check();

private:
  HeartbeatMonitor();
  unsigned long timeout_ms_ = 500;
  unsigned long last_feed_ms_ = 0;
  uint32_t last_seq_ = 0;
};
