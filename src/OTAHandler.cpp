#include "OTAHandler.h"
#include <ESPmDNS.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <Arduino.h>

// Statisk instans af HTTPUpdateServer
static HTTPUpdateServer httpUpdater;

void OTAHandler::beginMDNS(const char* hostname) {
  if (!MDNS.begin(hostname)) {
    Serial.println("[OTAHandler] mDNS start mislykkedes.");
    return;
  }
  Serial.print("[OTAHandler] mDNS kører som: ");
  Serial.println(hostname);
}

void OTAHandler::setupHTTPUpdate(WebServer &server) {
  // Sæt /update-side op uden password (du kan tilføje brugernavn/password om ønsket)
  httpUpdater.setup(&server);

  // mDNS: annoncer "http" service på port 80
  MDNS.addService("http", "tcp", 80);

  Serial.println("[OTAHandler] HTTP Update-server klar (besøg /update i browseren).");
}
