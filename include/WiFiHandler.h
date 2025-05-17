#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

class WiFiHandler {
public:
  static void begin();
  static void handleWiFi();
  static bool isAPMode();

private:
  static void startAP();
};

#endif // WIFI_HANDLER_H
