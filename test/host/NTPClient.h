#pragma once
#include "WiFiUdp.h"
extern unsigned mockNtpCalls;
class NTPClient {
 public:
  NTPClient(WiFiUDP&, const char*, int, int) {}
  void begin() {}
  bool update() { ++mockNtpCalls; delay(1020); return false; }
  unsigned long getEpochTime() { return 0; }
};
