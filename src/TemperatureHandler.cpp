#include "TemperatureHandler.h"
#include <OneWire.h>
#include <Arduino.h>
#include <DallasTemperature.h>
#include <math.h>

namespace {
constexpr uint8_t SENSOR_RESOLUTION_BITS = 12;
constexpr unsigned long CONVERSION_TIME_MS = 750;
constexpr unsigned long SAMPLE_INTERVAL_MS = 1000;
constexpr unsigned long MAX_READING_AGE_MS = 5000;
constexpr uint8_t REINITIALIZE_AFTER_FAILURES = 3;

struct SensorState {
  DallasTemperature* bus = nullptr;
  float lastGoodValue = NAN;
  unsigned long lastGoodMs = 0;
  uint8_t consecutiveFailures = 0;
  bool detected = false;
  const char* name = nullptr;
};

SensorState gryde;
SensorState ventil;
bool conversionPending = false;
unsigned long conversionStartedMs = 0;

bool isValidTemperature(const float value, const bool hasPreviousValue) {
  if (!isfinite(value) || value == DEVICE_DISCONNECTED_C || value < -20.0f || value > 125.0f) {
    return false;
  }
  // 85 C is the DS18B20 power-on register value. It is accepted after the
  // first real conversion because it can be a legitimate brewing temperature.
  return hasPreviousValue || fabsf(value - 85.0f) > 0.01f;
}

void configureSensor(SensorState& sensor) {
  if (!sensor.bus) return;
  sensor.bus->begin();
  sensor.bus->setWaitForConversion(false);
  sensor.bus->setCheckForConversion(false);
  sensor.bus->setResolution(SENSOR_RESOLUTION_BITS);
  sensor.detected = sensor.bus->getDeviceCount() > 0;
}

void requestConversions() {
  if (gryde.bus) gryde.bus->requestTemperatures();
  if (ventil.bus) ventil.bus->requestTemperatures();
  conversionStartedMs = millis();
  conversionPending = true;
}

void readSensor(SensorState& sensor, const unsigned long now) {
  if (!sensor.bus) return;
  const float candidate = sensor.bus->getTempCByIndex(0);
  if (isValidTemperature(candidate, sensor.lastGoodMs != 0)) {
    const bool recovered = sensor.consecutiveFailures > 0 || !sensor.detected;
    sensor.lastGoodValue = candidate;
    sensor.lastGoodMs = now;
    sensor.consecutiveFailures = 0;
    sensor.detected = true;
    if (recovered) {
      Serial.printf("[Temperature] %s recovered: %.2f C\n", sensor.name, candidate);
    }
    return;
  }

  if (sensor.consecutiveFailures < UINT8_MAX) ++sensor.consecutiveFailures;
  Serial.printf("[Temperature] Invalid %s reading (%.2f C), failure %u\n",
                sensor.name, candidate, sensor.consecutiveFailures);
  if (sensor.consecutiveFailures % REINITIALIZE_AFTER_FAILURES == 0) {
    Serial.printf("[Temperature] Reinitializing %s OneWire bus\n", sensor.name);
    configureSensor(sensor);
  }
}

bool isUsable(const SensorState& sensor) {
  return sensor.lastGoodMs != 0 && (millis() - sensor.lastGoodMs) <= MAX_READING_AGE_MS;
}

unsigned long readingAge(const SensorState& sensor) {
  return sensor.lastGoodMs == 0 ? ULONG_MAX : millis() - sensor.lastGoodMs;
}
}  // namespace

void TemperatureHandler::begin(uint8_t grydePin, uint8_t ventilPin) {
  pinMode(grydePin, INPUT_PULLUP);
  pinMode(ventilPin, INPUT_PULLUP);

  static OneWire grydeWire(grydePin);
  static OneWire ventilWire(ventilPin);
  static DallasTemperature grydeDallas(&grydeWire);
  static DallasTemperature ventilDallas(&ventilWire);

  gryde.bus = &grydeDallas;
  gryde.name = "pot sensor";
  ventil.bus = &ventilDallas;
  ventil.name = "valve sensor";
  configureSensor(gryde);
  configureSensor(ventil);
  requestConversions();
}

void TemperatureHandler::readTemperatures() {
  const unsigned long now = millis();
  if (conversionPending) {
    if (now - conversionStartedMs < CONVERSION_TIME_MS) return;
    readSensor(gryde, now);
    readSensor(ventil, now);
    conversionPending = false;
  }
  if (!conversionPending && now - conversionStartedMs >= SAMPLE_INTERVAL_MS) {
    requestConversions();
  }
}

float TemperatureHandler::getGrydeTemp() { return isUsable(gryde) ? gryde.lastGoodValue : NAN; }
float TemperatureHandler::getVentilTemp() { return isUsable(ventil) ? ventil.lastGoodValue : NAN; }
bool TemperatureHandler::isGrydeValid() { return isUsable(gryde); }
bool TemperatureHandler::isVentilValid() { return isUsable(ventil); }
bool TemperatureHandler::isGrydeHealthy() { return isUsable(gryde) && gryde.consecutiveFailures == 0; }
bool TemperatureHandler::isVentilHealthy() { return isUsable(ventil) && ventil.consecutiveFailures == 0; }
uint8_t TemperatureHandler::getGrydeFailureCount() { return gryde.consecutiveFailures; }
uint8_t TemperatureHandler::getVentilFailureCount() { return ventil.consecutiveFailures; }
unsigned long TemperatureHandler::getGrydeAgeMs() { return readingAge(gryde); }
unsigned long TemperatureHandler::getVentilAgeMs() { return readingAge(ventil); }
