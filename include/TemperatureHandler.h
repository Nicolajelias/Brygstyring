#ifndef TEMPERATURE_HANDLER_H
#define TEMPERATURE_HANDLER_H

#include <Arduino.h>
#include <DallasTemperature.h>

typedef uint8_t DeviceAddress[8];

class TemperatureHandler {
public:
  static void begin(uint8_t pin);
  static void readTemperatures();
  static float getGrydeTemp();
  static float getVentilTemp();

  // Nye funktioner:
  static bool compareAddress(const DeviceAddress a, const DeviceAddress b);
  static String addressToString(const DeviceAddress deviceAddress);
  static void stringToAddress(const String &hexString, DeviceAddress deviceAddress);
  static String getGrydeAddressString();
  static String getVentilAddressString();
  static void swapSensorAddresses();
  
  // Ny metode: Læs aktuelle sensoradresser og gem dem i EEPROM.
  // Returnerer true, hvis opdatering lykkes, false hvis ikke.
  static bool updateSensorAddresses();
  static bool readActualSensorAddresses(String &gryde, String &ventil);

private:
  // Ingen private variabler er nødvendige her, medmindre du vil gemme adresserne lokalt her.
};

#endif // TEMPERATURE_HANDLER_H
