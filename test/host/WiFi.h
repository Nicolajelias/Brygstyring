#pragma once
#include <cassert>
#include "IPAddress.h"
#include "Arduino.h"
constexpr int WL_CONNECTED = 3;
constexpr int WIFI_STA=1,WIFI_AP=2,WIFI_AP_STA=3;
extern bool mockWiFiConnected;
struct WiFiMock {
  unsigned attempts=0,disconnects=0,aps=0;
  bool apWorks=true;
  int currentMode=0;
  void persistent(bool) {}
  void setAutoReconnect(bool) {}
  void setSleep(bool) {}
  bool mode(int mode) {currentMode=mode; return true;}
  void setHostname(const char*) {}
  void softAPsetHostname(const char*) {}
  void begin(const char*,const char*) {++attempts; ++mockNow;}
  void disconnect(bool,bool) {++disconnects; mockWiFiConnected=false;}
  template<class... Args> bool config(Args...) {return true;}
  template<class... Args> bool softAPConfig(Args...) {return true;}
  bool softAP(const char* name,const char* password,int,int,int) { assert(!strcmp(name,"BrygAP") && password==nullptr); ++aps; return apWorks; }
  IPAddress localIP() {return IPAddress(192,168,1,2);}
  int status() const { return mockWiFiConnected ? WL_CONNECTED : 0; }
};
extern WiFiMock WiFi;
