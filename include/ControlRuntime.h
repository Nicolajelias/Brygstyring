#pragma once
#include <Arduino.h>

namespace ControlRuntime {
// This recursive lock protects only short in-memory/control operations.
// Never keep it while doing network, display, logging or flash I/O.
class Guard {
 public:
  Guard();
  ~Guard();
  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;
};
bool begin();
bool maintenance();
bool enterMaintenance();
void leaveMaintenance();
struct Metrics { uint32_t cycles, maxGapMs, maxWorkMs, heartbeatAgeMs; bool watchdog; };
Metrics metrics();
class Maintenance {
  bool acquired;
 public:
  Maintenance() : acquired(enterMaintenance()) {}
  ~Maintenance() { if (acquired) leaveMaintenance(); }
  explicit operator bool() const { return acquired; }
};
}
