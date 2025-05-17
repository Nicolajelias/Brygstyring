#include <Arduino.h>
#include "WiFiHandler.h"
#include "WebServerHandler.h"
#include "TemperatureHandler.h"
#include "ProcessHandler.h"
#include "EEPROMHandler.h"
#include "DisplayHandler.h"
#include "OTAHandler.h"
#include <ESP8266mDNS.h>
#include "Version.h"


// =====================================================================================
// PIN-KONFIGURATION (tilpas efter behov)
// =====================================================================================
static const uint8_t PIN_SENSOR = D4;   // DS18B20 sensorer
static const uint8_t PIN_GAS    = D6;   // Relæ til gas
static const uint8_t PIN_PUMP   = D5;   // Relæ til pumpe
static const uint8_t PIN_BUZZER = D7;   // Buzzer
static const uint8_t PIN_BUTTON = D3;   // Knap til bekræftelser

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

  // Init EEPROM (for at hente gemt konfiguration, eller lave defaults)
  EEPROMHandler::begin(); 
    
  // Opsæt pins
  pinMode(PIN_GAS, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);

  // Initialiser WiFi
  WiFiHandler::begin();

  // Initialiser WebServer
  WebServerHandler::begin();

  // Initialiser TemperatureHandler
  TemperatureHandler::begin(PIN_SENSOR);

  // Initialiser ProcessHandler (fjern hvis ikke nødvendig)
  ProcessHandler::begin(PIN_GAS, PIN_PUMP, PIN_BUZZER, PIN_BUTTON);

  // Initialiser DisplayHandler
  DisplayHandler::begin();

  // Vis splash screen
  DisplayHandler::displayBeerAnimation();
  delay(5000);
  
  // Vis besked på displayet afhængigt af WiFi-tilstanden
  if (WiFiHandler::isAPMode()) {
    DisplayHandler::showMessage("AP-mode - 192.168.4.1");
  } else {
    String ipAddress = WiFi.localIP().toString();
    DisplayHandler::showMessage("brygkontrol.local\n" + ipAddress);
  }
  
  // Vent i 5 sekunder
  delay(5000);

  Serial.println("==== Opstart gennemført ====");
}

void loop() {
  // Håndtér webserver
  WebServerHandler::handleClient();

  // Tjek WiFi
  WiFiHandler::handleWiFi();

  MDNS.update();

  // Læs temperaturer
  TemperatureHandler::readTemperatures();
  float tGryde  = TemperatureHandler::getGrydeTemp();
  float tVentil = TemperatureHandler::getVentilTemp();

  // Opdater processtyring (fx bang-bang regulering, tidsstyring)
  ProcessHandler::update(tGryde, tVentil);
  // Opdater display
  DisplayHandler::update(tGryde, tVentil, blinkState, ProcessHandler::getProcessStep(), ProcessHandler::getRemainingTime());

  // Long press detektion for knappen:
  if (digitalRead(PIN_BUTTON) == LOW) {
    // Knappen er trykket
    if (buttonPressStart == 0) {
      buttonPressStart = millis();  // Start tid
      longPressHandled = false;
    } else if (!longPressHandled && (millis() - buttonPressStart >= longPressThreshold)) {
      // Long press opdaget – start mæsning
      ProcessHandler::startMashing();
      longPressHandled = true;  // Undgå gentagen opstart
      Serial.println("Long press: Start mæsning");
    }
  } else {
    // Knappen er ikke trykket, nulstil starttid
    buttonPressStart = 0;
    longPressHandled = false;
  }

  // Blink-logik for TVentil, når pumpe er slukket
  bool pumpOn = ProcessHandler::isPumpOn();
  unsigned long now = millis();
  if (!pumpOn) {
    if (now - lastBlinkToggle >= 500) {
      lastBlinkToggle = now;
      blinkState = !blinkState;
    }
  } else {
    blinkState = true; // Konstant visning, hvis pumpen er tændt
  }

  }
