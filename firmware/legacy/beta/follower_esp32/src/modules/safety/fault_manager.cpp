#include "fault_manager.h"
#include <Arduino.h>

FaultManager &FaultManager::instance() {
  static FaultManager inst;
  return inst;
}

FaultManager::FaultManager() {}

void FaultManager::init() { faults_ = 0; }

void FaultManager::raise(Fault f) {
  faults_ |= static_cast<uint32_t>(f);
  Serial.print("FaultManager: fault raised ");
  Serial.println(static_cast<uint32_t>(f));
}

void FaultManager::clear() {
  faults_ = 0;
  Serial.println("FaultManager: cleared");
}

void FaultManager::clearFault(Fault f) {
  faults_ &= ~static_cast<uint32_t>(f);
}

bool FaultManager::hasFault() const { return faults_ != 0; }

FaultManager::Fault FaultManager::current() const {
  return static_cast<Fault>(faults_);
}
