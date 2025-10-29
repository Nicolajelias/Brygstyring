#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>

class StatusLED {
public:
  enum class Mode { Off, WifiConnecting, WifiConnected, AccessPoint };

  static void begin(uint8_t pin, uint8_t brightness = 64);
  static void setNetworkMode(Mode mode);
  static void setProcessActive(bool active);
  static void setAwaitingConfirmation(bool active);
  static void update();
};

#endif // STATUS_LED_H
