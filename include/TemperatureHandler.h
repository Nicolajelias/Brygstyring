#ifndef TEMPERATURE_HANDLER_H
#define TEMPERATURE_HANDLER_H

#include <Arduino.h>
#include <DallasTemperature.h>

class TemperatureHandler {
public:
  static void begin(uint8_t grydePin, uint8_t ventilPin);
  static void readTemperatures();
  static float getGrydeTemp();
  static float getVentilTemp();
  static bool isGrydeValid();
  static bool isVentilValid();
};

#endif // TEMPERATURE_HANDLER_H
