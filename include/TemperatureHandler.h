#ifndef TEMPERATURE_HANDLER_H
#define TEMPERATURE_HANDLER_H

#include <Arduino.h>
#include "TemperatureCalibration.h"

class TemperatureHandler {
public:
  struct Diagnostics { uint32_t samples, failures; const char* lastError; };
  static Diagnostics grydeDiagnostics();
  static Diagnostics ventilDiagnostics();
  static void begin(uint8_t grydePin, uint8_t ventilPin);
  static void readTemperatures();
  static float getGrydeTemp();
  static float getVentilTemp();
  static float getGrydeUncalibratedTemp();
  static float getVentilUncalibratedTemp();
  static TemperatureCalibration getGrydeCalibration();
  static TemperatureCalibration getVentilCalibration();
  static bool saveCalibration(const TemperatureCalibration& gryde, const TemperatureCalibration& ventil);
  static bool isGrydeValid();
  static bool isVentilValid();
  static bool isGrydeHealthy();
  static bool isVentilHealthy();
  static uint8_t getGrydeFailureCount();
  static uint8_t getVentilFailureCount();
  static unsigned long getGrydeAgeMs();
  static unsigned long getVentilAgeMs();
};

#endif // TEMPERATURE_HANDLER_H
