#include "OTAHandler.h"
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Arduino.h>

// Statisk instans af HTTPUpdateServer
static ESP8266HTTPUpdateServer httpUpdater;

void OTAHandler::beginMDNS(const char* hostname) {
  if (!MDNS.begin(hostname)) {
    Serial.println("[OTAHandler] mDNS start mislykkedes.");
    return;
  }
  Serial.print("[OTAHandler] mDNS kører som: ");
  Serial.println(hostname);
}

void OTAHandler::setupHTTPUpdate(ESP8266WebServer &server) {
  // Sæt /update-side op uden password (du kan tilføje brugernavn/password om ønsket)
  httpUpdater.setup(&server);

  // mDNS: annoncer "http" service på port 80
  MDNS.addService("http", "tcp", 80);

  Serial.println("[OTAHandler] HTTP Update-server klar (besøg /update i browseren).");
}
