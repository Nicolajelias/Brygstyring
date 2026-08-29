# Sikker OTA-opdatering

Brygstyringens OTA-side findes på `/update` og bruger samme sikre forløb som
frysetørrer-firmwaren:

- Digest-login og midlertidig spærring efter fem mislykkede loginforsøg.
- Opdatering accepteres kun, når processen er stoppet, og pumpe og gas er slukket.
- Image-headeren kontrolleres, så kun firmware til ESP32-S3 skrives.
- Firmwaren valideres helt før automatisk genstart.
- Den nye app markeres først som stabil efter 30 sekunders sund drift; ellers kan
  ESP32-bootloaderens rollback træde i kraft.
- Seneste resultat og boot-validering gemmes i NVS og vises via status-API'et.

## Opret login

Kopiér `include/secrets.example.hpp` til `include/secrets.hpp`, og erstat
eksempelværdierne:

```cpp
inline constexpr const char* OTA_USERNAME = "admin";
inline constexpr const char* OTA_PASSWORD = "en-unik-adgangskode-på-mindst-12-tegn";
```

`include/secrets.hpp` ignoreres af Git. Uden filen, eller med en adgangskode på
under 12 tegn, svarer `/update` med HTTP 503 og OTA er deaktiveret.

## Opdatering

1. Byg firmwaren med PlatformIO.
2. Stop brygprocessen, og kontrollér at pumpe og gas er slukket.
3. Åbn `http://brygkontrol.local/update` og log ind.
4. Vælg den versionsnavngivne `.bin` fra `.build-artifacts/`.
5. Vent på validering og automatisk genstart.

Den eksisterende 16 MB-partitionstabel har allerede to OTA-app-partitioner og
bevarer SPIFFS-området med opskrifter. Der kræves derfor ingen ændring af
partitionstabellen.
