#include <cassert>
#include <iostream>
#include <cstddef>
#include "Arduino.h"
#include "EEPROM.h"
#include "EEPROMHandler.h"
#include "WiFi.h"
#include "WiFiHandler.h"
#include "StatusLED.h"
#include "ESPmDNS.h"
uint32_t mockNow=0;
int mockPins[64]{};
bool mockWiFiConnected=false;
SerialMock Serial;
EEPROMMock EEPROM;
WiFiMock WiFi;
MDNSMock MDNS;
void StatusLED::setNetworkMode(StatusLED::Mode) {}
int main() {
 static_assert(sizeof(Config)==140 && offsetof(Config,sn)==96,"Legacy layout must remain readable");
 EEPROMHandler::resetToDefaults();
 Config config=EEPROMHandler::getConfig();
 strcpy(config.ssid,"test-network"); strcpy(config.password,"test-only");
 strcpy(config.ip,"192.168.1.99"); strcpy(config.gw,"192.168.1.1"); strcpy(config.sn,"255.255.255.0");
 EEPROMHandler::saveConfig(config);
 // Reproduce the legacy offset-100 collision, preserving credentials.
 memset(EEPROM.data.data()+100,0xff,12);
 EEPROMHandler::begin(); config=EEPROMHandler::getConfig();
 assert(!strcmp(config.ssid,"test-network") && !strcmp(config.password,"test-only"));
 assert(!config.ip[0] && !config.sn[0] && !config.gw[0]);
 std::cout << "PASS legacy subnet corruption migration retains Wi-Fi credentials\n";
 WiFiHandler::begin();
 assert(WiFi.attempts==1 && mockNow<10 && WiFi.aps==0);
 mockNow+=15000; WiFiHandler::handleWiFi();
 assert(WiFi.disconnects==1 && WiFi.aps==1 && WiFiHandler::isAPMode());
 mockNow+=29999; WiFiHandler::handleWiFi(); assert(WiFi.attempts==1);
 mockNow+=1; WiFiHandler::handleWiFi(); assert(WiFi.attempts==2);
 mockWiFiConnected=true; WiFiHandler::handleWiFi(); assert(!WiFiHandler::isAPMode());
 mockWiFiConnected=false; WiFiHandler::handleWiFi();
 assert(WiFi.attempts==3 && WiFi.disconnects==1); // no unsigned time underflow
 for (int i=0;i<12;++i) {mockNow+=15000; WiFiHandler::handleWiFi();}
 assert(WiFi.attempts>=6 && WiFiHandler::isAPMode());
 mockWiFiConnected=true; WiFiHandler::handleWiFi(); assert(!WiFiHandler::isAPMode());
 std::cout << "PASS missing router, open AP fallback, repeated retries, reconnect without system reset\n";
 // A fresh begin is a boot in production; explicitly reset test state in begin.
 EEPROMHandler::resetToDefaults(); WiFi=WiFiMock{}; mockWiFiConnected=false;
 WiFiHandler::begin(); assert(WiFi.attempts==0 && WiFi.aps==1);
 mockNow+=60000; WiFiHandler::handleWiFi(); assert(WiFi.attempts==0);
 std::cout << "PASS unconfigured Wi-Fi opens password-free BrygAP immediately\n";
}
