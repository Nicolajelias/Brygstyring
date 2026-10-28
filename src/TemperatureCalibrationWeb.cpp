#include "WebServerHandler.h"
#include "TemperatureHandler.h"
#include "ProcessHandler.h"
#include "OTAHandler.h"
#include "TemperatureCalibrationPage.h"
#include <stdlib.h>
#include <math.h>

void WebServerHandler::handleCalibrationPage() {
  server.send_P(200, "text/html; charset=utf-8", TEMPERATURE_CALIBRATION_PAGE);
}

void WebServerHandler::handleCalibrationData() {
  auto channel = [](const TemperatureCalibration& data, float uncalibrated, float value) {
    float gain, offset;
    TemperatureCalibrationMath::coefficients(data, gain, offset);
    String json = "{\"points\":" + String(data.points);
    json += ",\"measured1\":" + String(data.measured1, 4);
    json += ",\"reference1\":" + String(data.reference1, 4);
    json += ",\"measured2\":" + String(data.measured2, 4);
    json += ",\"reference2\":" + String(data.reference2, 4);
    json += ",\"gain\":" + String(gain, 7);
    json += ",\"offset\":" + String(offset, 5);
    json += ",\"uncalibrated\":" + (isfinite(uncalibrated) ? String(uncalibrated, 3) : String("null"));
    json += ",\"value\":" + (isfinite(value) ? String(value, 3) : String("null"));
    return json + '}';
  };
  String json = "{\"pot\":" + channel(TemperatureHandler::getGrydeCalibration(),
      TemperatureHandler::getGrydeUncalibratedTemp(), TemperatureHandler::getGrydeTemp());
  json += ",\"valve\":" + channel(TemperatureHandler::getVentilCalibration(),
      TemperatureHandler::getVentilUncalibratedTemp(), TemperatureHandler::getVentilTemp());
  const bool canSave = !OTAHandler::maintenanceActive() && !ProcessHandler::isGasValveOn() &&
      ProcessHandler::getCurrentState() == ProcessHandler::BrewState::IDLE;
  json += F(",\"canSave\":"); json += canSave ? "true}" : "false}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

void WebServerHandler::handleSaveCalibration() {
  if (rejectDuringOTA()) return;
  if (ProcessHandler::getCurrentState() != ProcessHandler::BrewState::IDLE || ProcessHandler::isGasValveOn()) {
    server.send(409, "text/plain; charset=utf-8", "Stop brygprocessen og sluk gas før kalibrering gemmes");
    return;
  }
  auto parseChannel = [&](const char* prefix, TemperatureCalibration& result) {
    const String modeName = String(prefix) + "Points";
    if (!server.hasArg(modeName)) return false;
    const String mode = server.arg(modeName);
    if (mode != "0" && mode != "1" && mode != "2") return false;
    result.points = mode.toInt();
    if (result.points == 0) return true;
    auto number = [&](const char* suffix, float& target) {
      const String name = String(prefix) + suffix;
      if (!server.hasArg(name)) return false;
      const String raw = server.arg(name);
      char* end = nullptr;
      const float value = strtof(raw.c_str(), &end);
      if (end == raw.c_str() || *end != '\0' || !TemperatureCalibrationMath::inRange(value)) return false;
      target = value;
      return true;
    };
    if (!number("measured1", result.measured1) || !number("reference1", result.reference1)) return false;
    if (result.points == 2 && (!number("measured2", result.measured2) || !number("reference2", result.reference2))) return false;
    float gain, offset;
    return TemperatureCalibrationMath::coefficients(result, gain, offset);
  };
  TemperatureCalibration pot, valve;
  if (!parseChannel("pot", pot) || !parseChannel("valve", valve)) {
    server.send(400, "text/plain; charset=utf-8", "Ugyldig kalibrering: kontrollér punkter, mindst 10 °C afstand, højst 10 °C korrektion/offset og hældning 0,9–1,1");
    return;
  }
  if (!TemperatureHandler::saveCalibration(pot, valve)) {
    server.send(500, "text/plain; charset=utf-8", "Kunne ikke gemme kalibreringen i flash; den tidligere kalibrering er bevaret");
    return;
  }
  server.send(200, "text/plain; charset=utf-8", "Kalibrering gemt for begge sensorer. Nye målinger bruger korrektionen; data bevares efter genstart.");
}

