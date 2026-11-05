#pragma once
#include <cstdint>
#include <cstdio>
class IPAddress {
  uint8_t bytes[4]{};
 public:
  IPAddress() = default;
  IPAddress(int a,int b,int c,int d) : bytes{uint8_t(a),uint8_t(b),uint8_t(c),uint8_t(d)} {}
  bool fromString(const char* text) {
    unsigned a,b,c,d; char extra;
    if (sscanf(text,"%u.%u.%u.%u%c",&a,&b,&c,&d,&extra)!=4 || a>255 || b>255 || c>255 || d>255) return false;
    *this=IPAddress(a,b,c,d); return true;
  }
  uint8_t operator[](int i) const { return bytes[i]; }
};
constexpr int INADDR_NONE = -1;
