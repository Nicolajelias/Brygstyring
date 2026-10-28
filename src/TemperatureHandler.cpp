#include "TemperatureHandler.h"
#include "PinConfig.h"
#include <Arduino.h>
#include <Adafruit_MAX31865.h>
#include <SPI.h>
#include <limits.h>
#include <math.h>

namespace {
constexpr float RTD_NOMINAL_OHMS = 100.0f;
constexpr float REFERENCE_OHMS = 430.0f;
constexpr unsigned long BIAS_SETTLING_MS = 10;
constexpr unsigned long CONVERSION_TIME_MS = 65;
constexpr unsigned long SAMPLE_INTERVAL_MS = 1000;
constexpr unsigned long MAX_READING_AGE_MS = 5000;
constexpr uint8_t REINITIALIZE_AFTER_FAILURES = 3;

struct SensorState {
  Adafruit_MAX31865* converter = nullptr;
  uint8_t chipSelect = 0;
  float lastGoodValue = NAN;
  unsigned long lastGoodMs = 0;
  uint8_t consecutiveFailures = 0;
  bool hasReading = false;
  const char* name = nullptr;
};

enum class ConversionPhase { Idle, BiasSettling, Converting };

SensorState gryde;
SensorState ventil;
ConversionPhase conversionPhase = ConversionPhase::Idle;
unsigned long conversionStartedMs = 0;
unsigned long phaseStartedMs = 0;

bool isValidTemperature(const float value) {
  return isfinite(value) && value >= -20.0f && value <= 125.0f;
}

void configureSensor(SensorState& sensor) {
  if (!sensor.converter) return;
  sensor.converter->begin(MAX31865_3WIRE);
  sensor.converter->enable50Hz(true);
}

void startOneShot(const SensorState& sensor) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(sensor.chipSelect, LOW);
  SPI.transfer(MAX31865_CONFIG_REG | 0x80);
  SPI.transfer(MAX31865_CONFIG_BIAS | MAX31865_CONFIG_1SHOT |
               MAX31865_CONFIG_3WIRE | MAX31865_CONFIG_FILT50HZ);
  digitalWrite(sensor.chipSelect, HIGH);
  SPI.endTransaction();
}

uint16_t readRtdRegister(const SensorState& sensor) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(sensor.chipSelect, LOW);
  SPI.transfer(MAX31865_RTDMSB_REG);
  const uint8_t highByte = SPI.transfer(0xFF);
  const uint8_t lowByte = SPI.transfer(0xFF);
  digitalWrite(sensor.chipSelect, HIGH);
  SPI.endTransaction();
  return (static_cast<uint16_t>(highByte) << 8) | lowByte;
}

void requestConversions() {
  gryde.converter->clearFault();
  ventil.converter->clearFault();
  gryde.converter->enableBias(true);
  ventil.converter->enableBias(true);
  conversionStartedMs = millis();
  phaseStartedMs = conversionStartedMs;
  conversionPhase = ConversionPhase::BiasSettling;
}

void readSensor(SensorState& sensor, const unsigned long now) {
  if (!sensor.converter) return;
  const uint16_t raw = readRtdRegister(sensor);
  const uint8_t fault = sensor.converter->readFault(MAX31865_FAULT_NONE);
  sensor.converter->enableBias(false);
  const float candidate = sensor.converter->calculateTemperature(raw >> 1, RTD_NOMINAL_OHMS, REFERENCE_OHMS);
  if (fault == 0 && (raw & 1) == 0 && isValidTemperature(candidate)) {
    const bool recovered = sensor.consecutiveFailures > 0 || !sensor.hasReading;
    sensor.lastGoodValue = candidate;
    sensor.lastGoodMs = now;
    sensor.consecutiveFailures = 0;
    sensor.hasReading = true;
    if (recovered) {
      Serial.printf("[Temperature] %s recovered: %.2f C\n", sensor.name, candidate);
    }
    return;
  }

  if (sensor.consecutiveFailures < UINT8_MAX) ++sensor.consecutiveFailures;
  Serial.printf("[Temperature] Invalid %s reading (%.2f C, raw 0x%04X, fault 0x%02X), failure %u\n",
                sensor.name, candidate, raw, fault, sensor.consecutiveFailures);
  if (sensor.consecutiveFailures % REINITIALIZE_AFTER_FAILURES == 0) {
    Serial.printf("[Temperature] Reinitializing %s MAX31865\n", sensor.name);
    configureSensor(sensor);
  }
}

bool isUsable(const SensorState& sensor) {
  return sensor.hasReading && (millis() - sensor.lastGoodMs) <= MAX_READING_AGE_MS;
}

unsigned long readingAge(const SensorState& sensor) {
  return sensor.hasReading ? millis() - sensor.lastGoodMs : ULONG_MAX;
}
}  // namespace

void TemperatureHandler::begin(uint8_t grydePin, uint8_t ventilPin) {
  pinMode(grydePin, OUTPUT);
  pinMode(ventilPin, OUTPUT);
  digitalWrite(grydePin, HIGH);
  digitalWrite(ventilPin, HIGH);
  SPI.begin(PIN_TEMP_SCK, PIN_TEMP_MISO, PIN_TEMP_MOSI);

  static Adafruit_MAX31865 grydeConverter(grydePin, &SPI);
  static Adafruit_MAX31865 ventilConverter(ventilPin, &SPI);

  gryde = SensorState{};
  ventil = SensorState{};
  gryde.converter = &grydeConverter;
  gryde.chipSelect = grydePin;
  gryde.name = "pot sensor";
  ventil.converter = &ventilConverter;
  ventil.chipSelect = ventilPin;
  ventil.name = "valve sensor";
  configureSensor(gryde);
  configureSensor(ventil);
  requestConversions();
}

void TemperatureHandler::readTemperatures() {
  if (!gryde.converter || !ventil.converter) return;
  const unsigned long now = millis();
  if (conversionPhase == ConversionPhase::BiasSettling) {
    if (now - phaseStartedMs < BIAS_SETTLING_MS) return;
    startOneShot(gryde);
    startOneShot(ventil);
    phaseStartedMs = millis();
    conversionPhase = ConversionPhase::Converting;
    return;
  }
  if (conversionPhase == ConversionPhase::Converting) {
    if (now - phaseStartedMs < CONVERSION_TIME_MS) return;
    readSensor(gryde, now);
    readSensor(ventil, now);
    conversionPhase = ConversionPhase::Idle;
  }
  if (conversionPhase == ConversionPhase::Idle && now - conversionStartedMs >= SAMPLE_INTERVAL_MS) {
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
