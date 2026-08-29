#pragma once

#include <Arduino.h>

constexpr uint8_t MAX_HOP_ADDITIONS = 8;
constexpr size_t HOP_NAME_BYTES = 24;
constexpr size_t RECIPE_NAME_BYTES = 32;

struct HopAddition {
  char name[HOP_NAME_BYTES + 1]{};
  uint32_t secondsBeforeBoilEnd = 0;
};

struct BrewRecipe {
  uint32_t id = 0;
  uint32_t revision = 0;
  char name[RECIPE_NAME_BYTES + 1]{};
  uint32_t mashTime = 90UL * 60UL;
  uint32_t mashoutTime = 10UL * 60UL;
  uint32_t boilTime = 60UL * 60UL;
  float mashSetpoint = 64.0f;
  float mashoutSetpoint = 75.0f;
  float hysteresis = 1.0f;
  float valveOffset = 5.0f;
  uint8_t hopCount = 0;
  HopAddition hops[MAX_HOP_ADDITIONS]{};
  bool used = false;
};
