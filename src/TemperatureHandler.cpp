#include "TemperatureHandler.h"
#include "PinConfig.h"
#include <Adafruit_MAX31865.h>
#include <Preferences.h>
#include <SPI.h>
#include <limits.h>
#include <math.h>

namespace {
constexpr float RTD_NOMINAL_OHMS = 100.0f;
constexpr float REFERENCE_OHMS = 430.0f;
constexpr unsigned long SETTLING_MS = 12;
constexpr unsigned long CONVERSION_MIN_MS = 70;  // datasheet max: 66 ms at 50 Hz
constexpr unsigned long CONVERSION_TIMEOUT_MS = 110;
constexpr unsigned long FAULT_TIMEOUT_MS = 20;
constexpr unsigned long SAMPLE_INTERVAL_MS = 1000;
constexpr unsigned long MAX_READING_AGE_MS = 5000;
constexpr uint8_t BASE_CONFIG = MAX31865_CONFIG_3WIRE | MAX31865_CONFIG_FILT50HZ;
constexpr uint8_t BIASED_CONFIG = BASE_CONFIG | MAX31865_CONFIG_BIAS;
constexpr uint8_t FAULT_MANUAL_RUN = 0x08;
constexpr uint8_t FAULT_MANUAL_FINISH = 0x0C;
// Broad electrical window (50..200 ohm); software additionally checks -20..125 C.
constexpr uint16_t LOW_THRESHOLD = 7620;
constexpr uint16_t HIGH_THRESHOLD = 30480;

enum class Phase { Idle, BiasSettling, FaultOpen, FaultFinishing, InputSettling, Converting };
struct SensorState {
  Adafruit_MAX31865* converter = nullptr;
  uint8_t chipSelect = 0;
  float lastGoodValue = NAN;
  float lastGoodUncalibrated = NAN;
  unsigned long lastGoodMs = 0;
  unsigned long cycleStartedMs = 0;
  unsigned long phaseStartedMs = 0;
  uint8_t consecutiveFailures = 0;
  uint8_t failuresSinceConfigure = 0;
  bool hasReading = false;
  Phase phase = Phase::Idle;
  TemperatureCalibration calibration;
  const char* name = nullptr;
};
struct CalibrationRecord {
  uint32_t version = 1;
  TemperatureCalibration gryde;
  TemperatureCalibration ventil;
};
SensorState gryde;
SensorState ventil;
constexpr char CALIBRATION_NAMESPACE[] = "pt100-cal";
constexpr char CALIBRATION_KEY[] = "channels";

uint16_t readRegister(const SensorState& sensor, uint8_t address, bool twoBytes = false) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(sensor.chipSelect, LOW);
  SPI.transfer(address);
  uint16_t result = SPI.transfer(0xFF);
  if (twoBytes) result = (result << 8) | SPI.transfer(0xFF);
  digitalWrite(sensor.chipSelect, HIGH);
  SPI.endTransaction();
  return result;
}
void writeConfig(const SensorState& sensor, uint8_t value) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(sensor.chipSelect, LOW);
  SPI.transfer(MAX31865_CONFIG_REG | 0x80);
  SPI.transfer(value);
  digitalWrite(sensor.chipSelect, HIGH);
  SPI.endTransaction();
}
bool configIs(const SensorState& sensor, uint8_t expected) {
  return readRegister(sensor, MAX31865_CONFIG_REG) == expected;
}
void configureSensor(SensorState& sensor) {
  sensor.converter->begin(MAX31865_3WIRE);
  sensor.converter->setThresholds(LOW_THRESHOLD, HIGH_THRESHOLD);
  writeConfig(sensor, BASE_CONFIG);
  sensor.failuresSinceConfigure = 0;
}
void fail(SensorState& sensor, const char* reason) {
  writeConfig(sensor, BASE_CONFIG);  // abort conversion/fault cycle and remove bias
  sensor.phase = Phase::Idle;
  if (sensor.consecutiveFailures < UINT8_MAX) ++sensor.consecutiveFailures;
  Serial.printf("[Temperature] %s: %s (failure %u)\n", sensor.name, reason, sensor.consecutiveFailures);
  if (++sensor.failuresSinceConfigure >= 3) configureSensor(sensor);
}
void enterPhase(SensorState& sensor, Phase phase) {
  sensor.phase = phase;
  sensor.phaseStartedMs = millis();
}
void startCycle(SensorState& sensor) {
  sensor.cycleStartedMs = millis();
  if (!configIs(sensor, BASE_CONFIG) ||
      readRegister(sensor, MAX31865_LFAULTMSB_REG, true) != LOW_THRESHOLD ||
      readRegister(sensor, MAX31865_HFAULTMSB_REG, true) != HIGH_THRESHOLD) {
    fail(sensor, "configuration readback failed"); return;
  }
  writeConfig(sensor, BIASED_CONFIG | MAX31865_CONFIG_FAULTSTAT);
  if (!configIs(sensor, BIASED_CONFIG)) { fail(sensor, "bias enable failed"); return; }
  enterPhase(sensor, Phase::BiasSettling);
}
void serviceSensor(SensorState& sensor) {
  const unsigned long now = millis();
  const unsigned long elapsed = now - sensor.phaseStartedMs;
  switch (sensor.phase) {
    case Phase::Idle:
      if (now - sensor.cycleStartedMs >= SAMPLE_INTERVAL_MS) startCycle(sensor);
      return;
    case Phase::BiasSettling:
      if (elapsed < SETTLING_MS) return;
      if (!configIs(sensor, BIASED_CONFIG)) { fail(sensor, "bias configuration lost"); return; }
      // Manual fault detection permits external-filter settling in both switch states.
      writeConfig(sensor, BIASED_CONFIG | FAULT_MANUAL_RUN);
      enterPhase(sensor, Phase::FaultOpen);
      return;
    case Phase::FaultOpen:
      if (elapsed < SETTLING_MS) return;
      if (!configIs(sensor, BIASED_CONFIG | FAULT_MANUAL_RUN)) {
        fail(sensor, "fault cycle start not acknowledged"); return;
      }
      writeConfig(sensor, BIASED_CONFIG | FAULT_MANUAL_FINISH);
      enterPhase(sensor, Phase::FaultFinishing);
      return;
    case Phase::FaultFinishing: {
      if (elapsed < 2) return;
      const uint8_t config = readRegister(sensor, MAX31865_CONFIG_REG);
      if (config == (BIASED_CONFIG | FAULT_MANUAL_FINISH) && elapsed < FAULT_TIMEOUT_MS) return;
      if (config != BIASED_CONFIG || readRegister(sensor, MAX31865_FAULTSTAT_REG) != 0) {
        fail(sensor, "fault cycle failed or timed out"); return;
      }
      enterPhase(sensor, Phase::InputSettling);
      return;
    }
    case Phase::InputSettling:
      if (elapsed < SETTLING_MS) return;
      if (!configIs(sensor, BIASED_CONFIG)) { fail(sensor, "conversion configuration lost"); return; }
      writeConfig(sensor, BIASED_CONFIG | MAX31865_CONFIG_1SHOT);
      // Observe command acceptance before accepting its later cleared state.
      if (!configIs(sensor, BIASED_CONFIG | MAX31865_CONFIG_1SHOT)) {
        fail(sensor, "one-shot start not acknowledged"); return;
      }
      enterPhase(sensor, Phase::Converting);
      return;
    case Phase::Converting: {
      if (elapsed < CONVERSION_MIN_MS) return;
      const uint8_t config = readRegister(sensor, MAX31865_CONFIG_REG);
      if (config == (BIASED_CONFIG | MAX31865_CONFIG_1SHOT) && elapsed < CONVERSION_TIMEOUT_MS) return;
      if (config != BIASED_CONFIG || elapsed > MAX_READING_AGE_MS) {
        fail(sensor, "conversion timeout or configuration lost"); return;
      }
      const uint16_t raw = readRegister(sensor, MAX31865_RTDMSB_REG, true);
      const uint8_t fault = readRegister(sensor, MAX31865_FAULTSTAT_REG);
      const float measured = sensor.converter->calculateTemperature(raw >> 1, RTD_NOMINAL_OHMS, REFERENCE_OHMS);
      const float corrected = TemperatureCalibrationMath::apply(measured, sensor.calibration);
      writeConfig(sensor, BASE_CONFIG);
      if (fault != 0 || (raw & 1) || !configIs(sensor, BASE_CONFIG) ||
          !TemperatureCalibrationMath::inRange(measured) || !TemperatureCalibrationMath::inRange(corrected)) {
        fail(sensor, "invalid RTD, fault or calibrated temperature"); return;
      }
      const bool recovered = sensor.consecutiveFailures > 0 || !sensor.hasReading;
      sensor.lastGoodUncalibrated = measured;
      sensor.lastGoodValue = corrected;
      // Date samples at conversion START, never at a potentially delayed register read.
      sensor.lastGoodMs = sensor.phaseStartedMs;
      sensor.hasReading = true;
      sensor.consecutiveFailures = 0;
      sensor.failuresSinceConfigure = 0;
      sensor.phase = Phase::Idle;
      if (recovered) Serial.printf("[Temperature] %s recovered: %.2f C\n", sensor.name, corrected);
      return;
    }
  }
}
bool isUsable(const SensorState& sensor) {
  return sensor.hasReading && sensor.consecutiveFailures == 0 &&
         (millis() - sensor.lastGoodMs) <= MAX_READING_AGE_MS;
}
unsigned long readingAge(const SensorState& sensor) {
  return sensor.hasReading ? millis() - sensor.lastGoodMs : ULONG_MAX;
}
void loadCalibration() {
  Preferences preferences;
  if (!preferences.begin(CALIBRATION_NAMESPACE, true)) return;
  CalibrationRecord data;
  float gain, offset;
  const bool valid = preferences.getBytesLength(CALIBRATION_KEY) == sizeof(data) &&
      preferences.getBytes(CALIBRATION_KEY, &data, sizeof(data)) == sizeof(data) && data.version == 1 &&
      TemperatureCalibrationMath::coefficients(data.gryde, gain, offset) &&
      TemperatureCalibrationMath::coefficients(data.ventil, gain, offset);
  preferences.end();
  if (valid) { gryde.calibration = data.gryde; ventil.calibration = data.ventil; }
  else Serial.println("[Temperature] Missing/invalid calibration; using uncalibrated temperatures");
}
}  // namespace

