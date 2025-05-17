#include "EEPROMHandler.h"
#include <EEPROM.h>
#include <Arduino.h>

#define EEPROM_SIZE 512
#define EEPROM_CONFIG_START 0

Config EEPROMHandler::config;

void EEPROMHandler::begin() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(EEPROM_CONFIG_START, config);
    // Hvis ssid er tom, antages der, at config ikke er blevet initialiseret korrekt.
    if (config.ssid[0] == '\0') {
        resetToDefaults();
        save();
    }
}

Config EEPROMHandler::getConfig() {
    return config;
}

//Til udskrivning af debug information:
String EEPROMHandler::getConfigAsString() {
    String s = "";
    s += "SSID: "; s += config.ssid; s += "\n";
    s += "Password: "; s += config.password; s += "\n";
    s += "IP: "; s += config.ip; s += "\n";
    s += "Gateway: "; s += config.gw; s += "\n";
    s += "Subnet: "; s += config.sn; s += "\n";
    s += "TempOffset: "; s += String(config.tempOffset); s += "\n";
    s += "Hysteresis: "; s += String(config.hysteresis); s += "\n";
    s += "MashTime: "; s += String(config.mashTime); s += "\n";
    s += "MashoutTime: "; s += String(config.mashoutTime); s += "\n";
    s += "BoilTime: "; s += String(config.boilTime); s += "\n";
    s += "MashSetpoint: "; s += String(config.mashSetpoint); s += "\n";
    s += "MashoutSetpoint: "; s += String(config.mashoutSetpoint); s += "\n";
    s += "GrydeAddress: "; s += config.grydeAddress; s += "\n";
    s += "VentilAddress: "; s += config.ventilAddress; s += "\n";
    return s;
}
  

void EEPROMHandler::saveConfig(const Config &cfg) {
    config = cfg;
    save();
}

void EEPROMHandler::resetToDefaults() {
    // Angiv default-værdier i den samme rækkefølge som felterne i Config:
    // ssid, password, ip, gw, sn,
    // tempOffset, hysteresis,
    // mashTime, mashoutTime, boilTime,
    // mashSetpoint, mashoutSetpoint,
    // grydeAddress, ventilAddress
    Config cfg = {
        "",                 // ssid
        "",                 // password
        "",                 // ip
        "",                 // gw
        "",                 // sn
        0.0,                // tempOffset
        1.0,                // hysteresis (default eksempelværdi)
        90 * 60,            // mashTime (90 minutter i sekunder)
        10 * 60,            // mashoutTime (10 minutter)
        60 * 60,            // boilTime (60 minutter)
        64.0,               // mashSetpoint (°C)
        75.0,               // mashoutSetpoint (°C)
        "28FFA1B2C3D4E5F6",  // grydeAddress (16 hex-tegn)
        "28FF112233445566"   // ventilAddress (16 hex-tegn)
    };
    saveConfig(cfg);
}

void EEPROMHandler::save() {
    EEPROM.put(EEPROM_CONFIG_START, config);
    EEPROM.commit();
}
