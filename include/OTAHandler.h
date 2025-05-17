#pragma once

#include <ESP8266WebServer.h>

namespace OTAHandler {
  // Starter mDNS, så du kan tilgå enheden via fx bryg.local
  void beginMDNS(const char* hostname);

  // Sætter HTTP Update Server op på en given webserver
  void setupHTTPUpdate(ESP8266WebServer &server);
}
