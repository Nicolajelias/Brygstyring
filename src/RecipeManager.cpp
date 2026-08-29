#include "RecipeManager.h"
#include "EEPROMHandler.h"
#include "ProcessHandler.h"
#include <SPIFFS.h>
#include <algorithm>
#include <math.h>
#include <string.h>

namespace {
constexpr uint32_t MAGIC = 0x43525742UL;  // "BWRC"
constexpr uint16_t SCHEMA_VERSION = 1;
constexpr char STORE_PATH[] = "/recipes.db";
constexpr char TEMP_PATH[] = "/recipes.tmp";
constexpr char BACKUP_PATH[] = "/recipes.bak";
constexpr char CORRUPT_PATH[] = "/recipes.corrupt";

#pragma pack(push, 1)
struct StoreImage {
  uint32_t magic = MAGIC;
  uint16_t schema = SCHEMA_VERSION;
  uint16_t reserved = 0;
  uint32_t nextId = 1;
  uint32_t activeId = 0;
  BrewRecipe records[RecipeManager::MAX_RECIPES]{};
  uint32_t crc = 0;
};
#pragma pack(pop)

StoreImage store;
bool ready = false;

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1U) ^ (0xEDB88320U & static_cast<uint32_t>(-(crc & 1U)));
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

bool validText(const char* text, size_t capacity) {
  const void* end = memchr(text, '\0', capacity);
  if (!end) return false;
  const size_t length = strlen(text);
  if (length == 0 || text[0] == ' ' || text[length - 1] == ' ') return false;
  for (size_t index = 0; index < length; ++index) {
    if (static_cast<uint8_t>(text[index]) < 0x20U) return false;
  }
  return true;
}

bool readImage(const char* path, StoreImage& destination) {
  File file = SPIFFS.open(path, FILE_READ);
  if (!file || file.size() != sizeof(StoreImage) ||
      file.read(reinterpret_cast<uint8_t*>(&destination), sizeof(destination)) != sizeof(destination)) {
    file.close();
    return false;
  }
  file.close();
  const uint32_t expected = destination.crc;
  destination.crc = 0;
  const bool headerValid = destination.magic == MAGIC && destination.schema == SCHEMA_VERSION;
  const bool crcValid = crc32(reinterpret_cast<const uint8_t*>(&destination), sizeof(destination)) == expected;
  destination.crc = expected;
  if (!headerValid || !crcValid || destination.nextId == 0) return false;
  uint32_t highestId = 0;
  for (uint8_t index = 0; index < RecipeManager::MAX_RECIPES; ++index) {
    const BrewRecipe& recipe = destination.records[index];
    if (!recipe.used) continue;
    if (!RecipeManager::validate(recipe) || recipe.id == 0 || recipe.revision == 0) return false;
    for (uint8_t other = index + 1; other < RecipeManager::MAX_RECIPES; ++other) {
      if (destination.records[other].used && destination.records[other].id == recipe.id) return false;
    }
    highestId = std::max(highestId, recipe.id);
  }
  if (destination.nextId <= highestId) return false;
  if (destination.activeId != 0) {
    bool found = false;
    for (const BrewRecipe& recipe : destination.records) found |= recipe.used && recipe.id == destination.activeId;
    if (!found) destination.activeId = 0;
  }
  return true;
}
}  // namespace

bool RecipeManager::begin() {
  if (!SPIFFS.begin(false)) {
    Serial.println("[Recipes] SPIFFS mount failed; formatting partition");
    if (!SPIFFS.format() || !SPIFFS.begin(false)) return false;
  }
  ready = load();
  if (!ready) {
    store = {};
    ready = persist();
  }
  Serial.printf("[Recipes] Store %s, %u recipe(s)\n", ready ? "ready" : "failed", count());
  return ready;
}

bool RecipeManager::load() {
  StoreImage candidate{};
  if (readImage(STORE_PATH, candidate)) { store = candidate; return true; }
  if (readImage(BACKUP_PATH, candidate)) {
    store = candidate;
    SPIFFS.remove(STORE_PATH);
    SPIFFS.rename(BACKUP_PATH, STORE_PATH);
    return true;
  }
  if (SPIFFS.exists(STORE_PATH)) {
    SPIFFS.remove(CORRUPT_PATH);
    SPIFFS.rename(STORE_PATH, CORRUPT_PATH);
  }
  return false;
}

bool RecipeManager::persist() {
  store.magic = MAGIC;
  store.schema = SCHEMA_VERSION;
  store.crc = 0;
  store.crc = crc32(reinterpret_cast<const uint8_t*>(&store), sizeof(store));
  SPIFFS.remove(TEMP_PATH);
  File file = SPIFFS.open(TEMP_PATH, FILE_WRITE);
  if (!file || file.write(reinterpret_cast<const uint8_t*>(&store), sizeof(store)) != sizeof(store)) {
    file.close(); SPIFFS.remove(TEMP_PATH); return false;
  }
  file.flush(); file.close();
  StoreImage verified{};
  if (!readImage(TEMP_PATH, verified)) { SPIFFS.remove(TEMP_PATH); return false; }
  SPIFFS.remove(BACKUP_PATH);
  const bool hadCurrent = SPIFFS.exists(STORE_PATH);
  if (hadCurrent && !SPIFFS.rename(STORE_PATH, BACKUP_PATH)) { SPIFFS.remove(TEMP_PATH); return false; }
  if (!SPIFFS.rename(TEMP_PATH, STORE_PATH)) {
    if (hadCurrent) SPIFFS.rename(BACKUP_PATH, STORE_PATH);
    return false;
  }
  SPIFFS.remove(BACKUP_PATH);
  return true;
}

