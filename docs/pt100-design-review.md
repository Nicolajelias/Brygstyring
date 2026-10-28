# PT100: kodekontrol og grundlag for PCB-design

## Status efter rettelser og tilføjelse af kalibrering

**Modulvalg:** Brugeren har efterfølgende vist et lilla MAX31865-breakout
med VIN/GND/3V3/CLK/SDO/SDI/CS/DRDY. Carrier-PCB'et planlægges nu med to
af disse moduler. [Hardwaredokumentet](hardware-pt100.md) indeholder den
tilpassede forbindelsesoversigt. Referenceværdi, jumpere og mekaniske mål
er endnu ikke verificeret; billedet viser kun undersiden. Den tidligere
netliste til løse MAX31865-chips er et alternativ, ikke ekstra komponenter
der skal monteres sammen med modulerne.

Opdateret 28. oktober 2026. F1-F4 er rettet i koden og testet med simuleret
SPI/MAX31865, flash og GPIO samt de faktiske temperatur-, proces- og
kalibrerings-HTTP-handlere. Firmwarebuild bestod. Ingen firmware er uploadet
til anlægget som del af denne ændring.

Valideret build: `2.2.0-dev.2+g52de072.dirty`, build 15, PlatformIO Espressif32
7.0.1 og Adafruit MAX31865 1.6.2. RAM: 53.480 byte; programstørrelse: 986.565 byte.
Firmware SHA-256: `6fab0e8ead9a0546b8b74b4d77be66fbc844e8d6267b91effc71d3157f392f91`.
De otte grupper af host-regressionstests bestod, inklusive de faktiske
kalibrerings-HTTP-handlere. JavaScript-syntaks, lokale dokumentlinks og
`git diff --check` er også kontrolleret.

| Fund | Aktuel status |
| --- | --- |
| F1 | Central sensorinterlock lukker gas i alle seks driftstilstande; alle gas-ON-veje kontrollerer begge sensorer. Første målefejl spærrer gas. |
| F2 | Minimum 70 ms konverteringsventetid, kontrol af 1SHOT og timeout ved 110 ms. |
| F3 | Konfiguration og thresholds verificeres; one-shot-start skal kvitteres ved readback før afslutning accepteres. Tidsstempel sættes til konverteringsstart. |
| F4 | Manuel fault-detection-cyklus før hver måling, settling i begge switchtilstande og kontrol af fault-register. |
| F5 | Brugeren har bekræftet ESP32-S3-DevKitC-1 N16R8. Boardnavnet er rettet; 16 MB QIO flash og 8 MB OPI PSRAM er bevaret. |
| F6 | LED-data kan vælges som GPIO48 eller GPIO38 via `BRYG_RGB_LED_GPIO` i platformio.ini. Fysisk boardrevision er ikke oplyst; GPIO48 er bevaret som standard. |
| F7 | README angiver nu passiv buzzer/transducer og passende driver til 750 Hz. |

Separat ét-/topunktskalibrering, validering og flashlagring er tilføjet.
Den aktuelle driftsbeskrivelse findes i [hardwaredokumentet](hardware-pt100.md)
og [kalibreringsvejledningen](pt100-calibration.md). De simulerede tests beviser
ikke analog målenøjagtighed, alle fysiske ledningsfejl eller fysisk afbrydelsestid.
PCB-/hardwaretestplanen skal fortsat udføres. Kør `python scripts/test_pt100.py`
på Windows med PlatformIOs MinGW-toolchain for at gentage regressionstestene.

**Resten af rapporten er den historiske gennemgang før rettelserne.** Fil- og
linjehenvisninger, den tidligere buildhash og beskrivelser af åbne firmwarefejl
nedenfor dokumenterer den oprindelige tilstand og er overhalet af status ovenfor.

## Historisk gennemgang før rettelser

Dato: 28. oktober 2026. Gennemgået arbejdsmappe ved Git HEAD `6cd08c7`, inklusive lokale, ikke-committede PT100-ændringer.

