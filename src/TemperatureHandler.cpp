#include "TemperatureHandler.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "EEPROMHandler.h"

// Definér PIN_SENSOR – evt. D4
static const uint8_t PIN_SENSOR = D4; 

OneWire oneWire(PIN_SENSOR);
DallasTemperature sensors(&oneWire);

float grydeTemp = 0.0;
float ventilTemp = 0.0;
bool grydeTempValid = false;
bool ventilTempValid = false;

// Globale DeviceAddress-variabler til sensorernes adresser
DeviceAddress grydeAddress;
DeviceAddress ventilAddress;

void TemperatureHandler::begin(uint8_t pin) {
  sensors.begin();

  // Hent konfigurationen fra EEPROM
  Config cfg = EEPROMHandler::getConfig();

  // Konverter de gemte hex-strenge til DeviceAddress
  if (cfg.grydeAddress[0] != '\0') {
    stringToAddress(String(cfg.grydeAddress), grydeAddress);
  } else {
    // Default adresse – tilpas efter dine sensorer
    DeviceAddress defaultGryde = { 0x28, 0xFF, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 };
    memcpy(grydeAddress, defaultGryde, sizeof(DeviceAddress));
  }

  if (cfg.ventilAddress[0] != '\0') {
    stringToAddress(String(cfg.ventilAddress), ventilAddress);
  } else {
    DeviceAddress defaultVentil = { 0x28, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    memcpy(ventilAddress, defaultVentil, sizeof(DeviceAddress));
  }
}

void TemperatureHandler::readTemperatures() {
  grydeTempValid = false;
  ventilTempValid = false;

  sensors.requestTemperatures();
  delay(100); // Vent på måling
  int deviceCount = sensors.getDeviceCount();
  for (int i = 0; i < deviceCount; i++) {
    DeviceAddress addr;
    if (sensors.getAddress(addr, i)) {
      float temp = sensors.getTempC(addr);
      if (temp == -127.0f || temp < -50 || temp > 150) {
        Serial.println("Fejl: Temperatur ugyldig fra sensor");
        continue;
      }
      if (compareAddress(addr, grydeAddress)) {
        grydeTemp = temp;
        grydeTempValid = true;
      } else if (compareAddress(addr, ventilAddress)) {
        ventilTemp = temp;
        ventilTempValid = true;
      }
    }
  }
}

float TemperatureHandler::getGrydeTemp() {
  return grydeTempValid ? grydeTemp : NAN;
}

float TemperatureHandler::getVentilTemp() {
  return ventilTempValid ? ventilTemp : NAN;
}

bool TemperatureHandler::isGrydeValid() {
  return grydeTempValid;
}

bool TemperatureHandler::isVentilValid() {
  return ventilTempValid;
}

bool TemperatureHandler::compareAddress(const DeviceAddress a, const DeviceAddress b) {
  for (uint8_t i = 0; i < 8; i++) {
    if (a[i] != b[i])
      return false;
  }
  return true;
}

String TemperatureHandler::addressToString(const DeviceAddress deviceAddress) {
  char addrStr[17];
  for (uint8_t i = 0; i < 8; i++) {
    sprintf(addrStr + (i * 2), "%02X", deviceAddress[i]);
  }
  addrStr[16] = '\0';
  return String(addrStr);
}

void TemperatureHandler::stringToAddress(const String &hexString, DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    String byteStr = hexString.substring(i * 2, i * 2 + 2);
    deviceAddress[i] = (uint8_t) strtol(byteStr.c_str(), NULL, 16);
  }
}

String TemperatureHandler::getGrydeAddressString() {
  return addressToString(grydeAddress);
}

String TemperatureHandler::getVentilAddressString() {
  return addressToString(ventilAddress);
}

void TemperatureHandler::swapSensorAddresses() {
  DeviceAddress temp;
  memcpy(temp, grydeAddress, sizeof(DeviceAddress));
  memcpy(grydeAddress, ventilAddress, sizeof(DeviceAddress));
  memcpy(ventilAddress, temp, sizeof(DeviceAddress));
  // Hvis du ønsker at gemme byttet i EEPROM, kan du kalde updateSensorAddresses() eller
  // opdatere konfigurationen her.
}

// Ny metode: Læs de aktuelle sensoradresser og opdater EEPROM
bool TemperatureHandler::updateSensorAddresses() {
  // Antag at vi har mindst to sensorer
  int deviceCount = sensors.getDeviceCount();
  if (deviceCount < 2) return false;
  
  DeviceAddress addr1, addr2;
  if (!sensors.getAddress(addr1, 0) || !sensors.getAddress(addr2, 1))
    return false;
  
  String grydeStr = addressToString(addr1);
  String ventilStr = addressToString(addr2);
  
  // Opdater de globale adresser
  memcpy(grydeAddress, addr1, sizeof(DeviceAddress));
  memcpy(ventilAddress, addr2, sizeof(DeviceAddress));
  
  // Hent den nuværende config, opdater og gem den
  Config cfg = EEPROMHandler::getConfig();
  strncpy(cfg.grydeAddress, grydeStr.c_str(), sizeof(cfg.grydeAddress));
  cfg.grydeAddress[sizeof(cfg.grydeAddress)-1] = '\0';
  strncpy(cfg.ventilAddress, ventilStr.c_str(), sizeof(cfg.ventilAddress));
  cfg.ventilAddress[sizeof(cfg.ventilAddress)-1] = '\0';
  EEPROMHandler::saveConfig(cfg);
  
  return true;
}

bool TemperatureHandler::readActualSensorAddresses(String &gryde, String &ventil) {
  sensors.requestTemperatures();
  int count = sensors.getDeviceCount();
  if (count < 2) {
    return false; // Mindst to sensorer skal være tilsluttet
  }
  
  DeviceAddress addr0, addr1;
  if (!sensors.getAddress(addr0, 0) || !sensors.getAddress(addr1, 1)) {
    return false;
  }
  
  gryde = addressToString(addr0);
  ventil = addressToString(addr1);
  return true;
}
