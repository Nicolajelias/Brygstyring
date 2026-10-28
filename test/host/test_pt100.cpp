#include "Arduino.h"
#include "SPI.h"
#include "Preferences.h"
#include "TemperatureHandler.h"
#include "EEPROMHandler.h"
#include "EEPROM.h"
#include "StatusLED.h"
#include "WebServer.h"
// Select all process states without driving long brewing sequences in a unit test.
#define private public
#include "ProcessHandler.h"
#include "WebServerHandler.h"
#undef private
#include <cassert>
#include <iostream>

uint32_t mockNow = 0;
SerialMock Serial;
SPIClass SPI;
std::array<MockChip, 64> mockChips;
std::vector<unsigned char> Preferences::blob;
bool Preferences::failWrites = false;
EEPROMMock EEPROM;
WebServer WebServerHandler::server(80);
bool mockOTA = false;
namespace OTAHandler { bool maintenanceActive() { return mockOTA; } }
bool WebServerHandler::rejectDuringOTA() {
  if (!mockOTA) return false;
  server.send(503, "text/plain", "OTA"); return true;
}
int mockPins[64] = {};
void digitalWrite(int pin, int level) {
  mockPins[pin] = level;
  if (pin == 4 || pin == 5) {
    if (level == LOW) { assert(SPI.selected < 0); SPI.selected = pin; SPI.address = -1; }
    else SPI.selected = -1;
  }
}
Config EEPROMHandler::getConfig() {
  Config config{};
  config.mashTime = 5400; config.mashoutTime = 600; config.boilTime = 3600;
  config.mashSetpoint = 64; config.mashoutSetpoint = 75; config.hysteresis = 1; config.tempOffset = 5;
  return config;
}
void StatusLED::setAwaitingConfirmation(bool) {}

void advance(uint32_t ms) {
  for (uint32_t i = 0; i < ms; ++i) { ++mockNow; TemperatureHandler::readTemperatures(); }
}
void reset(bool eraseCalibration = true) {
  mockNow = 0; SPI.selected = -1;
  mockChips = {};
  std::fill(std::begin(mockPins), std::end(mockPins), HIGH);
  Preferences::failWrites = false;
  if (eraseCalibration) Preferences::blob.clear();
  mockOTA = false;
  TemperatureHandler::begin(4, 5);
  ProcessHandler::begin(17, 18, 21, 15);
}
void validReadings() {
  advance(120);
  assert(TemperatureHandler::isGrydeValid() && TemperatureHandler::isVentilValid());
}
void expectOff() { assert(!ProcessHandler::isGasValveOn() && mockPins[17] == LOW); }