## Konklusion og valideringsstatus

**Forbindelsesplanen til to 3-leder PT100 med hver sin MAX31865 er elektrisk korrekt på netniveau. Firmwaren bygger, men har fejl og begrænsninger, som skal løses før idriftsættelse. Grundlaget kan bruges til at tegne et lavspændings-carrier-PCB; det er ikke en frigivelse til produktion eller drift med gas.**

Følgende er kontrolleret: kodeændringer mod Git, alle anvendte GPIO, SPI-registeradgang, installeret MAX31865-bibliotek, sensorforbindelser og TQFN-benkort mod primærkilder. Der foreligger intet faktisk PCB-skema, layout eller BOM i det gennemgåede materiale, og intet tilsluttet anlæg er afprøvet. ERC/DRC, footprint, EMC, målenøjagtighed og den fysiske sikkerhedskæde er derfor åbne punkter.

Den tidligere beskrivelse af, at sensorfejl spærrer varmestyringen, gælder **ikke alle driftstilstande**. Den vigtigste afvigelse er manuel gasstyring i IDLE/PAUSED.

## Fund, der kræver handling

| ID | Prioritet | Fund og evidens | Nødvendig handling |
| --- | --- | --- | --- |
| F1 | Kritisk før drift | `ProcessHandler::toggleGasValve()` kontrollerer kun sensorgyldighed ved manuel indkobling. `update()` foretager ingen vedvarende sensorinterlock i IDLE eller PAUSED. Manuel gas kan derfor forblive tændt efter udløbet af gyldige målinger. Dette er eksisterende kode, ikke en ny PT100-regression. | Indfør en central gasinterlock, som gælder alle tilstande og alle indkoblingsveje. Test manuel ON efterfulgt af sensorbrud i både IDLE og PAUSED. |
| F2 | Høj | `TemperatureHandler.cpp:13` bruger 65 ms. MAX31865-databladets elektriske karakteristik angiver 66 ms maksimum for en enkelt 50 Hz-konvertering. Koden kan læse den tidligere værdi og slukke bias før den nye konvertering er afsluttet. | Brug konservativ ventetid, eksempelvis 70 ms, og verificer afslutning med DRDY eller tilbageaflæsning af det selvnulstillende 1SHOT-bit samt timeout. |
| F3 | Høj | `readSensor()` opdaterer `lastGoodMs` ved en plausibel registerværdi uden bevis for en ny konvertering. `begin()` fra Adafruit returnerer heller ikke et chip-ID eller en egentlig presence-test. En konfigurations-/kommunikationsfejl kan dermed i visse fejltilfælde gøre en gammel registerværdi til en tilsyneladende frisk måling. | Kontroller konfiguration og konverteringsforløb. Friskhed må kun opdateres efter en dokumenteret afsluttet konvertering. Uændret temperatur alene er ikke bevis på fejl. |
| F4 | Høj | `readFault(MAX31865_FAULT_NONE)` aflæser kun eksisterende fault-status. Den særlige fault-detection-cyklus initieres aldrig. Bibliotekets standardtærskler sættes til 0 og 0xFFFF. Det er derfor ikke dokumenteret, at alle ledningsfejl giver fault eller en temperatur uden for softwareintervallet. | Tilføj en planlagt fault-detection-cyklus med korrekt settling for det valgte filter, og afprøv hver ledning samt relevante kortslutninger. Almindelige fault-bits og softwareinterval er nyttige, men dækker ikke alene alle fejl. |
| F5 | Høj for BOM | Board-JSON hedder `...16mb-psram`, men dets visningsnavn indeholder N8R8. N8R8 betegner 8 MB flash og 8 MB PSRAM; buildet bruger 16 MB flash og OPI PSRAM. Den faktiske hardwarevariant er ukendt. | Lås præcist DevKit-/modulbestillingsnummer og hukommelse i BOM. Ret board-definition eller hardwarevalg, så de stemmer. Buildsucces beviser ikke monteret flashstørrelse. |
| F6 | Middel for PCB | `PIN_RGB_LED = 48` passer til det oprindelige officielle DevKitC-1. Officiel revision 1.1 bruger GPIO38 til onboard-RGB-LED. | Lås boardrevision. Ved rev. 1.1 skal firmware bruge GPIO38 til onboard-LED; alternativt definér en særskilt ekstern LED på GPIO48. GPIO38 er data på rev. 1.1, ikke power-enable. |
| F7 | Middel | README angiver aktiv buzzer, mens firmwaren genererer en 750 Hz tone. Hardwaredokumentet beskriver mere korrekt en passiv transducer eller en aktiv buzzer med typespecifik reaktion. | Vælg konkret buzzer og driver. En passiv transducer passer til den eksisterende tonestyring. |

