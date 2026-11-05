#include "Arduino.h"
#include "SPI.h"
#include "Preferences.h"
#include "TemperatureHandler.h"
#include "EEPROMHandler.h"
#include "EEPROM.h"
#include "StatusLED.h"
#include "WebServer.h"
#include "WiFi.h"
#include "ControlRuntime.h"
#include "ClockService.h"
#include "ProcessStorage.h"
#include "freertos/FreeRTOS.h"
// Select all process states without driving long brewing sequences in a unit test.
#define private public
#include "ProcessHandler.h"
#include "WebServerHandler.h"
#undef private
#include <cassert>
#include <iostream>

uint32_t mockNow = 0;
bool mockWiFiConnected = false;
unsigned mockTaskTicks = 0;
void (*mockControlTask)(void*) = nullptr;
unsigned long mockEpoch = 0;
namespace ClockService { bool synchronized() {return mockEpoch != 0;} unsigned long epoch() {return mockEpoch;} }
WiFiMock WiFi;
SerialMock Serial;
SPIClass SPI;
std::array<MockChip, 64> mockChips;
std::vector<unsigned char> Preferences::blob;
std::map<std::string, std::vector<unsigned char>> Preferences::records;
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
int digitalRead(int pin) {
  if (pin == 6 || pin == 7) {
    auto& chip = mockChips[pin - 2];
    chip.tick();
    if (chip.stuckLow) return LOW;
    if (chip.disconnected || chip.stuckHigh) return HIGH;
    return chip.ready ? LOW : HIGH;
  }
  return mockPins[pin];
}
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
void StatusLED::setProcessActive(bool) {}
void StatusLED::notify() {}

