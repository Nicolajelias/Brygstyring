#include "ClockService.h"
#include <time.h>
#include <atomic>
#include <esp_sntp.h>
namespace { std::atomic<bool> synced{false}; }
void ClockService::begin() {
  sntp_set_time_sync_notification_cb([](struct timeval*) { synced.store(true); });
  // Starts background SNTP; never wait for a response in control or UI code.
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
}
bool ClockService::synchronized() { return synced.load(); }
unsigned long ClockService::epoch() {
  if (!synchronized()) return 0;
  // Preserve the project's existing fixed UTC+1 display convention.
  return static_cast<unsigned long>(time(nullptr)) + 3600UL;
}
