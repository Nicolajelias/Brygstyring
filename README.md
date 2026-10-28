# Brygstyring

Brygstyring er en ESP32-S3-baseret brygkontroller, der overvåger og styrer mæskning/kogning via to 3-leder PT100-temperatursensorer med hver sin MAX31865, relæer til pumpe og gasventil, samt en OLED-statusskærm og webinterface.

## Funktioner
- Robust, ikke-blokerende temperaturovervågning med to 3-leder PT100-sensorer, fælles SPI-bus, 50 Hz-filter, friskhedskontrol og automatisk geninitialisering ved fejl.
- Relækontrol for pumpe og gasventil samt buzzer-alarmer og knap-input til brugerbekræftelser.
- 128×64 I²C OLED-display med processtatus, tider og temperaturer.
- Indbygget webserver med status-dashboard, proceskontrol og indstillingsside.
- Navngivet opskriftsbibliotek med op til 12 komplette brygprofiler og otte humlealarmer pr. opskrift.
- Git-baseret SemVer, buildnummer, commit-hash, SHA-256 og firmwaremanifest.
- WiFi STA/AP fallback med mDNS (`brygkontrol.local`).
- RGB status-LED med farvekoder for WiFi/AP og animationsmode under aktiv brygproces.
- Loginbeskyttet og valideret OTA-firmwareopdatering (`/update`) med rollback.

## Hardware
- `ESP32-S3-DevKitC-1` (projektets build er konfigureret til 16 MB flash og 8 MB OPI PSRAM).
- 2 × PT100 temperatursensor, 3-leder, IEC 60751 (100 ohm ved 0 °C).
- 2 × MAX31865 til PT100, hver med 430 ohm præcisionsreference og korrekt 3-lederforbindelse.
- Relæmodul til pumpe (230 V) og gasventil (sørg for galvanisk isolation).
- Aktiv buzzer.
- Momentary-knap (benyttes til bekræftelser/start).
- SSD1306 128×64 OLED (I²C).
- Adresserbar RGB status-LED med GPIO48 data. Separat strøm-enable er deaktiveret i den aktuelle pin-konfiguration.

| Komponent          | GPIO (ESP32-S3) | Makro i `PinConfig.h` |
|--------------------|-----------------|------------------------|
| MAX31865 gryde CS  | 4               | `PIN_TEMP_GRYDE`       |
| MAX31865 ventil CS | 5               | `PIN_TEMP_VENTIL`      |
| MAX31865 fælles SCK | 12             | `PIN_TEMP_SCK`         |
| MAX31865 fælles SDI/MOSI | 13        | `PIN_TEMP_MOSI`        |
| MAX31865 fælles SDO/MISO | 14        | `PIN_TEMP_MISO`        |
| Pumpe-relæ         | 18              | `PIN_PUMP`             |
| Gasventil-relæ     | 17              | `PIN_GAS`              |
| Buzzer             | 21              | `PIN_BUZZER`           |
| Knap               | 15              | `PIN_BUTTON`           |
| OLED SDA           | 8               | `PIN_OLED_SDA`         |
| OLED SCL           | 9               | `PIN_OLED_SCL`         |
| RGB status LED     | 48              | `PIN_RGB_LED`          |
| RGB LED strøm      | Ingen (`-1`)    | `PIN_RGB_LED_PWR`      |

> **PCB-grundlag:** Se [docs/hardware-pt100.md](docs/hardware-pt100.md) for alle forbindelser, MAX31865-chipben, sensorstik, referencekomponenter og sikkerhedskrav. PT100 er passive modstandssensorer og må ikke tilsluttes som de tidligere digitale sensorer.

## Software & Build

### Krav
- [PlatformIO CLI](https://platformio.org/install) ≥ 6 (kræves pga. Python 3.12 kompatibilitet).
- Python 3.11/3.12 (PlatformIO installeres via `pip install -U platformio`).
- USB-driver til ESP32-S3 (CP210x eller tilsvarende).

### Kompilering
```bash
platformio run
```
Miljøet hedder `esp32-s3-devkitc-1-16mb-psram`. Almindeligt buildoutput ligger i `.pio/build/esp32-s3-devkitc-1-16mb-psram/`; versionsnavngivet firmware, SHA-256 og manifest ligger i `.build-artifacts/esp32-s3-devkitc-1-16mb-psram/`.

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
- **Sensorsikkerhed**: Dashboardet viser målingernes alder og retry-status. Efter fem sekunder uden en gyldig måling markeres sensoren som utilgængelig, og gas-failsafe aktiveres.
- **Proceskontrol**: Start/stop/pause/resume for mæskning, mashout og kogning.
- **Opskrifter**: `/recipes` gemmer, redigerer, henter og sletter komplette brygprofiler med humletilsætninger.
- **Indstillinger**: WiFi-parametre, tider, setpoints, hysterese, ventil-offset.
- **OTA**: Tilgå `/update` for en sikker, loginbeskyttet opdatering. Opsætning er beskrevet i [docs/ota.md](docs/ota.md).
- **Debug**: `/debug` returnerer den aktuelle EEPROM-konfiguration som tekst.

## EEPROM & Indstillinger
Konfigurationen (WiFi, temperaturparametre osv.) gemmes i ESP32’ens interne EEPROM-emulering. `EEPROMHandler::resetToDefaults()` nulstiller værdierne, hvorefter enheden genstarter.

## Fejlfinding
- **PlatformIO 4.x fejl (`resultcallback`)**: Opgrader til seneste PlatformIO CLI (`pip install -U platformio`).
- **Ingen/forkerte temperaturer**: Kontrollér MAX31865-forsyning, SPI, individuelle CS-signaler, 430 ohm reference og både fysisk og softwaremæssig 3-lederkonfiguration. De to ledninger fra samme ende af PT100-elementet skal føres separat til FORCE+ og RTDIN+. Se PCB-grundlaget; de gamle OneWire-pull-ups skal ikke bruges.
- **WiFi forbinder ikke**: Kontrollér kredsoplysninger i UI’et og genstart. Enheden falder tilbage til AP-tilstand efter timeout.

## Filstruktur (uddrag)
```
├── include/
│   ├── PinConfig.h          # Central pin-konfiguration
│   └── Version.h            # Software-version
├── src/
│   ├── main.cpp             # App-entry, setup/loop
│   ├── TemperatureHandler.cpp# PT100/MAX31865 håndtering
│   ├── WebServerHandler.cpp # Webserver & UI
│   ├── WiFiHandler.cpp      # WiFi + mDNS
│   └── ...                  # Proces, display, OTA mm.
├── platformio.ini           # PlatformIO miljø-konfiguration
└── rename_firmware.py       # Post-build omdøbning af firmware.bin
```

## Licens
Angiv licensinformation her (juster efter projektets behov).