void advance(uint32_t ms) {
  for (uint32_t i = 0; i < ms; ++i) { ++mockNow; TemperatureHandler::readTemperatures(); }
}
void reset(bool eraseCalibration = true) {
  mockNow = 0; SPI.selected = -1; mockEpoch = 0;
  ControlRuntime::leaveMaintenance();
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
  reset();
  mockChips[4].disconnected = true;
  for (int i = 0; i < 10000; ++i) {
    advance(1);
    ProcessHandler::update(NAN, TemperatureHandler::getVentilTemp());
    ProcessHandler::getFormattedTime();
  }
  assert(TemperatureHandler::isVentilValid());
  assert(TemperatureHandler::getVentilAgeMs() < 1200);
  assert(ProcessHandler::getFormattedTime() == "--:--:--");
  // Actual periodic task continues with no UI, network or clock service calls.
  assert(ControlRuntime::begin());
  mockTaskTicks = 12000; // 60 seconds, no web loop running
  try { mockControlTask(nullptr); } catch (TaskFinished&) {}
  assert(TemperatureHandler::isVentilValid());
  assert(TemperatureHandler::getVentilAgeMs() < 1200);
  assert(ControlRuntime::metrics().maxGapMs <= 5);
  assert(ControlRuntime::metrics().cycles == 12000);
  std::cout << "PASS actual control task: 60 s without UI/network/time, missing pot, fresh valve\n";
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
  assert(ProcessHandler::toggleGasValve());
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
    if (state == ProcessHandler::BrewState::IDLE || state == ProcessHandler::BrewState::PAUSED) {
      assert(ProcessHandler::isGasValveOn() && ProcessHandler::isGasManual());
      assert(!ProcessHandler::toggleGasValve()); expectOff();
      assert(ProcessHandler::togglePump());
      assert(!ProcessHandler::togglePump());
    } else expectOff();
    ProcessHandler::gasControl(true); expectOff();
  }
  std::cout << "PASS automatic gas interlock, manual gas and pump unaffected in IDLE/PAUSED\n";

  reset(); validReadings(); assert(ProcessHandler::toggleGasValve());
  mockNow += 5001;
  ProcessHandler::enforceTemperatureSafety();
  assert(ProcessHandler::isGasValveOn() && ProcessHandler::isGasManual());
  assert(!TemperatureHandler::isGrydeValid());
  assert(!ProcessHandler::toggleGasValve()); expectOff();
  assert(ProcessHandler::toggleGasValve()); // stale data cannot block ON either
  ProcessHandler::startMashing(); expectOff();
  assert(ProcessHandler::getCurrentState() == ProcessHandler::BrewState::IDLE);
  ProcessHandler::currentState = ProcessHandler::BrewState::PAUSED;
  assert(ProcessHandler::toggleGasValve());
  ProcessHandler::resumeProcess(); expectOff();
  assert(ProcessHandler::getCurrentState() == ProcessHandler::BrewState::PAUSED);
  std::cout << "PASS stale readings preserve manual gas; automatic start/resume reject stale data\n";

  reset();
  mockChips[4].clearStartImmediately = true;
  advance(103);
  assert(!TemperatureHandler::isGrydeValid());
  advance(5);
  assert(TemperatureHandler::isGrydeValid());
  assert(TemperatureHandler::isVentilValid());
  assert(mockChips[4].earlyReads == 0);
  advance(1000);
  assert(TemperatureHandler::isGrydeValid());
  reset(); mockChips[4].neverFinish = true; advance(149);
  assert(TemperatureHandler::getGrydeFailureCount() == 1);
  assert(!TemperatureHandler::isGrydeValid() && mockChips[4].registers[0] == 0x11);
  reset(); mockChips[4].faultHangs = true; advance(45);
  assert(TemperatureHandler::getGrydeFailureCount() == 1);
  reset(); mockChips[4].disconnected = true; advance(1100);
  assert(!TemperatureHandler::isGrydeValid() && TemperatureHandler::isVentilValid());
  std::cout << "PASS self-clearing start bit, full conversion wait, timeout and disconnection\n";

  reset(); validReadings();
  mockChips[4].ignoreStart = true; advance(1100);
  assert(!TemperatureHandler::isGrydeValid() && TemperatureHandler::isVentilValid());
  assert(TemperatureHandler::grydeDiagnostics().samples == 1);
  assert(!strcmp(TemperatureHandler::grydeDiagnostics().lastError,"DRDY completion timeout"));
  mockChips[4].ignoreStart = false; advance(1000);
  assert(TemperatureHandler::isGrydeValid());
  for (int cs : {4,5}) {
    reset(); mockChips[cs].stuckHigh = true; advance(150);
    assert(cs == 4 ? !TemperatureHandler::isGrydeValid() : !TemperatureHandler::isVentilValid());
    reset(); mockChips[cs].stuckLow = true; advance(150);
    assert(cs == 4 ? !TemperatureHandler::isGrydeValid() : !TemperatureHandler::isVentilValid());
    reset(); mockChips[cs].ignoreReadyClear = true; advance(150);
    assert(cs == 4 ? !TemperatureHandler::isGrydeValid() : !TemperatureHandler::isVentilValid());
    assert(!strcmp((cs == 4 ? TemperatureHandler::grydeDiagnostics() : TemperatureHandler::ventilDiagnostics()).lastError,
                   "DRDY not HIGH after RTD read"));
  }
  reset(); validReadings(); advance(5000);
  assert(TemperatureHandler::grydeDiagnostics().samples == 6); // identical temperature is legitimate
  std::cout << "PASS DRDY: ignored start with stale plausible data, stuck HIGH/LOW, failed clear, recovery, constant temperature\n";

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
  reset();
  // No flash writes from process state transitions or when an output is ON.
  Preferences::records.clear();
  ProcessHandler::stopProcess();
  assert(Preferences::records.empty());
  mockNow = 2000;
  assert(ProcessHandler::toggleGasValve());
  ProcessStorage::flush(); assert(Preferences::records.empty());
  assert(!TemperatureHandler::saveCalibration(one,two));
  assert(!ControlRuntime::enterMaintenance());
  ProcessHandler::stopProcess();
  assert(ControlRuntime::enterMaintenance());
  assert(!ProcessHandler::toggleGasValve());
  assert(!ProcessHandler::togglePump());
  ControlRuntime::leaveMaintenance();
  mockNow += 2000; ProcessStorage::flush();
  assert(!Preferences::records["brew-process"].empty());
  assert(Preferences::blob.empty()); // separate from calibration
  ProcessStorage::Record saved;
  ProcessStorage::begin(); assert(ProcessStorage::load(saved));
  assert(saved.state == 0);
  // Time corrections never change elapsed process durations.
  reset(); validReadings(); ProcessHandler::startBoiling();
  assert(ProcessHandler::isGasValveOn());
  mockEpoch = 1800000000; mockNow += 2000;
  assert(ProcessHandler::getRemainingTime() == 3598);
  mockEpoch = 1700000000; mockNow += 1000;
  assert(ProcessHandler::getRemainingTime() == 3597);
  mockEpoch = 0; ProcessHandler::stopProcess();
  std::cout << "PASS maintenance exclusion, deferred independent storage, monotonic process timers\n";

}
