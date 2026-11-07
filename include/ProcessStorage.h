#pragma once
#include <Arduino.h>
#include "BrewRecipe.h"
namespace ProcessStorage {
struct Record {
  uint32_t version = 2, epoch = 0, elapsed = 0;
  uint8_t state = 0, timer = 0, previous = 0, hopMask = 0;
  int8_t activeHop = -1;
  uint8_t completed = 0, expired = 0, reserved = 0;
  BrewRecipe recipe{};
  uint32_t checksum = 0;
};
void begin();
void stage(const Record& record);
bool load(Record& record);
void flush(); // Dedicated storage task; no control mutex held during flash I/O.
bool pending();
bool failed();
}
