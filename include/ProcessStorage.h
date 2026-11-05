#pragma once
#include <Arduino.h>
namespace ProcessStorage {
struct Record { uint32_t version = 1, epoch = 0; uint8_t state = 0, timer = 0; uint8_t reserved[2] = {}; };
void begin();
void stage(uint32_t epoch, uint8_t state, bool timer);
bool load(Record& record);
void flush(); // Background only, when process and manual outputs are OFF.
}
