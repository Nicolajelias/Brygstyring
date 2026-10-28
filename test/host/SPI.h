#pragma once
#include "Arduino.h"
#include <array>
constexpr int MSBFIRST = 1, SPI_BITORDER_MSBFIRST = 1, SPI_MODE1 = 1;
struct SPISettings { SPISettings(int, int, int) {} };
struct MockChip {
  std::array<uint8_t, 8> registers{};
  uint16_t nextRaw = 19076;  // approximately 65 C, includes fault bit at bit zero
  uint32_t conversionStarted = 0;
  bool disconnected = false, ignoreStart = false, neverFinish = false, faultHangs = false;
  uint8_t injectedFault = 0;
  unsigned starts = 0, earlyReads = 0;
  void tick() {
    if ((registers[0] & 0x20) && !neverFinish && uint32_t(mockNow - conversionStarted) >= 66) {
      registers[0] &= ~0x20;
      registers[1] = nextRaw >> 8; registers[2] = nextRaw & 255;
    }
  }
  void write(uint8_t address, uint8_t value) {
    if (disconnected) return;
    if (address == 0) {
      if (value & 2) { registers[7] = 0; value &= ~2; }
      if ((value & 0x0C) == 0x0C && !faultHangs) {
        value &= ~0x0C;
        registers[7] = injectedFault;
      }
      if (value & 0x20) {
        if (ignoreStart) return;
        conversionStarted = mockNow; ++starts;
      }
    }
    registers[address] = value;
  }
  uint8_t read(uint8_t address) {
    tick();
    if (disconnected) return 0xFF;
    if (address == 1 && (registers[0] & 0x20)) ++earlyReads;
    return registers[address];
  }
};
extern std::array<MockChip, 64> mockChips;
class SPIClass {
 public:
  int selected = -1, address = -1;
  bool writing = false;
  void begin(int = -1, int = -1, int = -1) {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t value) {
    if (selected < 0) return 0xFF;
    if (address < 0) { writing = value & 0x80; address = value & 0x7F; return 0; }
    const uint8_t reg = address++ & 7;
    if (writing) { mockChips[selected].write(reg, value); return 0; }
    return mockChips[selected].read(reg);
  }
};
extern SPIClass SPI;
