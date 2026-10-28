#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include <Arduino.h>

constexpr uint8_t PIN_TEMP_GRYDE  = 4;
constexpr uint8_t PIN_TEMP_VENTIL = 5;
constexpr uint8_t PIN_TEMP_SCK    = 12;
constexpr uint8_t PIN_TEMP_MOSI   = 13;
constexpr uint8_t PIN_TEMP_MISO   = 14;
constexpr uint8_t PIN_PUMP        = 18;
constexpr uint8_t PIN_GAS         = 17;
constexpr uint8_t PIN_BUZZER      = 21;
constexpr uint8_t PIN_BUTTON      = 15;
constexpr uint8_t PIN_OLED_SDA    = 8;
constexpr uint8_t PIN_OLED_SCL    = 9;
#ifndef BRYG_RGB_LED_GPIO
#define BRYG_RGB_LED_GPIO 48  // Original DevKitC-1. Set to 38 for official revision 1.1.
#endif
constexpr uint8_t PIN_RGB_LED     = BRYG_RGB_LED_GPIO;
static_assert(PIN_RGB_LED == 48 || PIN_RGB_LED == 38, "Select the DevKit RGB LED GPIO: 48 or 38");
constexpr int8_t  PIN_RGB_LED_PWR = -1;  // Sæt til -1 hvis strømstyring ikke er nødvendig

#endif // PIN_CONFIG_H