bool RecipeManager::isReady() { return ready; }

uint8_t RecipeManager::count() {
  uint8_t result = 0;
  for (const BrewRecipe& recipe : store.records) result += recipe.used ? 1 : 0;
  return result;
}

const BrewRecipe* RecipeManager::at(uint8_t requested) {
  for (const BrewRecipe& recipe : store.records) {
    if (recipe.used && requested-- == 0) return &recipe;
  }
  return nullptr;
}

const BrewRecipe* RecipeManager::find(uint32_t id) {
  for (const BrewRecipe& recipe : store.records) if (recipe.used && recipe.id == id) return &recipe;
  return nullptr;
}

bool RecipeManager::validate(const BrewRecipe& recipe) {
  if (!validText(recipe.name, sizeof(recipe.name)) || recipe.hopCount > MAX_HOP_ADDITIONS ||
      recipe.mashTime < 60 || recipe.mashTime > 300UL * 60UL ||
      recipe.mashoutTime < 60 || recipe.mashoutTime > 120UL * 60UL ||
      recipe.boilTime < 60 || recipe.boilTime > 300UL * 60UL ||
      !isfinite(recipe.mashSetpoint) || recipe.mashSetpoint < 20 || recipe.mashSetpoint > 100 ||
      !isfinite(recipe.mashoutSetpoint) || recipe.mashoutSetpoint < 20 || recipe.mashoutSetpoint > 100 ||
      !isfinite(recipe.hysteresis) || recipe.hysteresis < 0.1f || recipe.hysteresis > 10 ||
      !isfinite(recipe.valveOffset) || recipe.valveOffset < 0 || recipe.valveOffset > 30) return false;
  uint32_t previous = UINT32_MAX;
  for (uint8_t index = 0; index < recipe.hopCount; ++index) {
    const HopAddition& hop = recipe.hops[index];
    if (!validText(hop.name, sizeof(hop.name)) || hop.secondsBeforeBoilEnd > recipe.boilTime ||
        hop.secondsBeforeBoilEnd > previous) return false;
    previous = hop.secondsBeforeBoilEnd;
  }
  return true;
}

bool RecipeManager::save(BrewRecipe recipe, uint32_t& assignedId) {
  if (!ready || !validate(recipe)) return false;
  BrewRecipe* destination = nullptr;
  if (recipe.id != 0) {
    for (BrewRecipe& item : store.records) if (item.used && item.id == recipe.id) destination = &item;
    if (!destination) return false;
    recipe.revision = destination->revision + 1;
  } else {
    for (BrewRecipe& item : store.records) if (!item.used) { destination = &item; break; }
    if (!destination || store.nextId == 0) return false;
    recipe.id = store.nextId++;
    recipe.revision = 1;
  }
  recipe.used = true;
  *destination = recipe;
  assignedId = recipe.id;
  if (!persist()) { load(); return false; }
  return true;
}

bool RecipeManager::erase(uint32_t id) {
  if (!ready) return false;
  for (BrewRecipe& recipe : store.records) {
    if (recipe.used && recipe.id == id) {
      recipe = {};
      if (store.activeId == id) store.activeId = 0;
      if (!persist()) { load(); return false; }
      if (store.activeId == 0) ProcessHandler::setHopSchedule(nullptr, 0);
      return true;
    }
  }
  return false;
}

void RecipeManager::apply(const BrewRecipe& recipe) {
  Config config = EEPROMHandler::getConfig();
  config.mashTime = recipe.mashTime; config.mashoutTime = recipe.mashoutTime; config.boilTime = recipe.boilTime;
  config.mashSetpoint = recipe.mashSetpoint; config.mashoutSetpoint = recipe.mashoutSetpoint;
  config.hysteresis = recipe.hysteresis; config.tempOffset = recipe.valveOffset;
  EEPROMHandler::saveConfig(config);
  ProcessHandler::setMashTime(recipe.mashTime); ProcessHandler::setMashoutTime(recipe.mashoutTime);
  ProcessHandler::setBoilTime(recipe.boilTime); ProcessHandler::setMashSetpoint(recipe.mashSetpoint);
  ProcessHandler::setMashoutSetpoint(recipe.mashoutSetpoint); ProcessHandler::setHysteresis(recipe.hysteresis);
  ProcessHandler::setValveOffset(recipe.valveOffset); ProcessHandler::setHopSchedule(recipe.hops, recipe.hopCount);
}

bool RecipeManager::activate(uint32_t id) {
  const BrewRecipe* recipe = find(id);
  if (!recipe) return false;
  store.activeId = id;
  apply(*recipe);
  return persist();
}

bool RecipeManager::restoreActive() {
  const BrewRecipe* recipe = find(store.activeId);
  if (!recipe) return false;
  apply(*recipe);
  return true;
}

uint32_t RecipeManager::getActiveId() { return store.activeId; }
const char* RecipeManager::getActiveName() {
  const BrewRecipe* recipe = find(store.activeId);
  return recipe ? recipe->name : "Ingen valgt";
}
