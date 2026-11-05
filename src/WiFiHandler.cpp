#include "WiFiHandler.h"
#include "EEPROMHandler.h"
#include "StatusLED.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
namespace {
constexpr const char* AP_SSID = "BrygAP";
constexpr const char* HOSTNAME = "brygkontrol";
constexpr unsigned long CONNECT_TIMEOUT_MS = 15000, RETRY_MS = 30000;
Config networkConfig{};
bool credentials = false, attempting = false, connected = false, apActive = false;
unsigned long lastAttempt = 0;
void mdns() {
  MDNS.end();
  if (MDNS.begin(HOSTNAME)) { MDNS.setInstanceName("Brygkontrol"); MDNS.addService("http", "tcp", 80); }
}
void connectStation() {
  lastAttempt = millis(); attempting = true;
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(networkConfig.ssid, networkConfig.password);
  StatusLED::setNetworkMode(StatusLED::Mode::WifiConnecting);
  Serial.println("[WiFi] Connecting in background; control remains active");
}
}
void WiFiHandler::begin() {
  attempting = connected = apActive = false;
  lastAttempt = millis();
  networkConfig = EEPROMHandler::getConfig();
  credentials = networkConfig.ssid[0] != '\0';
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  if (!credentials) { startAP(); return; }
  WiFi.mode(WIFI_STA);
  IPAddress ip, gw, sn;
  if (ip.fromString(networkConfig.ip) && gw.fromString(networkConfig.gw) && sn.fromString(networkConfig.sn))
    WiFi.config(ip, gw, sn, gw);
  else WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  connectStation();
}
void WiFiHandler::startAP() {
  if (!WiFi.mode(credentials ? WIFI_AP_STA : WIFI_AP)) return;
  WiFi.softAPsetHostname(HOSTNAME);
  if (!WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0))) return;
  if (!WiFi.softAP(AP_SSID, nullptr, 1, 0, 4)) return;
  esp_wifi_set_ps(WIFI_PS_NONE);
  apActive = true;
  StatusLED::setNetworkMode(StatusLED::Mode::AccessPoint);
  mdns();
  Serial.println("[WiFi] Open BrygAP available at 192.168.4.1");
}
void WiFiHandler::handleWiFi() {
  const unsigned long now = millis();
  if (!credentials) {
    if (!apActive && now - lastAttempt >= RETRY_MS) { lastAttempt = now; startAP(); }
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    attempting = false;
    if (!connected) {
      connected = true;
      StatusLED::setNetworkMode(StatusLED::Mode::WifiConnected);
      mdns();
      Serial.print("[WiFi] Connected, IP: "); Serial.println(WiFi.localIP());
    }
    return;
  }
  if (connected) {
    connected = false;
    Serial.println("[WiFi] Connection lost; retrying without restarting controller");
    if (!apActive) startAP();
    connectStation();
    return;
  }
  if (attempting && now - lastAttempt >= CONNECT_TIMEOUT_MS) {
    WiFi.disconnect(false, false);
    attempting = false; lastAttempt = now;
    if (!apActive) startAP();
  } else if (!attempting && now - lastAttempt >= RETRY_MS) connectStation();
}
bool WiFiHandler::isAPMode() { return apActive && WiFi.status() != WL_CONNECTED; }
