#pragma once
#include "WiFiUdp.h"
class NTPClient {
 public:
  NTPClient(WiFiUDP&, const char*, int, int) {}
  void begin() {}
  bool update() { return false; }
  unsigned long getEpochTime() { return 0; }
};
