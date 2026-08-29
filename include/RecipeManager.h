#pragma once

#include "BrewRecipe.h"

class RecipeManager {
public:
  static constexpr uint8_t MAX_RECIPES = 12;

  static bool begin();
  static bool isReady();
  static uint8_t count();
  static const BrewRecipe* at(uint8_t index);
  static const BrewRecipe* find(uint32_t id);
  static bool save(BrewRecipe recipe, uint32_t& assignedId);
  static bool erase(uint32_t id);
  static bool activate(uint32_t id);
  static bool restoreActive();
  static uint32_t getActiveId();
  static const char* getActiveName();
  static bool validate(const BrewRecipe& recipe);

private:
  static bool persist();
  static bool load();
  static void apply(const BrewRecipe& recipe);
};