F2-F4 følger af sammenholdning af `src/TemperatureHandler.cpp`, det installerede Adafruit-bibliotek 1.6.2 og [MAX31865-databladet, rev. 3, side 3, 8 og 11-15](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX31865.pdf). De beskrevne fejlmekanismer er kode-/databladsanalyse; de er ikke målt på et fysisk MAX31865-kredsløb.

For F1 kan forløbet efterprøves direkte i koden: gyldige temperaturer → manuel gas ON → sensor ugyldig/for gammel → `update()` i IDLE uden `gasControl(false)`. PAUSED har heller ingen gren, som lukker gas ved sensorfejl. En automatisk proces, der genoptages efter recovery, kræver desuden en bevidst beslutning om genstartspolitik; koden låser ikke generelt en sensorfejl, indtil operatøren kvitterer.

## Gennemgang af kodeændringer

| Fil | Vurdering |
| --- | --- |
| `include/PinConfig.h` | CS på GPIO4/5 er bevaret, nye SPI-signaler på GPIO12/13/14 er indført. Ingen dobbeltallokering mellem aktive GPIO. LED-revision skal afklares, jf. F6. |
| `include/TemperatureHandler.h` | DallasTemperature-include er fjernet. Eksisterende offentlige temperatur-/diagnostikfunktioner er bevaret. |
| `src/TemperatureHandler.cpp` | To hardware-SPI-enheder med separate CS, 3-wire og 50 Hz er implementeret. Registerretning, MSB-first, SPI mode 1 ved 1 MHz, 15-bit råværdi (`raw >> 1`) og temperaturberegning med 100/430 ohm stemmer. F2-F4 skal håndteres. |
| `platformio.ini` | OneWire og DallasTemperature er erstattet af Adafruit MAX31865. Det installerede bibliotek er 1.6.2. Platformen er ikke versionslåst; arkivér de faktisk anvendte versioner ved frigivelse. |
| `src/main.cpp` | Kun DS18B20-kommentaren er fjernet i diffen. Kaldet til den nye måletilstandsmaskine er bevaret. |
| `README.md` | Overordnet opdateret til PT100, SPI og 430 ohm. Buzzer og generelle robusthedspåstande skal præciseres. |
| `docs/hardware-pt100.md` | Sensorernes netforbindelser og TQFN-benkort er korrekte. Påstanden om generel gas-OFF ved udløb skal begrænses; LED-revision og firmwarefund skal fremgå. |
| `.vscode/extensions.json` | Separat ændring i frarådede udvidelser. Ingen elektrisk eller firmwaremæssig virkning. |

Denne gennemgang ændrer ikke firmwaren. Nedenstående beskriver derfor både den aktuelle adfærd og krav til rettelser.

### Aktuel måling og fejlreaktion

Måletilstandsmaskinen aktiverer bias, venter nominelt mindst 10 ms, starter begge kanaler og læser efter nominelt mindst 65 ms. Næste cyklus anmodes efter cirka ét sekund regnet fra biasstart. Hver kanal har sin egen værdi, alder og fejltæller. Der anvendes intervallet -20 til +125 °C; 85 °C behandles korrekt som en almindelig temperatur.

