#include <Arduino.h>
#include "WiFiHandler.h"
#include "WebServerHandler.h"
#include "TemperatureHandler.h"
#include "ProcessHandler.h"
#include "EEPROMHandler.h"
#include "DisplayHandler.h"
#include "OTAHandler.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "Version.h"
#include "PinConfig.h"

// =====================================================================================
// PIN-KONFIGURATION (tilpas efter behov)
// =====================================================================================
// Blink-variabler til ventiltemp
unsigned long lastBlinkToggle = 0;
bool blinkState = false;

const unsigned long longPressThreshold = 3000; // 3000 ms = 3 sekunder
unsigned long buttonPressStart = 0;
bool longPressHandled = false;

unsigned long lastTemperatureRead = 0;
const unsigned long temperatureInterval = 1000; // 1 sekund

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("==== Opstart af Brygkontroller (HTTP-OTA) ====");

  EEPROMHandler::begin(); 
  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_GAS, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  WiFiHandler::begin();
  WebServerHandler::begin();
  TemperatureHandler::begin(PIN_TEMP_GRYDE, PIN_TEMP_VENTIL);
  ProcessHandler::begin(PIN_GAS, PIN_PUMP, PIN_BUZZER, PIN_BUTTON);
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

  // Opdater temperatur hvert sekund
  unsigned long now = millis();
  static float tGryde = NAN;
  static float tVentil = NAN;
  static bool isGrydeValid = false;
  static bool isVentilValid = false;

  if (now - lastTemperatureRead >= temperatureInterval) {
    lastTemperatureRead = now;
    TemperatureHandler::readTemperatures();

    isGrydeValid = TemperatureHandler::isGrydeValid();
    isVentilValid = TemperatureHandler::isVentilValid();

    if (isGrydeValid) {
      tGryde = TemperatureHandler::getGrydeTemp();
    }
    if (isVentilValid) {
      tVentil = TemperatureHandler::getVentilTemp();
    }
  }

  // Opdater processtyring og display med seneste gyldige temperaturer
  ProcessHandler::update(tGryde, tVentil);
  DisplayHandler::update(
    isGrydeValid ? tGryde : NAN,
    isVentilValid ? tVentil : NAN,
    blinkState,
    ProcessHandler::getProcessStep(),
    ProcessHandler::getRemainingTime()
  );

  if (digitalRead(PIN_BUTTON) == LOW) {
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
