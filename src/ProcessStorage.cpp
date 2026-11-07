#include "ProcessStorage.h"
#include "ControlRuntime.h"
#include <Preferences.h>
#include <stddef.h>
#include <math.h>
namespace {
ProcessStorage::Record cached;
bool valid = false, dirty = false, error = false;
uint32_t revision = 0, lastAttempt = 0;
uint32_t checksum(const ProcessStorage::Record& r) {
  uint32_t hash = 2166136261UL;
  const auto* data = reinterpret_cast<const uint8_t*>(&r);
  for (size_t i=0; i<offsetof(ProcessStorage::Record, checksum); ++i) hash = (hash ^ data[i]) * 16777619UL;
  return hash;
}
bool sane(const ProcessStorage::Record& r) {
  const auto& p = r.recipe;
  if (r.version != 2 || r.checksum != checksum(r) || r.state > 5 || r.timer > 1 ||
      r.previous > 4 || r.completed > 1 || r.expired > 1 || r.elapsed > 86400 ||
      (r.state == 5 && r.previous == 0) || p.hopCount > MAX_HOP_ADDITIONS ||
      r.activeHop < -1 || r.activeHop >= static_cast<int>(p.hopCount)) return false;
  if (p.mashTime < 60 || p.mashTime > 18000 || p.mashoutTime < 60 || p.mashoutTime > 18000 ||
      p.boilTime < 60 || p.boilTime > 18000 || !isfinite(p.mashSetpoint) || !isfinite(p.mashoutSetpoint) ||
      p.mashSetpoint < 20 || p.mashSetpoint > 100 || p.mashoutSetpoint < 20 || p.mashoutSetpoint > 100 ||
      !isfinite(p.hysteresis) || p.hysteresis < 0 || p.hysteresis > 10 ||
      !isfinite(p.valveOffset) || p.valveOffset < 0 || p.valveOffset > 30) return false;
  for (unsigned i=0; i<p.hopCount; ++i)
    if (p.hops[i].name[HOP_NAME_BYTES] != 0 || p.hops[i].secondsBeforeBoilEnd > p.boilTime) return false;
  return true;
}
}
void ProcessStorage::begin() {
  valid = dirty = error = false; revision = lastAttempt = 0;
  Preferences p;
  if (!p.begin("brew-process", true)) return;
  Record loaded{};
  valid = p.getBytesLength("state-v2") == sizeof(loaded) &&
    p.getBytes("state-v2", &loaded, sizeof(loaded)) == sizeof(loaded) && sane(loaded);
  if (valid) cached = loaded;
  p.end();
}
void ProcessStorage::stage(const Record& record) {
  ControlRuntime::Guard guard;
  cached = record; cached.checksum = checksum(cached);
  valid = true; dirty = true; ++revision;
}
bool ProcessStorage::load(Record& record) { ControlRuntime::Guard guard; record = cached; return valid; }
bool ProcessStorage::pending() { ControlRuntime::Guard guard; return dirty; }
bool ProcessStorage::failed() { ControlRuntime::Guard guard; return error; }
void ProcessStorage::flush() {
  Record record; uint32_t saving;
  { ControlRuntime::Guard guard;
    if (!dirty || (error && millis()-lastAttempt < 1000)) return;
    record = cached; saving = revision; lastAttempt = millis();
  }
  Preferences p;
  bool ok = p.begin("brew-process", false);
  if (ok) { ok = p.putBytes("state-v2", &record, sizeof(record)) == sizeof(record); p.end(); }
  { ControlRuntime::Guard guard; error = !ok; if (ok && saving == revision) dirty = false; }
}
