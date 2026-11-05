#include "ProcessStorage.h"
#include "ControlRuntime.h"
#include <Preferences.h>
namespace { ProcessStorage::Record cached; bool valid = false, dirty = false; uint32_t revision = 0, lastAttempt = 0; }
void ProcessStorage::begin() {
  Preferences p;
  if (!p.begin("brew-process", true)) return;
  Record loaded{};
  valid = p.getBytesLength("state-v1") == sizeof(loaded) && p.getBytes("state-v1", &loaded, sizeof(loaded)) == sizeof(loaded)
       && loaded.version == 1 && loaded.state <= 5 && loaded.timer <= 1;
  if (valid) cached = loaded;
  p.end();
  // Legacy EEPROM offset 100 overlapped Config.sn; never migrate that record.
}
void ProcessStorage::stage(uint32_t epoch, uint8_t state, bool timer) {
  ControlRuntime::Guard guard;
  cached = Record{}; cached.epoch = epoch; cached.state = state; cached.timer = timer;
  valid = true; dirty = true; ++revision;
}
bool ProcessStorage::load(Record& record) { ControlRuntime::Guard guard; record = cached; return valid; }
void ProcessStorage::flush() {
  if (millis() - lastAttempt < 1000) return;
  lastAttempt = millis();
  ControlRuntime::Maintenance safe;
  if (!safe) return;
  Record pending; uint32_t saving;
  { ControlRuntime::Guard guard; if (!dirty) return; pending = cached; saving = revision; }
  Preferences p;
  if (!p.begin("brew-process", false)) return;
  const bool ok = p.putBytes("state-v1", &pending, sizeof(pending)) == sizeof(pending);
  p.end();
  if (ok) { ControlRuntime::Guard guard; if (revision == saving) dirty = false; }
}
