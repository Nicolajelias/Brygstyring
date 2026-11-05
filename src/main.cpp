#include <Arduino.h>
#include <esp_system.h>
#include <WiFi.h>
#include "WiFiHandler.h"
#include "WebServerHandler.h"
#include "TemperatureHandler.h"
#include "ProcessHandler.h"
#include "EEPROMHandler.h"
#include "DisplayHandler.h"
#include "OTAHandler.h"
#include "StatusLED.h"
#include "PinConfig.h"
#include "RecipeManager.h"
#include "ControlRuntime.h"
#include "ClockService.h"
#include "ProcessStorage.h"
#include "RuntimeLog.h"
void setup() {
  for (int pin : {PIN_GAS, PIN_PUMP, PIN_BUZZER}) {
    digitalWrite(pin, LOW); pinMode(pin, OUTPUT);
  }
  Serial.begin(115200);
  RuntimeLog.begin();
  Serial.println("==== Brygkontroller: independent control task ====");
  Serial.printf("[Boot] Reset reason: %d\n", static_cast<int>(esp_reset_reason()));
  EEPROMHandler::begin();
  ProcessStorage::begin();
  ProcessHandler::begin(PIN_GAS, PIN_PUMP, PIN_BUZZER, PIN_BUTTON);
  TemperatureHandler::begin(PIN_TEMP_GRYDE, PIN_TEMP_VENTIL);
  StatusLED::begin(PIN_RGB_LED);
  ControlRuntime::Maintenance startup;
  if (!startup || !ControlRuntime::begin()) {
    Serial.println("FATAL: control task could not start; outputs remain OFF");
    for (;;) delay(1000);
  }
  RecipeManager::begin(); RecipeManager::restoreActive();
  WiFiHandler::begin();
  ClockService::begin();
  OTAHandler::begin();
  WebServerHandler::begin();
  DisplayHandler::begin();
  StatusLED::notify(); // Brief visual check when startup has completed.
  Serial.println("==== Opstart gennemfoert ====");
}
void loop() {
  WiFiHandler::handleWiFi();
  WebServerHandler::handleClient();
  OTAHandler::update(millis());
  ProcessStorage::flush();
  DisplayHandler::update(TemperatureHandler::getGrydeTemp(), TemperatureHandler::getVentilTemp(),
    ProcessHandler::isPumpOn() || (millis() / 500 % 2),
    ProcessHandler::getProcessStep(), ProcessHandler::getRemainingTime());
  RuntimeLog.drain();
  static unsigned long lastDiagnostics = 0;
  if (millis() - lastDiagnostics >= 10000) {
    lastDiagnostics = millis();
    const auto timing = ControlRuntime::metrics();
    Serial.printf("[Runtime] cycles=%lu maxGap=%lu ms maxWork=%lu ms watchdog=%d pot=%.2f/%lu ms valve=%.2f/%lu ms wifi=%d rssi=%d\n",
      static_cast<unsigned long>(timing.cycles), static_cast<unsigned long>(timing.maxGapMs),
      static_cast<unsigned long>(timing.maxWorkMs), timing.watchdog,
      TemperatureHandler::getGrydeTemp(), TemperatureHandler::getGrydeAgeMs(),
      TemperatureHandler::getVentilTemp(), TemperatureHandler::getVentilAgeMs(), WiFi.status(), WiFi.RSSI());
  }
  delay(1); // UI/network task only; control has its own 5 ms schedule.
}
