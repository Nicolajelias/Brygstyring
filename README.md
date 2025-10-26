# Brygstyring

Brygstyring er en ESP32-S3-baseret brygkontroller, der overvåger og styrer mæskning/kogning via to DS18B20-temperatursensorer, relæer til pumpe og gasventil, samt en OLED-statusskærm og webinterface.

## Funktioner
- Temperaturovervågning med to DS18B20 sensorer (gryde og ventil) på separate GPIO-busser.
- Relækontrol for pumpe og gasventil samt buzzer-alarmer og knap-input til brugerbekræftelser.
- 128×64 I²C OLED-display med processtatus, tider og temperaturer.
- Indbygget webserver med status-dashboard, proceskontrol og indstillingsside.
- WiFi STA/AP fallback med mDNS (`brygkontrol.local`).
- OTA-firmwareopdatering (`/update`) og automatisk firmware-navngivning via `rename_firmware.py`.

## Hardware
- `ESP32-S3-DevKitC-1` (8 MB Flash anbefales).
- 2 × DS18B20 temperatursensor (tre-ledet udgave).
- Relæmodul til pumpe (230 V) og gasventil (sørg for galvanisk isolation).
- Aktiv buzzer.
- Momentary-knap (benyttes til bekræftelser/start).
- SSD1306 128×64 OLED (I²C).
- Status LED samt passende forsyning/styresignaler.

| Komponent          | GPIO (ESP32-S3) | Makro i `PinConfig.h` |
|--------------------|-----------------|------------------------|
| DS18B20 gryde      | 4               | `PIN_TEMP_GRYDE`       |
| DS18B20 ventil     | 5               | `PIN_TEMP_VENTIL`      |
| Pumpe-relæ         | 18              | `PIN_PUMP`             |
| Gasventil-relæ     | 17              | `PIN_GAS`              |
| Buzzer             | 21              | `PIN_BUZZER`           |
| Knap               | 15              | `PIN_BUTTON`           |
| OLED SDA           | 8               | `PIN_OLED_SDA`         |
| OLED SCL           | 9               | `PIN_OLED_SCL`         |
| Status LED         | 10              | `PIN_STATUS_LED`       |

> **Bemærk:** DS18B20-sensorerne kører på separate datalinjer, så der er ikke behov for at håndtere sensoradresser i softwaren. Husk pull-up modstand (typisk 4.7 kΩ) på hver datalinje.

## Software & Build

### Krav
- [PlatformIO CLI](https://platformio.org/install) ≥ 6 (kræves pga. Python 3.12 kompatibilitet).
- Python 3.11/3.12 (PlatformIO installeres via `pip install -U platformio`).
- USB-driver til ESP32-S3 (CP210x eller tilsvarende).

### Kompilering
```bash
platformio run
```
Miljøet hedder `esp32-s3-devkitc-1`. Output ligger i `.pio/build/esp32-s3-devkitc-1/`. Post-build scriptet omdøber firmware til `firmware_v<SOFTWARE_VERSION>.bin` baseret på `include/Version.h`.

### Flashing
```bash
platformio run --target upload
```
Alternativt kan den genererede `.bin` uploades via OTA (`/update`).

## Første opsætning
1. Efter første boot skifter enheden til AP-tilstand (`BrygAP`, IP 192.168.4.1).
2. Besøg `http://192.168.4.1/settings` og indtast WiFi-oplysninger.
3. Når enheden forbinder til dit netværk, kan UI’et nås via `http://brygkontrol.local/` eller den tildelte IP.

## Webinterface
- **Status**: Live temperaturer, procestrin, pumpe/gas-status, tidsinformation.
- **Proceskontrol**: Start/stop/pause/resume for mæskning, mashout og kogning.
- **Indstillinger**: WiFi-parametre, tider, setpoints, hysterese, ventil-offset.
- **OTA**: Tilgå `/update` for at uploade ny firmware (kræver `.bin` fra build).
- **Debug**: `/debug` returnerer den aktuelle EEPROM-konfiguration som tekst.

## EEPROM & Indstillinger
Konfigurationen (WiFi, temperaturparametre osv.) gemmes i ESP32’ens interne EEPROM-emulering. `EEPROMHandler::resetToDefaults()` nulstiller værdierne, hvorefter enheden genstarter.

## Fejlfinding
- **PlatformIO 4.x fejl (`resultcallback`)**: Opgrader til seneste PlatformIO CLI (`pip install -U platformio`).
- **Ingen temperaturer**: Kontroller pull-up modstande og kabelføring. Da hver sensor har sin egen pin, skal begge have 3.3 V, GND og data med pull-up.
- **WiFi forbinder ikke**: Kontrollér kredsoplysninger i UI’et og genstart. Enheden falder tilbage til AP-tilstand efter timeout.

## Filstruktur (uddrag)
```
├── include/
│   ├── PinConfig.h          # Central pin-konfiguration
│   └── Version.h            # Software-version
├── src/
│   ├── main.cpp             # App-entry, setup/loop
│   ├── TemperatureHandler.cpp# DS18B20 håndtering
│   ├── WebServerHandler.cpp # Webserver & UI
│   ├── WiFiHandler.cpp      # WiFi + mDNS
│   └── ...                  # Proces, display, OTA mm.
├── platformio.ini           # PlatformIO miljø-konfiguration
└── rename_firmware.py       # Post-build omdøbning af firmware.bin
```

## Licens
Angiv licensinformation her (juster efter projektets behov).
