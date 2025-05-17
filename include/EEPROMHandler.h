#ifndef EEPROMHANDLER_H
#define EEPROMHANDLER_H

#include <Arduino.h>

struct Config {
    char ssid[32];
    char password[32];
    char ip[16];
    char gw[16];
    char sn[16];
    float tempOffset;
    float hysteresis;
    unsigned long mashTime;      // i sekunder
    unsigned long mashoutTime;   // i sekunder
    unsigned long boilTime;      // i sekunder
    float mashSetpoint;
    float mashoutSetpoint;
    char grydeAddress[17];       // 16 tegn + nullterminator (f.eks. "28FFA1B2C3D4E5F6")
    char ventilAddress[17];      // samme format som grydeAddress
};

class EEPROMHandler {
public:
    static void begin();
    static Config getConfig();
    static void saveConfig(const Config &cfg);
    static void resetToDefaults();
    static String getConfigAsString();
    
private:
    static Config config;
    static void save();
};

#endif // EEPROMHANDLER_H
