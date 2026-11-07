#include "ControlRuntime.h"
#include "ProcessHandler.h"
#include "TemperatureHandler.h"
#include "StatusLED.h"
#include "PinConfig.h"
#include "RuntimeLog.h"
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace {
StaticSemaphore_t mutexBuffer;
SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutexStatic(&mutexBuffer);
bool inMaintenance = false;
bool watchdogReady = false;
uint32_t cycles = 0, lastTick = 0, maxGap = 0, maxWork = 0;
void controlTask(void*) {
  { ControlRuntime::Guard guard;
    watchdogReady = esp_task_wdt_add(nullptr) == ESP_OK;
    if (!watchdogReady) RuntimeLog.println("[Control] Task watchdog registration failed");
  }
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    const unsigned long started = millis();
    {
      ControlRuntime::Guard guard;
      if (cycles) maxGap = max(maxGap, uint32_t(started - lastTick));
      lastTick = started; ++cycles;
      TemperatureHandler::readTemperatures();
      ProcessHandler::enforceTemperatureSafety();
      if (!inMaintenance) {
        ProcessHandler::update(TemperatureHandler::getGrydeTemp(), TemperatureHandler::getVentilTemp());

      }
      const auto state = ProcessHandler::getCurrentState();
      StatusLED::setProcessActive(state != ProcessHandler::BrewState::IDLE && state != ProcessHandler::BrewState::PAUSED);
      maxWork = max(maxWork, uint32_t(millis() - started));
    }
    if (watchdogReady) esp_task_wdt_reset();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(5));
  }
}
}
ControlRuntime::Guard::Guard() { xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }
ControlRuntime::Guard::~Guard() { xSemaphoreGiveRecursive(mutex); }
bool ControlRuntime::begin() {
  if (esp_task_wdt_init(5, true) != ESP_OK) return false;
  return xTaskCreatePinnedToCore(controlTask, "brew-control", 8192, nullptr, 3, nullptr, 1) == pdPASS;
}
bool ControlRuntime::maintenance() { Guard guard; return inMaintenance; }
bool ControlRuntime::enterMaintenance() {
  Guard guard;
  if (inMaintenance || ProcessHandler::getCurrentState() != ProcessHandler::BrewState::IDLE ||
      ProcessHandler::isGasValveOn() || ProcessHandler::isPumpOn()) return false;
  inMaintenance = true;
  return true;
}
void ControlRuntime::leaveMaintenance() { Guard guard; inMaintenance = false; }
ControlRuntime::Metrics ControlRuntime::metrics() {
  Guard guard;
  return {cycles, maxGap, maxWork, cycles ? uint32_t(millis() - lastTick) : UINT32_MAX, watchdogReady};
}
