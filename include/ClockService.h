#pragma once
#include <Arduino.h>
namespace ClockService {
void begin();
bool synchronized();
unsigned long epoch();
}