Ved fejl bevares seneste gyldige værdi, indtil dens alder er **over** 5000 ms. `healthy` bliver falsk ved første registrerede fejl, men styringen bruger `valid`/temperaturværdi og kan fortsat anvende den gamle værdi inden udløb. Hver tredje fejl geninitialiseres kanalen. Fejltælleren mætter ved 255; da 255 er deleligt med tre, geninitialiseres derefter ved hver fejl, indtil en måling lykkes. Dette er en mindre diagnostisk detalje.

Ved ugyldige målinger lukker MASHING/MASHOUT gas gennem `temperatureControl()`, men pumpen forbliver tændt. BOILHEATUP/BOILING slukker både gas og pumpe. IDLE/PAUSED har hullet i F1. I kogefaserne er der kun kontrol for endelige temperaturværdier; ventilens setpoint+offset-beskyttelse fra mæskningen bruges ikke dér. En særskilt fysisk overtemperatursikring er derfor nødvendig i det samlede anlægsdesign.

Fem sekunder er en friskhedsgrænse, **ikke en garanteret fysisk afbrydelsestid**. Lukning kræver eksekvering af kontrolløkken. Blandt andet kan `NTPClient::update()` blokere omkring ét sekund uden serversvar; webarbejde og øvrige forsinkelser påvirker også løkken. Opstarten har desuden animation og to 5-sekunders ventetider, hvor temperaturtilstandsmaskinen endnu ikke serviceres. Kortsluttede eller afbrudte SPI-datalinjer, som returnerer udelukkende nuller eller ettaller, afvises normalt af de eksisterende plausibilitets-/faultkontroller; det beviser ikke beskyttelse mod et fastfrosset plausibelt registerindhold.

## PCB-forudsætning og GPIO-/headerplan

Denne plan gælder et carrier-PCB med et ESP32-S3-DevKitC-1. Numrene i kolonnen GPIO er **ikke** chipben eller ESP32-modulben. Headerreferencerne DEV.J1/DEV.J3 nedenfor er Espressifs officielle DevKit-headernavne; de må ikke forveksles med sensorstikkenes navne. Kontroller monteringsretning, pin 1, rækkeafstand og boardmål mod det indkøbte board, især ved kloner.

| Funktion | GPIO | Officiel DevKit-header | Elektrisk forbindelse |
| --- | ---: | --- | --- |
| Gryde CS | 4 | DEV.J1.4 | Til U_GRYDE CS, 10 kohm til 3V3 |
| Ventil CS | 5 | DEV.J1.5 | Til U_VENTIL CS, 10 kohm til 3V3 |
| SPI SCK | 12 | DEV.J1.18 | Til begge SCLK |
| SPI MOSI | 13 | DEV.J1.19 | Til begge SDI |
| SPI MISO | 14 | DEV.J1.20 | Fra begge SDO |
| Pumpe | 18 | DEV.J1.11 | Aktiv-HIGH, 3,3 V kompatibel driver |
| Gasventil | 17 | DEV.J1.10 | Aktiv-HIGH, 3,3 V kompatibel driver |
| Buzzer | 21 | DEV.J3.18 | 750 Hz tone til driver |
| Knap | 15 | DEV.J1.8 | Normalt åben kontakt til GND |
| OLED SDA | 8 | DEV.J1.12 | SSD1306 I2C SDA, pull-up til 3V3 |
| OLED SCL | 9 | DEV.J1.15 | SSD1306 I2C SCL, pull-up til 3V3 |
| RGB data, aktuel kode | 48 | DEV.J3.16 | Onboard ved oprindelig revision; jf. F6 |
| RGB data, officiel rev. 1.1 | 38 | DEV.J3.10 | Kræver ændring fra den aktuelle kode |
| 3V3 | - | DEV.J1.1 eller .2 | Til MAX31865 og egnet OLED |
| GND | - | DEV.J1.22 / DEV.J3.1, .21, .22 | Fælles lavspændingsstel |
| Ekstern 5 V-indgang | - | DEV.J1.21 | Kun efter valgt forsynings-/USB-topologi |

