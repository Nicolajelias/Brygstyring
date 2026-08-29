#pragma once

#include <Arduino.h>
#include <WebServer.h>

namespace OTAHandler {
enum class State : uint8_t { IDLE, UPLOADING, VALIDATING, SUCCESS, FAILED, REBOOTING };

struct Status {
  State state = State::IDLE;
  uint8_t progress = 0;
  String error;
  String lastResult;
  bool authConfigured = false;
  bool rollbackSupported = false;
  bool pendingBootValidation = false;
};

void begin();
void update(unsigned long now);
void registerRoutes(WebServer& server);
bool maintenanceActive();
const Status& getStatus();
const char* stateName(State state);
}  // namespace OTAHandler
