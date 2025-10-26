#include "WiFiHandler.h"
#include <WiFi.h>
#include <Arduino.h>
#include "EEPROMHandler.h" // Inkluderer definitionen af Config og EEPROMHandler
#include <ESPmDNS.h> // Inkluderer mDNS
//#include "DisplayHandler.h"

// =====================================================================
// Indkapsling af konstanter og variabler i anonymt namespace
// =====================================================================
namespace {
  const char* AP_SSID    = "BrygAP";
  //const char* NTP_SERVER = "pool.ntp.org";
  const char* MDNS_NAME  = "brygkontrol";

  // Tving en fast IP-adresse i AP-mode
  IPAddress apIP(192, 168, 4, 1);
  IPAddress apGW(192, 168, 4, 1);
  IPAddress apSN(255, 255, 255, 0);

  // Hvor ofte vi tjekker WiFi-forbindelsen (STA-tilstand)
  const unsigned long WIFI_CHECK_INTERVAL = 60000; // 10 sek
  unsigned long lastWiFiCheck = 0; // Tidspunkt for sidste tjek

  // Tiden vi bruger på at prøve STA-forbindelse, før vi giver op (i ms)
  const unsigned long STA_CONNECT_TIMEOUT = 60000;  // 2 sek
}

// =====================================================================
// WiFiHandler::begin()
// =====================================================================
void WiFiHandler::begin() {
  // Læs config fra EEPROM (WiFi/Netværk)
  const Config & cfg = EEPROMHandler::getConfig();
  
  // For at minimere forsinkelse og gøre AP mere responsivt:
  WiFi.setSleep(false);

  // Hvis SSID/password i EEPROM er tomt, start AP med det samme
  if (cfg.ssid[0] == '\0' || cfg.password[0] == '\0') {
    Serial.println("[WiFiHandler] Ingen gyldig WiFi-konfiguration. Starter AP straks...");
    startAP();
    return;
  }

  // Forsøg at forbinde til WiFi (STA)
  WiFi.mode(WIFI_MODE_STA);
  WiFi.setHostname("brygkontrol");
  WiFi.begin(cfg.ssid, cfg.password);

  // Evt. fast IP i STA, hvis config er sat
  if (cfg.ip[0] != '\0' && cfg.gw[0] != '\0' && cfg.sn[0] != '\0') {
    IPAddress ip, gw, sn;
    ip.fromString(cfg.ip);
    gw.fromString(cfg.gw);
    sn.fromString(cfg.sn);
    if(!WiFi.config(ip, gw, sn)) {
      Serial.println("[WiFiHandler] WiFi config mislykkedes, bruger DHCP.");
    }
  }

  Serial.print("[WiFiHandler] Forbinder til: ");
  Serial.println(cfg.ssid);
  
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < STA_CONNECT_TIMEOUT) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFiHandler] Forbundet (STA)!");
    Serial.print("IP: ");
    //DisplayHandler::showMessage("IP: " + WiFi.localIP().toString()+"\n");
    Serial.println(WiFi.localIP());

    // Initialiser mDNS
    if (MDNS.begin(MDNS_NAME)) {
      Serial.println("[WiFiHandler] mDNS responder startet!");
      MDNS.addService("http", "tcp", 80); // Tilføj mDNS-tjeneste
      Serial.print("Du kan nu tilgå webinterfacet på http://");
      Serial.print(MDNS_NAME);
      Serial.println(".local");
      //DisplayHandler::showMessage("http://bryg.local");
    } else {
      Serial.println("[WiFiHandler] Fejl ved start af mDNS responder!");
    }
  } else {
    Serial.println("[WiFiHandler] Forbindelse mislykkedes efter kort forsøg. Starter AP.");
    startAP();
  }
}

// =====================================================================
// WiFiHandler::startAP()
// =====================================================================
void WiFiHandler::startAP() {
  WiFi.mode(WIFI_MODE_AP);

  // Tving IP-konfiguration på AP
  bool success = WiFi.softAPConfig(apIP, apGW, apSN);
  if(!success) {
    Serial.println("[WiFiHandler] Fejl i softAPConfig!");
  }

  // Start AP - uden password => åbent netværk
  // Vi vælger en fast kanal, fx 6, for at undgå scanning
  WiFi.softAP(AP_SSID, nullptr, 6);

  Serial.print("[WiFiHandler] AP startet. SSID: ");
  Serial.println(AP_SSID);
  Serial.print("[WiFiHandler] AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Initialiser mDNS for AP mode
  if (MDNS.begin(MDNS_NAME)) {
    Serial.println("[WiFiHandler] mDNS responder startet i AP mode!");
    MDNS.addService("http", "tcp", 80); // Tilføj mDNS-tjeneste
    Serial.print("Du kan nu tilgå webinterfacet på http://");
    Serial.print(MDNS_NAME);
    Serial.println(".local");
      } else {
    Serial.println("[WiFiHandler] Fejl ved start af mDNS responder i AP mode!");
  }
}

// =====================================================================
// WiFiHandler::handleWiFi()
// =====================================================================
void WiFiHandler::handleWiFi() {
  // Tjek kun, hvis enheden er i STA-mode (eller STA+AP)
  wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
    unsigned long now = millis();
    if (now - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
      lastWiFiCheck = now;
      if (WiFi.status() != WL_CONNECTED) {
        // Hvis forbindelsen tabes i STA, så genstart
        Serial.println("[WiFiHandler] WiFi-forbindelse tabt => Genstart!");
        ESP.restart();
      }
    }
  }
}

bool WiFiHandler::isAPMode() {
    return WiFi.getMode() == WIFI_MODE_AP;
}