int main() {
  static_assert(sizeof(unsigned long) == 4, "Tests require 32-bit millis arithmetic, as on ESP32");
  float gain, offset;
  TemperatureCalibration one{1, 20, 21, 100, 100};
  TemperatureCalibration two{2, 1, 0, 101, 100};
  assert(TemperatureCalibrationMath::apply(65, one) == 66);
  assert(TemperatureCalibrationMath::apply(51, two) == 50);
  assert(std::fabs(TemperatureCalibrationMath::apply(51, {2, 1, 0, 101, 102}) - 51) < 0.001f);
  TemperatureCalibration invalid{2, 20, 21, 20, 22};
  assert(!TemperatureCalibrationMath::coefficients(invalid, gain, offset));
  invalid = {2, 100, 100, 0, 0};
  assert(!TemperatureCalibrationMath::coefficients(invalid, gain, offset));
  invalid = {1, NAN, 20, 100, 100};
  assert(!TemperatureCalibrationMath::coefficients(invalid, gain, offset));
  invalid = {1, 20, 40, 100, 100};
  assert(!TemperatureCalibrationMath::coefficients(invalid, gain, offset));
  std::cout << "PASS calibration math and invalid inputs\n";

  reset();
  assert(!ProcessHandler::toggleGasValve()); expectOff();
  advance(103);  // conversion started at 38 ms: 65 ms has elapsed
  assert(!TemperatureHandler::isGrydeValid());
  advance(5);
  assert(TemperatureHandler::isGrydeValid());
  assert(mockChips[4].earlyReads == 0 && mockChips[5].earlyReads == 0);
  assert(mockChips[4].registers[0] == 0x11 && mockChips[5].registers[0] == 0x11);
  assert(TemperatureHandler::getGrydeAgeMs() == 70);
  std::cout << "PASS startup, 66 ms converter, sample age and bias shutdown\n";

  for (auto state : {ProcessHandler::BrewState::IDLE, ProcessHandler::BrewState::PAUSED,
       ProcessHandler::BrewState::MASHING, ProcessHandler::BrewState::MASHOUT,
       ProcessHandler::BrewState::BOILHEATUP, ProcessHandler::BrewState::BOILING}) {
    reset(); validReadings();
    ProcessHandler::currentState = state;
    if (state == ProcessHandler::BrewState::IDLE || state == ProcessHandler::BrewState::PAUSED)
      assert(ProcessHandler::toggleGasValve());
    else ProcessHandler::gasControl(true);
    mockChips[5].injectedFault = 0x10;  // fault cycle detects a wiring fault with plausible RTD data
    advance(1000);
    assert(!TemperatureHandler::isVentilValid());
    assert(TemperatureHandler::isGrydeValid());
    ProcessHandler::update(TemperatureHandler::getGrydeTemp(), TemperatureHandler::getVentilTemp());
    expectOff();
    ProcessHandler::gasControl(true); expectOff();
  }
  std::cout << "PASS gas interlock in all six process states and every ON path\n";

  reset(); validReadings(); assert(ProcessHandler::toggleGasValve());
  mockNow += 5001;
  ProcessHandler::enforceTemperatureSafety(); expectOff();
  assert(!TemperatureHandler::isGrydeValid());
  std::cout << "PASS stale readings close manual gas\n";

  reset(); validReadings();
  const auto beforeAge = TemperatureHandler::getGrydeAgeMs();
  mockChips[4].ignoreStart = true;
  advance(1000);
  assert(!TemperatureHandler::isGrydeValid());
  assert(TemperatureHandler::getGrydeAgeMs() > beforeAge);
  assert(TemperatureHandler::isVentilValid());
  mockChips[4].ignoreStart = false; advance(1000);
  assert(TemperatureHandler::isGrydeValid());
  reset(); mockChips[4].neverFinish = true; advance(149);
  assert(TemperatureHandler::getGrydeFailureCount() == 1);
  assert(!TemperatureHandler::isGrydeValid() && mockChips[4].registers[0] == 0x11);
  reset(); mockChips[4].faultHangs = true; advance(45);
  assert(TemperatureHandler::getGrydeFailureCount() == 1);
  reset(); mockChips[4].disconnected = true; advance(1100);
  assert(!TemperatureHandler::isGrydeValid() && TemperatureHandler::isVentilValid());
  std::cout << "PASS missing command, stale register, conversion/fault timeout, disconnection, recovery\n";

  reset(); validReadings();
  assert(TemperatureHandler::saveCalibration(one, two));
  assert(!TemperatureHandler::isGrydeValid());
  advance(1000);
  assert(std::fabs(TemperatureHandler::getGrydeTemp() - TemperatureHandler::getGrydeUncalibratedTemp() - 1) < 0.001f);
  assert(std::fabs(TemperatureHandler::getVentilTemp() - TemperatureHandler::getVentilUncalibratedTemp() + 1) < 0.001f);
  reset(false); validReadings();
  assert(TemperatureHandler::getGrydeCalibration().points == 1);
  assert(TemperatureHandler::getVentilCalibration().points == 2);
  Preferences::failWrites = true;
  assert(!TemperatureHandler::saveCalibration({}, {}));
  assert(TemperatureHandler::getGrydeCalibration().points == 1);
  assert(!TemperatureHandler::saveCalibration(invalid, two));
  Preferences::failWrites = false;
  assert(TemperatureHandler::saveCalibration({}, {}));
  reset(false); validReadings();
  assert(TemperatureHandler::getGrydeTemp() == TemperatureHandler::getGrydeUncalibratedTemp());
  Preferences::blob[0] = 99; reset(false); validReadings();
  assert(TemperatureHandler::getGrydeCalibration().points == 0);
  std::cout << "PASS independent calibration, persistence, flash failure, reset, unknown record version\n";

  reset();
  mockChips[4].nextRaw = 2; advance(120);
  assert(!TemperatureHandler::isGrydeValid());
  reset();
  mockChips[4].nextRaw |= 1; advance(120);
  assert(!TemperatureHandler::isGrydeValid());
  reset(); validReadings();
  mockNow = UINT32_MAX - 100;
  advance(1200); // new conversion across millis rollover
  assert(TemperatureHandler::isGrydeValid() && TemperatureHandler::isVentilValid());
  std::cout << "PASS invalid RTD, embedded fault bit and millis rollover\n";

  reset(); validReadings();
  auto& web = WebServerHandler::server;
  const std::map<std::string, String> validForm = {
    {"potPoints", "1"}, {"potmeasured1", "20"}, {"potreference1", "21"},
    {"valvePoints", "2"}, {"valvemeasured1", "1"}, {"valvereference1", "0"},
    {"valvemeasured2", "101"}, {"valvereference2", "100"}
  };
  web.arguments = validForm;
  WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 200);
  for (const char* bad : {"", "abc", "nan", "inf", "20junk", "126"}) {
    web.arguments = validForm; web.arguments["potmeasured1"] = bad;
    WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 400);
    assert(TemperatureHandler::getGrydeCalibration().reference1 == 21);
  }
  web.arguments = validForm; web.arguments.erase("valvereference2");
  WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 400);
  web.arguments = validForm; web.arguments["valvePoints"] = "1x";
  WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 400);
  web.arguments = validForm; web.arguments["valvemeasured2"] = "2";
  WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 400);
  web.arguments = validForm;
  for (auto state : {ProcessHandler::BrewState::PAUSED, ProcessHandler::BrewState::MASHING,
       ProcessHandler::BrewState::MASHOUT, ProcessHandler::BrewState::BOILHEATUP, ProcessHandler::BrewState::BOILING}) {
    ProcessHandler::currentState = state;
    WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 409);
  }
  ProcessHandler::currentState = ProcessHandler::BrewState::IDLE;
  advance(1000); assert(ProcessHandler::toggleGasValve());
  WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 409);
  ProcessHandler::toggleGasValve();
  mockOTA = true;
  WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 503);
  mockOTA = false; Preferences::failWrites = true;
  WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 500);
  Preferences::failWrites = false;
  web.arguments = validForm; web.arguments["potPoints"] = "0";
  WebServerHandler::handleSaveCalibration(); assert(web.responseCode == 200);
  assert(TemperatureHandler::getGrydeCalibration().points == 0);
  assert(TemperatureHandler::getVentilCalibration().points == 2);
  WebServerHandler::handleCalibrationData();
  assert(web.responseCode == 200 && web.response.find("\"uncalibrated\":null") != String::npos);
  assert(web.response.find("\"canSave\":true") != String::npos);
  std::cout << "PASS real HTTP handlers: validation, process/gas/OTA guards, NVS error, per-channel reset, JSON\n";
}
