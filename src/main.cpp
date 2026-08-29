#include <Arduino.h>
#include "WiFiHandler.h"
#include "WebServerHandler.h"
#include "TemperatureHandler.h"
#include "ProcessHandler.h"
#include "EEPROMHandler.h"
#include "DisplayHandler.h"
#include "OTAHandler.h"
#include "StatusLED.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "Version.h"
#include "PinConfig.h"
#include "RecipeManager.h"

// =====================================================================================
// PIN-KONFIGURATION (tilpas efter behov)
// =====================================================================================
// Blink-variabler til ventiltemp
unsigned long lastBlinkToggle = 0;
bool blinkState = false;

const unsigned long longPressThreshold = 3000; // 3000 ms = 3 sekunder
unsigned long buttonPressStart = 0;
bool longPressHandled = false;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("==== Opstart af Brygkontroller (HTTP-OTA) ====");

  EEPROMHandler::begin();
  RecipeManager::begin();
  StatusLED::begin(PIN_RGB_LED);
  pinMode(PIN_GAS, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);

  WiFiHandler::begin();
  TemperatureHandler::begin(PIN_TEMP_GRYDE, PIN_TEMP_VENTIL);
  ProcessHandler::begin(PIN_GAS, PIN_PUMP, PIN_BUZZER, PIN_BUTTON);
  RecipeManager::restoreActive();
  OTAHandler::begin();
  WebServerHandler::begin();
  DisplayHandler::begin();

  DisplayHandler::displayBeerAnimation();
  delay(5000);

  if (WiFiHandler::isAPMode()) {
    DisplayHandler::showMessage("AP-mode - 192.168.4.1");
  } else {
    String ipAddress = WiFi.localIP().toString();
    DisplayHandler::showMessage("brygkontrol.local\n" + ipAddress);
  }
  delay(5000);
  Serial.println("==== Opstart gennemført ====");
}

void loop() {
  WebServerHandler::handleClient();
  WiFiHandler::handleWiFi();

  // Service both asynchronous DS18B20 conversions without blocking web/UI work.
  unsigned long now = millis();
  OTAHandler::update(now);
  TemperatureHandler::readTemperatures();
  const bool isGrydeValid = TemperatureHandler::isGrydeValid();
  const bool isVentilValid = TemperatureHandler::isVentilValid();
  const float tGryde = TemperatureHandler::getGrydeTemp();
  const float tVentil = TemperatureHandler::getVentilTemp();

  // Opdater processtyring og display med seneste gyldige temperaturer
  if (!OTAHandler::maintenanceActive()) {
    ProcessHandler::update(tGryde, tVentil);
  }
  auto brewState = ProcessHandler::getCurrentState();
  bool processRunning = (brewState != ProcessHandler::BrewState::IDLE) && (brewState != ProcessHandler::BrewState::PAUSED);

  DisplayHandler::update(
    isGrydeValid ? tGryde : NAN,
    isVentilValid ? tVentil : NAN,
    blinkState,
    ProcessHandler::getProcessStep(),
    ProcessHandler::getRemainingTime()
  );

  StatusLED::setProcessActive(processRunning);
  StatusLED::update();

  if (!OTAHandler::maintenanceActive() && brewState == ProcessHandler::BrewState::IDLE && digitalRead(PIN_BUTTON) == LOW) {
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
      longPressHandled = false;
    } else if (!longPressHandled && (millis() - buttonPressStart >= longPressThreshold)) {
      ProcessHandler::startMashing();
      longPressHandled = true;
      Serial.println("Long press: Start mæsning");
    }
  } else {
    buttonPressStart = 0;
    longPressHandled = false;
  }

  bool pumpOn = ProcessHandler::isPumpOn();
  if (!pumpOn) {
    if (now - lastBlinkToggle >= 500) {
      lastBlinkToggle = now;
      blinkState = !blinkState;
    }
  } else {
    blinkState = true;
  }
}