Headerplan og revisionsforskel er kontrolleret i [Espressifs oprindelige hardwareguide](https://documentation.espressif.com/api/resource/path/docs/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html) og [rev. 1.1-guiden](https://documentation.espressif.com/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html).

Alle SPI-signaler er 3,3 V. GPIO12/13/14 anvendes som omdirigerede hardware-SPI-signaler; de behøver ikke følge navnene på chippenes alternative standardfunktioner. CS er aktiv LOW, og SDO fra den fravalgte chip er højimpedant. DRDY anvendes aktuelt ikke. Reserver mindst et separat testpunkt pr. DRDY; tilslut dem til hver sin ledige GPIO, hvis færdigmelding vælges som firmwareløsning. DRDY er push-pull og må ikke uden videre sammenkobles mellem kanalerne.

GPIO0/3/45/46 er boot-strapping-ben. GPIO19/20 bruges til native USB, GPIO43/44 til UART. GPIO35/36/37 må ikke bruges ved den relevante Octal-hukommelsesvariant. Der er ingen konflikt mellem disse reserveringer og den aktuelle plan.

## Gentaget PT100-kanal: netliste til skemategning

Implementér nedenstående én gang for GRYDE og én gang for VENTIL. Kun SPI, forsyning og GND deles; sensor- og referencenet holdes adskilt mellem kanalerne.

Vælg **MAX31865ATP+**, 20-ben TQFN med exposed pad, 5 × 5 mm. Tabellen nedenfor gælder denne kapsling. SSOP-varianten MAX31865AAP+ har andre bennumre og kan ikke benytte tabellen direkte. Producenten angiver TQFN package code T2055+5, outline 21-0140 og land pattern 90-0010; kontroller det konkrete CAD-footprint mod disse tegninger.

| TQFN-ben | Producentens signalnavn | Net/tilslutning pr. kanal |
| ---: | --- | --- |
| 1 | BIAS | REF_TOP |
| 2 | REFIN+ | REF_TOP, separat senseføring til Rref's øvre pad |
| 3 | REFIN- | REF_BOTTOM, separat senseføring til Rref's nedre pad |
| 4 | ISENSOR | REF_BOTTOM |
| 5 | FORCE+ | SENS_A / sensorstik 1 |
| 6 | FORCE2 | SENS_A, lokal forbindelse til ben 5 |
| 7 | RTDIN+ | SENS_B / sensorstik 2 |
| 8 | RTDIN- | SENS_C / sensorstik 3 |
| 9 | FORCE- | SENS_C, lokal forbindelse til ben 8 |
| 10 | GND2 | GND |
| 11 | SDI | GPIO13 |
| 12 | SCLK | GPIO12 |
| 13 | CS | GPIO4 (gryde) eller GPIO5 (ventil) |
| 14 | SDO | GPIO14 |
| 15 | DGND | GND |
| 16 | GND1 | GND |
| 17 | N.C. | Ingen forbindelse |
| 18 | DRDY | Separat testpunkt, aktuelt uden GPIO |
| 19 | DVDD | 3V3, egen 100 nF til GND |
| 20 | VDD | 3V3, egen 100 nF til GND |
| EP | Exposed pad | GND; korrekt loddeland/viaudformning |

Benkortet følger [producentens Pin Description og 3-wire application circuit, side 8 og 24](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX31865.pdf). GND1/GND2/DGND er de præcise producentnavne; de to AGND-betegnelser i det tidligere dokument er forståelige, men mindre præcise.

### Passiver og forbindelser pr. kanal

| Del | Værdi/krav | Mellem net |
| --- | --- | --- |
| Rref | 430 ohm, foreslået 0,01 %, højst 15 ppm/°C | REF_TOP og REF_BOTTOM |
| C_RTD | 100 nF, stabil dielektrikum, gerne C0G hvis praktisk | SENS_B og SENS_C |
| C_DVDD | 100 nF keramisk, tæt ved ben 19 | DVDD og GND |
| C_VDD | 100 nF keramisk, tæt ved ben 20 | VDD og GND |
| C_bulk | 1-10 µF, vælges efter regulator/PDN | 3V3 og GND |
| R_CS | 10 kohm | CS og 3V3 |
| J_SENSOR | 3-polet, låsbart og entydigt polariseret | SENS_A, SENS_B, SENS_C |

Der skal være to uafhængige Rref. 400 ohm i et databladseksempel eller 4,3 kohm på et PT1000-modul passer ikke til firmwarens 430-ohm-konstant. 0,1 % kan anvendes med et passende nøjagtighedsbudget; ved 100 °C kan referencefejlen alene omtrent svare til 0,36 °C, mod cirka 0,036 °C ved 0,01 %. Disse er beregnede referencebidrag, ikke samlet systemnøjagtighed.

### Sensorstik og multimeterkontrol

| Stikben | Til PT100 | Chipnet |
| ---: | --- | --- |
| 1 | Første leder fra den dobbelte ende, ofte rød | FORCE+ og FORCE2 |
| 2 | Anden leder fra samme ende, ofte rød | RTDIN+ |
| 3 | Enkelt leder fra modsatte ende, ofte hvid | RTDIN- og FORCE- |

**FORCE+ og RTDIN+ må ikke kortsluttes ved PCB eller stik.** Samlingen mellem deres sensorledere findes ved selve RTD-elementet. RTDIN- og FORCE- samles lokalt ved chippen. Ingen af de tre sensorben forbindes til 3V3 eller GND. FORCE2 forbindes til FORCE+, ikke GND, i netop denne 3-ledertopologi.

Find de to ledere med lav indbyrdes modstand med et multimeter; mellem hver af dem og den tredje leder skal der være cirka 100-110 ohm ved 0-25 °C plus kabelmodstand. Farver alene er utilstrækkelige. Ledningsmodstandene skal være så ens som muligt, inklusive stik og baner. Kabelskærm får sin egen chassis-/EMC-forbindelse, hvis valgt; den er ikke en af målelederne.

### Hvis MAX31865-moduler anvendes

SPI- og sensorplanen er den samme. På et originalt Adafruit-modul kan 3,3 V føres til VIN; 3Vo er en regulatorudgang. Tilpas begge hardwarejumpere til 3-lederdrift efter [Adafruits vejledning](https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/rtd-wiring-config), og kontroller netforbindelserne med multimeter før spænding. Andre modulproducenter kan have anden regulator, pinrækkefølge, referenceværdi og jumperudformning. Modulernes færdige passiver skal ikke duplikeres ukritisk på carrier-PCB'et. Se også [Adafruits åbne referenceskema](https://github.com/adafruit/Adafruit-MAX31865-PCB).

## Øvrig hardware, forsyning og layout

OLED er SSD1306 128 × 64, I2C-adresse **0x3C** i den aktuelle kode. Forsyn med 3,3 V, hvis den konkrete model understøtter det. SDA/SCL får typisk 4,7 kohm til 3V3; medregn eksisterende pull-ups på modulet. Knap til GND får firmware-pull-up og eventuelt ekstern 10 kohm til 3V3. Buzzer, pumpe og gasventil skal have dimensionerede drivertrin; ingen spoler eller andre effektbelastninger direkte på GPIO.

Pumpe-/gasdrivere skal være aktiv-HIGH med ekstern default OFF, eksempelvis 10 kohm pulldown på et egnet 3,3 V driverinput. MOSFET-valg skal baseres på garanteret modstand ved den anvendte gatespænding, ikke blot tærskelspændingen. Vælg gate-/basismodstand, beskyttelse og effektstik ud fra konkret spænding, strøm og lasttype. DC-spolers friløbs-/clampnetværk skal også opfylde kravet til hurtig ventilafbrydelse. Et aktiv-LOW relæmodul kan ikke kobles til denne firmware uden en bevidst logiktilpasning.

Definér én forsyningsvej til DevKit og undgå USB-tilbagefødning. Dimensionér 3V3-regulatoren til hele lasten og WiFi-spidser. Relæ-/ventilforsyning og returstrømme holdes væk fra måledelen. Ved galvanisk isolerede drivere må isolationens to stel ikke forbindes utilsigtet. Alle forbindelser i tabellerne vedrører logiksiden.

Rref og MAX31865 placeres væk fra varme regulatorer, WiFi-antenne og effektkomponenter. Brug korte senseføringer, lokalt placeret C_RTD, sammenhængende stelplan og Kelvinføring ved Rref. Hold SPI på PCB. Sensorstik får passende ESD/transientbeskyttelse med dokumenteret lækstrøm og målefejl. Beskyttelsesmodstande og filtre skal dimensioneres symmetrisk og indgå i settling-/fejltest; vilkårlige seriekomponenter kan ødelægge 3-lederkompensationen. De gamle OneWire-pull-ups fjernes.

For en ekstern 5 V adresserbar LED kræves kontrol af dens logiktærskel; anvend om nødvendigt en egnet levelshifter, eksempelvis 74AHCT125, samt lokal afkobling. Lås LED-type og boardrevision i BOM.

Der er ikke angivet konkrete pumpe-, gasventil-, relæ-, forsynings- eller stikmodeller. Effektdelen kan derfor ikke færdigdimensioneres ud fra projektet. Det samlede gasanlæg skal have en fysisk sikkerhedskæde med normalt lukket ventil, flammeovervågning og uafhængig overtemperatur-/nødafbrydelse. Temperaturfirmwaren udgør ikke en dokumenteret brændersikkerhedsstyring. Isolation, krybeafstande, sikringer og eventuel netspænding skal fastlægges i det konkrete eldesign.

## Udført validering

| Kontrol | Resultat og grænse |
| --- | --- |
| Git-diff og tilstødende styringskode | Gennemgået; F1-F7 ovenfor. Ingen firmware ændret i denne reviewrunde. |
| GPIO-kollisioner | Ingen mellem de 12 aktive GPIO-konstanter. `PIN_RGB_LED_PWR = -1` er deaktiveret. |
| Beregnede testmål | CVD-værdierne ved 0/65/100 °C er genberegnet. Ideelle afrundede 15-bit ADC-koder ved 430 ohm: 7620 / 9538 / 10555. Dette er regnekontrol, ikke sensormåling. |
| Sensor-/TQFN-forbindelser | Sammenholdt med primære producentkilder. Intet CAD-skema foreligger til netlistekontrol. |
| Adafruit-referenceskema | Eagle-XML hentet og aflæst: BIAS/REFIN+/Rref og REFIN-/ISENSOR/Rref stemmer; FORCE2 vælges via SJ1, og RTDIN-/FORCE- samles via SJ4. RTDIN+/FORCE+ skal holdes adskilt ved at lade SJ3 være åben. U1's symbol-til-pad-kort stemmer med TQFN-tabellen. Et biblioteks-footprintnavn erstatter ikke kontrol mod producentens måltegning. |
| PlatformIO-build | **PASS**, `platformio.exe run`, miljø `esp32-s3-devkitc-1-16mb-psram`. Første forsøg krævede adgang til PlatformIOs låse-/cachefiler uden for projektet; genkørsel lykkedes. |
| Byggemiljø | Espressif32 7.0.1; Arduino-ESP32 2.0.17; Adafruit MAX31865 1.6.2; BusIO som installeret afhængighed. |
| Programstørrelse | RAM 53.408 / 327.680 byte (16,3 %). Flash 974.849 / 6.553.600 byte (14,9 %). Flashgrænsen er applikationspartitionen, ikke fysisk flashkapacitet. |
| Bygget version | `2.2.0-dev.1+g6cd08c7.dirty`, build 14. |
| Firmware.bin SHA-256 | `025f84e594464560b3d6e6243909457e118b9acf4edf96a994f717926e419faf` |
| Dokumentkontrol | GPIO-konstanter kontrolleret for dubletter, lokale dokumentlinks kontrolleret, testmodstande genberegnet og firmwarehash genberegnet. `git diff --check` bestod; Git varsler normal LF/CRLF-konvertering for den eksisterende temperaturfil. |
| Dynamiske sensor-/sikkerhedstests | Ikke udført. Ingen fysiske enheder eller simulator til rådighed. Buildsucces dokumenterer kompilering/linkning, ikke korrekt fejlreaktion. |

## Krav før prototype og frigivelse

1. Lås DevKit-/modulvariant, boardrevision, MAX31865-kapsling, sensorvariant, ønsket nøjagtighed og stikfamilier i BOM. Afklar 8 kontra 16 MB flash. Vælg carrier med DevKit eller selvstændigt ESP32-modul; sidstnævnte kræver et yderligere referencedesign for reset/boot, regulator, USB, programmering og antennefriholdelse.
2. Ret F1-F4 i firmware, og præciser buzzer-/LED-valg. Definér om sensorfejl skal stoppe gas straks, om fem sekunders tolerancetid accepteres, og om recovery kræver manuel kvittering. Kravene skal verificeres i alle proces- og manuelle tilstande.
3. Tegn skema ud fra netlisten, gennemfør ERC og uafhængig net-/footprintkontrol. Kontroller alle 20 chipben samt EP pr. kanal, stikpin 1 og fravær af forbindelse mellem SENS_A og SENS_B på PCB. Gennemfør DRC, forsynings-/varmebudget og konkret sikkerhedsreview af effektkredsløbet.
4. Idriftsæt med ufarlige dummybelastninger. Mål forsyning, CS og OFF-niveauer ved power-on, reset, brownout, programmering, watchdog og WiFi-genstart. Kontroller at gas ikke pulser under noget af disse forløb.
5. Brug en 3-ledertilsluttet præcisionsmodstand/simulator pr. kanal: 100,0000 ohm ved 0 °C; 125,1600 ohm ved 65 °C; 138,5055 ohm ved 100 °C. Sammenlign med forventet ADC-kode, temperatur og faktisk referenceværdi. Test begge kanaler samtidig og verificer deres navngivning.
6. Indsæt ens kendte kabelmodstande og derefter en ubalance, eksempelvis 1 ohm/1 ohm/1 ohm og en enkelt leder ændret. Dokumentér kompensation, selvopvarmning og det samlede fejlbudget over relevant temperatur og omgivelsestemperatur.
7. Bryd hver af de tre ledere enkeltvis, kortslut sensor, afbryd hvert MAX31865 og test SPI-/forsyningsfejl. Prøv også fejl, der kan efterlade plausibel måling. Registrér fault, `healthy`, `valid`, alder, fysisk gasudgang og recovery. Gentag med manuel gas i IDLE/PAUSED og i alle automatiske faser.
8. Test pumpe-/ventilskift, længste kabler, WiFi uden internet/NTP, OTA og sensorgendannelse. Mål den værste faktiske afbrydelsestid med logikanalysator/oscilloskop; sammenhold den med et fastlagt krav. Kontroller 50 Hz-konverteringens færdigmelding og timeout.
9. Frigiv først efter arkiveret skema, BOM, PCB-/ERC-/DRC-resultater, versionslåst firmware og udfyldt måle-/fejltestprotokol. Der er åbne punkter i alle fysiske valideringstrin i denne rapport.

Referenceværdierne følger Callendar-Van Dusen for en 100-ohm RTD med A = 3,9083 × 10^-3 og B = -5,775 × 10^-7 ved positive temperaturer, som anvendt i [Adafruits biblioteksimplementering](https://github.com/adafruit/Adafruit_MAX31865/blob/1.6.2/Adafruit_MAX31865.cpp). De er nominelle testmål; accepttolerancer skal udledes af den valgte sensors og elektroniks fejlbudget.
