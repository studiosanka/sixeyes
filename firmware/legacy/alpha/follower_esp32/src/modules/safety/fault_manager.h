#pragma once
#include <cstdint>

class FaultManager {
public:
  enum class Fault : uint32_t {
    NONE = 0,
    HEARTBEAT_TIMEOUT = 1,
    OVERTEMP = 2,
    OVERCURRENT = 4
  };
  static FaultManager &instance();
  void init();
  void raise(Fault f);
  void clear();
  void clearFault(Fault f); // clears only the specified fault bit
  bool hasFault() const;
  Fault current() const;

private:
  FaultManager();
  uint32_t faults_ = 0; // bitmask; multiple simultaneous faults supported
};
