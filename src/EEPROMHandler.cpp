#include "EEPROMHandler.h"
#include <EEPROM.h>
#include <Arduino.h>
#include <math.h>
#include <string.h>

#define EEPROM_SIZE 512
#define EEPROM_CONFIG_START 0

Config EEPROMHandler::config;

namespace {
bool hasTerminator(const char* value, size_t capacity) {
    return memchr(value, '\0', capacity) != nullptr;
}

bool isConfigSane(const Config& cfg) {
    return hasTerminator(cfg.ssid, sizeof(cfg.ssid)) &&
           hasTerminator(cfg.password, sizeof(cfg.password)) &&
           hasTerminator(cfg.ip, sizeof(cfg.ip)) &&
           hasTerminator(cfg.gw, sizeof(cfg.gw)) &&
           hasTerminator(cfg.sn, sizeof(cfg.sn)) &&
           isfinite(cfg.tempOffset) && cfg.tempOffset >= 0.0f && cfg.tempOffset <= 30.0f &&
           isfinite(cfg.hysteresis) && cfg.hysteresis >= 0.1f && cfg.hysteresis <= 10.0f &&
           cfg.mashTime >= 60 && cfg.mashTime <= 300UL * 60UL &&
           cfg.mashoutTime >= 60 && cfg.mashoutTime <= 120UL * 60UL &&
           cfg.boilTime >= 60 && cfg.boilTime <= 300UL * 60UL &&
           isfinite(cfg.mashSetpoint) && cfg.mashSetpoint >= 20.0f && cfg.mashSetpoint <= 100.0f &&
           isfinite(cfg.mashoutSetpoint) && cfg.mashoutSetpoint >= 20.0f && cfg.mashoutSetpoint <= 100.0f;
}
}

void EEPROMHandler::begin() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(EEPROM_CONFIG_START, config);
    // Hvis ssid er tom, antages der, at config ikke er blevet initialiseret korrekt.
    if (!isConfigSane(config)) {
        Serial.println("[EEPROM] Invalid configuration; restoring safe defaults");
        resetToDefaults();
    }
}

Config EEPROMHandler::getConfig() {
    return config;
}

//Til udskrivning af debug information:
String EEPROMHandler::getConfigAsString() {
    String s = "";
    s += "SSID: "; s += config.ssid; s += "\n";
    s += "Password: [hidden]\n";
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
    // mashSetpoint, mashoutSetpoint
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
        75.0                // mashoutSetpoint (°C)
    };
    saveConfig(cfg);
}

void EEPROMHandler::save() {
    EEPROM.put(EEPROM_CONFIG_START, config);
    EEPROM.commit();
}
