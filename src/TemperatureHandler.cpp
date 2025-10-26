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
}

void TemperatureHandler::readTemperatures() {
  grydeTempValid = false;
  ventilTempValid = false;

  if (grydeSensor) {
    grydeSensor->requestTemperatures();
    float temp = grydeSensor->getTempCByIndex(0);
    if (isValidTemperature(temp)) {
      grydeTemp = temp;
      grydeTempValid = true;
    } else {
      Serial.println("Fejl: Ugyldig gryde-temperatur");
    }
  }

  if (ventilSensor) {
    ventilSensor->requestTemperatures();
    float temp = ventilSensor->getTempCByIndex(0);
    if (isValidTemperature(temp)) {
      ventilTemp = temp;
      ventilTempValid = true;
    } else {
      Serial.println("Fejl: Ugyldig ventil-temperatur");
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
