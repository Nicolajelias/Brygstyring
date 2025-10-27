#include "TemperatureHandler.h"
#include <OneWire.h>
#include <Arduino.h>
#include <DallasTemperature.h>

namespace {
  DallasTemperature* grydeSensor = nullptr;
  DallasTemperature* ventilSensor = nullptr;

  float grydeTemp = NAN;
  float ventilTemp = NAN;
  bool grydeTempValid = false;
  bool ventilTempValid = false;

  bool isValidTemperature(float temp) {
    return temp != DEVICE_DISCONNECTED_C && temp > -50.0f && temp < 150.0f;
  }
}

void TemperatureHandler::begin(uint8_t grydePin, uint8_t ventilPin) {
  pinMode(grydePin, INPUT_PULLUP);
  pinMode(ventilPin, INPUT_PULLUP);

  static OneWire grydeWire(grydePin);
  static OneWire ventilWire(ventilPin);
  static DallasTemperature grydeDallas(&grydeWire);
  static DallasTemperature ventilDallas(&ventilWire);

  grydeSensor = &grydeDallas;
  ventilSensor = &ventilDallas;

  grydeSensor->begin();
  ventilSensor->begin();

  grydeSensor->setWaitForConversion(true);
  ventilSensor->setWaitForConversion(true);
  grydeSensor->setResolution(12);
  ventilSensor->setResolution(12);
}

void TemperatureHandler::readTemperatures() {
  grydeTempValid = false;
  ventilTempValid = false;

  if (grydeSensor) {
    auto request = grydeSensor->requestTemperatures();
    if (!request) {
      Serial.println("Fejl: Gryde-sensor svarede ikke på request");
    } else {
      float temp = grydeSensor->getTempCByIndex(0);
      if (isValidTemperature(temp)) {
        grydeTemp = temp;
        grydeTempValid = true;
      } else {
        Serial.println("Fejl: Ugyldig gryde-temperatur");
      }
    }
  }

  if (ventilSensor) {
    auto request = ventilSensor->requestTemperatures();
    if (!request) {
      Serial.println("Fejl: Ventil-sensor svarede ikke på request");
    } else {
      float temp = ventilSensor->getTempCByIndex(0);
      if (isValidTemperature(temp)) {
        ventilTemp = temp;
        ventilTempValid = true;
      } else {
        Serial.println("Fejl: Ugyldig ventil-temperatur");
      }
    }
  }
}

float TemperatureHandler::getGrydeTemp() {
  return grydeTempValid ? grydeTemp : NAN;
}

float TemperatureHandler::getVentilTemp() {
  return ventilTempValid ? ventilTemp : NAN;
}

bool TemperatureHandler::isGrydeValid() {
  return grydeTempValid;
}

bool TemperatureHandler::isVentilValid() {
  return ventilTempValid;
}
