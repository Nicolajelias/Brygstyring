#pragma once
#include "SPI.h"
class Adafruit_SPIDevice {
  int cs;
 public:
  Adafruit_SPIDevice(int c, int, int, int, SPIClass*) : cs(c) {}
  Adafruit_SPIDevice(int c, int, int, int, int, int, int) : cs(c) {}
  bool begin() { digitalWrite(cs, HIGH); return true; }
  bool write(const uint8_t* data, size_t length) {
    digitalWrite(cs, LOW);
    for (size_t i = 0; i < length; ++i) SPI.transfer(data[i]);
    digitalWrite(cs, HIGH); return true;
  }
  bool write_then_read(const uint8_t* data, size_t length, uint8_t* result, size_t count) {
    digitalWrite(cs, LOW);
    for (size_t i = 0; i < length; ++i) SPI.transfer(data[i]);
    for (size_t i = 0; i < count; ++i) result[i] = SPI.transfer(0xFF);
    digitalWrite(cs, HIGH); return true;
  }
};
