#pragma once

#include <WebServer.h>

namespace OTAHandler {
  // Starter mDNS, så du kan tilgå enheden via fx bryg.local
  void beginMDNS(const char* hostname);

  // Sætter HTTP Update Server op på en given webserver
  void setupHTTPUpdate(WebServer &server);
}