void TemperatureHandler::begin(uint8_t grydePin, uint8_t ventilPin) {
  digitalWrite(grydePin, HIGH);
  digitalWrite(ventilPin, HIGH);
  pinMode(grydePin, OUTPUT);
  pinMode(ventilPin, OUTPUT);
  SPI.begin(PIN_TEMP_SCK, PIN_TEMP_MISO, PIN_TEMP_MOSI);
  static Adafruit_MAX31865 grydeConverter(grydePin, &SPI);
  static Adafruit_MAX31865 ventilConverter(ventilPin, &SPI);
  gryde = SensorState{}; ventil = SensorState{};
  gryde.converter = &grydeConverter; gryde.chipSelect = grydePin; gryde.name = "pot sensor";
  ventil.converter = &ventilConverter; ventil.chipSelect = ventilPin; ventil.name = "valve sensor";
  loadCalibration();
  configureSensor(gryde); configureSensor(ventil);
  startCycle(gryde); startCycle(ventil);
}
void TemperatureHandler::readTemperatures() {
  if (!gryde.converter || !ventil.converter) return;
  serviceSensor(gryde); serviceSensor(ventil);
}
bool TemperatureHandler::saveCalibration(const TemperatureCalibration& pot, const TemperatureCalibration& valve) {
  float gain, offset;
  if (!TemperatureCalibrationMath::coefficients(pot, gain, offset) ||
      !TemperatureCalibrationMath::coefficients(valve, gain, offset)) return false;
  CalibrationRecord data;
  data.gryde = pot; data.ventil = valve;
  Preferences preferences;
  if (!preferences.begin(CALIBRATION_NAMESPACE, false)) return false;
  const bool saved = preferences.putBytes(CALIBRATION_KEY, &data, sizeof(data)) == sizeof(data);
  preferences.end();
  if (!saved) return false;
  gryde.calibration = pot; ventil.calibration = valve;
  gryde.hasReading = false; ventil.hasReading = false;
  return true;
}
TemperatureCalibration TemperatureHandler::getGrydeCalibration() { return gryde.calibration; }
TemperatureCalibration TemperatureHandler::getVentilCalibration() { return ventil.calibration; }
float TemperatureHandler::getGrydeUncalibratedTemp() { return isUsable(gryde) ? gryde.lastGoodUncalibrated : NAN; }
float TemperatureHandler::getVentilUncalibratedTemp() { return isUsable(ventil) ? ventil.lastGoodUncalibrated : NAN; }
float TemperatureHandler::getGrydeTemp() { return isUsable(gryde) ? gryde.lastGoodValue : NAN; }
float TemperatureHandler::getVentilTemp() { return isUsable(ventil) ? ventil.lastGoodValue : NAN; }
bool TemperatureHandler::isGrydeValid() { return isUsable(gryde); }
bool TemperatureHandler::isVentilValid() { return isUsable(ventil); }
bool TemperatureHandler::isGrydeHealthy() { return isUsable(gryde); }
bool TemperatureHandler::isVentilHealthy() { return isUsable(ventil); }
uint8_t TemperatureHandler::getGrydeFailureCount() { return gryde.consecutiveFailures; }
uint8_t TemperatureHandler::getVentilFailureCount() { return ventil.consecutiveFailures; }
unsigned long TemperatureHandler::getGrydeAgeMs() { return readingAge(gryde); }
unsigned long TemperatureHandler::getVentilAgeMs() { return readingAge(ventil); }
