# PCB-forbindelser: to 3-leder PT100

> Review 28. oktober 2026: Se [den samlede designrapport](pt100-design-review.md)
> for valideringsstatus, fysiske DevKit-headerben og fund F1-F7. Sensorernes
> netforbindelser er kontrolleret mod databladet, og firmwarebuildet lykkes.
> Der er aabne firmwarefejl, herunder manglende vedvarende gasinterlock ved
> manuel gas i IDLE/PAUSED og for kort konverteringsventetid. Dette dokument
> er et skemagrundlag, ikke en produktions- eller driftstilladelse.

## Forudsaetninger

Firmware er aendret til to IEC 60751 PT100 (100 ohm ved 0 C, alpha 0.00385),
hver med sin MAX31865 og en 430 ohm referencemodstand. PT1000 og moduler med
4.3 kohm reference passer ikke til denne konfiguration.

Oversigten tager udgangspunkt i det eksisterende ESP32-S3-DevKitC-1 som
controller paa et carrier-PCB. Alle ESP32-tal er GPIO-numre, ikke fysiske
headerben eller ESP32-modulben. Kontroller footprint og headerplacering mod
netop dit DevKit. Et PCB med et loest ESP32-S3-WROOM-modul kraever desuden
modulets eget referencedesign for forsyning, EN/reset, boot, USB og antenne.

## Samlet GPIO-plan

| ESP32-S3 | Forbindelse | Retning set fra ESP32 | Bemaerkning |
| --- | --- | --- | --- |
| GPIO4 | MAX31865 gryde CS | Ud | Aktiv LOW; 10 kohm pull-up til 3.3 V |
| GPIO5 | MAX31865 ventil CS | Ud | Aktiv LOW; 10 kohm pull-up til 3.3 V |
| GPIO12 | Begge MAX31865 SCLK/SCK | Ud | Faelles SPI-clock |
| GPIO13 | Begge MAX31865 SDI/MOSI | Ud | ESP32 sender data |
| GPIO14 | Begge MAX31865 SDO/MISO | Ind | MAX31865 sender data; kun valgt chip driver |
| GPIO18 | Pumpe-driver IN | Ud | HIGH = ON, LOW = OFF |
| GPIO17 | Gasventil-driver IN | Ud | HIGH = ON, LOW = OFF |
| GPIO21 | Buzzer-driver IN | Ud | Firmware bruger 750 Hz PWM |
| GPIO15 | Betjeningsknap | Ind | Knap til GND; aktiv LOW |
| GPIO8 | OLED SDA | Tovejs | I2C pull-up til 3.3 V |
| GPIO9 | OLED SCL | Ud | I2C pull-up til 3.3 V |
| GPIO48 | Adresserbar RGB-LED DIN | Ud | Ikke en almindelig RGB-LED med tre farveben |
| 3V3 | MAX31865-forsyning og 3.3 V OLED | Forsyning | Kun lavspaendingsdelen |
| GND | Faelles logik-/sensorinterface-GND | Stel | Ikke PT100-sensorledningerne |

MAX31865 DRDY/RDY bruges ikke; lad udgangen vaere uforbundet eller foer den
til et separat testpunkt pr. kanal. DRDY er push-pull; udgangene maa ikke
kortsluttes mellem kanalerne. GPIO38 bruges ikke som LED-stroemstyring i denne firmware
(`PIN_RGB_LED_PWR = -1`). Behold GPIO19/20 til native USB og GPIO43/44 til
UART, hvis disse interfaces skal bruges. Undgaa boot-strapping GPIO0/3/45/46
til ekstra belastninger. Flash-/PSRAM-pins afhaenger af modulvarianten;
GPIO35/36/37 skal bl.a. reserveres ved den aktuelle OPI PSRAM-konfiguration.

GPIO48 til onboard-RGB passer til det oprindelige officielle DevKitC-1.
Officiel revision 1.1 bruger GPIO38 som RGB-data og kraever firmwaretilpasning.
Laas boardrevision og hukommelsesvariant i BOM; boardfilens N8R8-navn er
ikke konsistent med dens 16 MB flash-konfiguration. Se designrapporten.

## MAX31865-moduler

| Modulsignal | Gryde-modul | Ventil-modul |
| --- | --- | --- |
| VIN/VCC | 3.3 V, hvis modulet understotter det | 3.3 V, hvis modulet understotter det |
| GND | GND | GND |
| SCK/CLK | GPIO12 | GPIO12 |
| SDI/MOSI | GPIO13 | GPIO13 |
| SDO/MISO | GPIO14 | GPIO14 |
| CS | GPIO4 | GPIO5 |
| DRDY/RDY | Ikke tilsluttet | Ikke tilsluttet |

Paa Adafruit-modulet forbindes 3.3 V til VIN, ikke til 3Vo-udgangen.
Klonmoduler kan have andre regulatorer og pinnavne; kontroller deres skema.
Brug 3.3 V logik paa alle SPI-signaler. En loes MAX31865 maa ikke forsynes
med 5 V.

Paa hvert Adafruit-modul: luk jumperen "2/3 Wire", afbryd standardbroen paa
valgjumperen over Rref og luk dens "3"-position. Andre moduler skal give
samme elektriske forbindelser som beskrevet nedenfor, ikke blot samme tekst
paa silketrykket. Softwareindstillingen alene er ikke nok.

## PT100-stik

Brug et separat 3-polet stik til hver sensor, f.eks. J1 GRYDE og J2 VENTIL.
De to ledninger til samme ende af elementet er ofte begge roede; den tredje
er ofte hvid. Farver er ikke en garanti: maal foerst med et multimeter.
Mellem de to samme-ende-ledninger ses kun ledningsmodstanden; til den tredje
ses cirka 100-110 ohm ved 0-25 C plus ledningsmodstand.

| Stikben paa J1/J2 | Sensorledning | Til den tilhoerende MAX31865 |
| --- | --- | --- |
| 1 | Samme ende, ledning A (ofte roed) | FORCE+ / F+ |
| 2 | Samme ende, ledning B (ofte roed) | RTDIN+ / RTD+ |
| 3 | Modsat ende, ledning C (ofte hvid) | RTDIN- og FORCE- kortsluttet lokalt paa PCB |

FORCE2 forbindes til FORCE+ lokalt ved chippen for 3-lederdrift.
FORCE+ og RTDIN+ maa IKKE kortsluttes paa PCB: de skal have hver sin
sensorledning. PT100-ledningerne forbindes hverken til 3.3 V eller GND.
Ingen af sensorstikkets tre ben er et digitalt datasignal.
En eventuel kabelskaerm er en separat chassisforbindelse, ikke en af de tre
maaleledninger. Ledningerne skal have ens laengde og tvaersnit for god
3-lederkompensation.

## Loese MAX31865-chips paa PCB

Foelgende benkort er for 20-ben TQFN-varianten MAX31865ATP+ som i Adafruits
referenceskema. Kontroller altid bestillingsnummer, datablad, pin-1-markering
og CAD-footprint inden produktion; overfoer ikke benkortet ukritisk til andre
kapslinger. Gentag hele kredsloebet en gang pr. sensor.

| Chipben | Signal | Forbindelse |
| --- | --- | --- |
| 1 | BIAS | Til ben 2 og oeversiden af egen Rref |
| 2 | REFIN+ | Til ben 1 og oeversiden af egen Rref |
| 3 | REFIN- | Til ben 4 og undersiden af egen Rref |
| 4 | ISENSOR | Til ben 3 og undersiden af egen Rref |
| 5 | FORCE+ | Sensorstik ben 1 samt chipben 6 |
| 6 | FORCE2 | Til chipben 5, ikke til GND |
| 7 | RTDIN+ | Sensorstik ben 2 |
| 8 | RTDIN- | Sensorstik ben 3 samt chipben 9 |
| 9 | FORCE- | Til chipben 8/sensorstik ben 3, ikke til GND |
| 10 | GND2 | GND |
| 11 | SDI | GPIO13 |
| 12 | SCLK | GPIO12 |
| 13 | CS | GPIO4 for gryde, GPIO5 for ventil; egen 10 kohm pull-up |
| 14 | SDO | GPIO14, faelles med den anden chip |
| 15 | DGND | GND |
| 16 | GND1 | GND |
| 17 | N.C. | Ikke tilsluttet |
| 18 | DRDY | Ikke tilsluttet/testpunkt |
| 19 | DVDD | 3.3 V; 100 nF til GND taet ved benet |
| 20 | VDD | Ren 3.3 V; 100 nF til GND taet ved benet |
| Exposed pad | EP | GND, med korrekt termisk land pattern |

Komponenter pr. kanal:

- Rref: 430 ohm, hoejst 0.1 %, gerne 0.01 %, lav temperaturkoefficient
  (f.eks. hoejst 15 ppm/C). Forbind mellem BIAS/REFIN+ og REFIN-/ISENSOR.
  Brug Kelvin-routing fra referenceindgangene til modstandens pads.
- 100 nF mellem RTDIN+ og RTDIN-, taet ved chippen, som i PT100-referencen.
- 100 nF ved hver af VDD og DVDD samt lokal bulk-afkobling paa 1-10 uF.
- 10 kohm mellem CS og 3.3 V, saa begge chips er fravalgt under reset.
- Eventuelt ferritfilter i den analoge VDD-forsyning efter referencedesignet.

Hold SPI kort og paa PCB. Placer MAX31865 og Rref vaek fra regulatorvarme,
WiFi-antennen, relaespoler og effektbaner. Hold et sammenhaengende stelplan
under maaledelen og foer ikke pumpe-/relaestroemme gennem dens returveje.
ESD/transientbeskyttelse ved eksterne sensorstik skal vaelges med lav laekstroem
og verificeres for maaleafvigelse. De gamle 4.7 kohm OneWire-pull-ups fjernes.

## Oevrige forbindelser og forsyning

- OLED: VCC til 3.3 V, GND til GND, SDA til GPIO8, SCL til GPIO9.
  Firmwaren bruger SSD1306 128x64 med I2C-adresse 0x3C.
  Brug normalt 4.7 kohm pull-ups fra SDA/SCL til 3.3 V, medmindre modulet
  allerede har passende pull-ups. Ingen I2C pull-ups til 5 V.
- Knap: normalt aaben kontakt mellem GPIO15 og GND. Firmware aktiverer intern
  pull-up; ekstern 10 kohm til 3.3 V anbefales paa et PCB med ekstern knap.
- Pumpe: GPIO18 til et 3.3 V kompatibelt, aktiv-HIGH drivertrin/relaeinterface.
- Gasventil: GPIO17 til et 3.3 V kompatibelt, aktiv-HIGH drivertrin/relaeinterface.
- Buzzer: GPIO21 til passende drivertrin; firmwaren sender 750 Hz PWM.
  En passiv buzzer/transducer passer til tonestyringen; en aktiv buzzers lyd
  med denne PWM afhaenger af typen. Belast ikke GPIO direkte med en effektbuzzer.
- RGB: GPIO48 til DIN paa en adresserbar LED. Brug passende forsyning,
  lokal afkobling og eventuelt seriemodstand; ved 5 V LED vaelges en
  levelshifter med garanteret 3.3 V inputkompatibilitet, f.eks. 74AHCT125.
  DevKit har allerede en onboard-LED; undgaa utilsigtet parallel tilslutning.
- DevKit forsynes via den dokumenterede 5 V-indgang eller USB. Undgaa
  tilbagefoedning mellem ekstern 5 V og USB. Dimensioner forsyningen efter
  controller, WiFi-spidser, display og LED; relaespoler forsynes separat efter
  deres maerkedata. ESP32 GPIO er ikke 5 V-tolerante.

Pumpe- og gasudgange skal have ekstern hardware-default OFF, f.eks. 10 kohm
pulldown paa et aktiv-HIGH driverinput. Brug passende transistor/MOSFET eller
isoleret interface, og friloebs-/transientbeskyttelse til DC-spoler efter
aktuatorens krav til afbrydelsestid. Ingen spoler eller ventiler direkte paa GPIO.
Ved galvanisk isolation maa lastsidens stel ikke kortsluttes til logikstel.

230 V og gas kraever et selvstaendigt, fagligt kontrolleret sikkerhedsdesign:
korrekt isolation, krybeafstande, sikringer, jord og kapsling. Brug en
normalt lukket gasventil samt uafhaengig flammevagt, overtemperatursikring
og sikkerhedsafbrydelse. Denne firmware er ikke en certificeret braenderstyring;
softwarebaseret temperaturkontrol kan ikke erstatte disse funktioner.
Driverkomponenter og effektstik kan ikke dimensioneres endeligt uden
spaending, stroem, spoletype og model paa pumpe, ventiler og relaeinterface.

## Firmware og idriftsaettelse

Maalingerne sker cirka en gang pr. sekund med 50 Hz-filter. Bias aktiveres,
der ventes mindst 10 ms via tilstandsmaskinen, begge one-shot-konverteringer
startes, og resultaterne laeses efter mindst 65 ms uden blokerende ventekald.
Dette er den aktuelle implementering: 65 ms er for kort i forhold til
databladets maksimum paa 66 ms ved 50 Hz. Faerdigmelding/timeout og en
konservativ ventetid skal indfoeres, jf. designrapportens F2-F3.
Bias slukkes efter aflaesning for at begraense selvopvarmning.
Godkendt procesinterval er fortsat -20 til 125 C, selv om PT100 kan have et
stoerre fysisk maaleomraade. Hardwarefejlflag og ugyldige temperaturer afvises.
Seneste gyldige maaling bevares, indtil dens alder er over fem sekunder;
derefter returneres NAN. I automatiske varmefaser medfoerer dette gas-OFF,
naar proceskontrollen koerer. Manuel gas i IDLE/PAUSED har derimod ingen
vedvarende sensorinterlock og kan forblive ON. Denne fejl skal rettes.
En enkelt fejlaflaesning betyder heller ikke oejeblikkelig gasafbrydelse.
Geninitialisering sker ved hver tredje fejl. Koden aflaeser kun eksisterende
fault-status og starter ikke den saerlige fault-detection-cyklus; alle
ledningsfejl er derfor ikke dokumenteret daekket. Se rapportens F1-F4.

Foer PCB frigives eller gas tilsluttes, testes begge kanaler med ufarlige
dummybelastninger paa udgangene:

1. Kontroller 3.3 V, stel, CS-pull-ups og hardware-default OFF under reset,
   boot og stroemudfald. Flash foerst PT100-firmwaren efter hardwareombygningen.
2. Brug praecisionsmodstande/sensorsimulator: 100 ohm skal give cirka 0 C,
   og 138.5055 ohm cirka 100 C. Tilslut som 3-leder, og verificer begge kanaler.
3. Kontroller at gryde og ventil ikke er byttet, og at 85 C accepteres som
   normal maaling; den tidligere digitale sensors opstartsvaerdi er ikke relevant.
4. Afbryd hver af de tre sensorledninger enkeltvis, kortslut sensoren, og
   afbryd et MAX31865-modul. Verificer fejlstatus, maalingens alder og gas-OFF
   efter udloeb af gyldighedstiden, og test efterfoelgende recovery.
5. Test under pumpe-/ventilskift, WiFi og OTA samt ved realistiske kabellaengder.
   Kontroller temperaturafvigelse, stoeg og fejlreaktion med en referencemaaler.

Firmwarebuild er kontrolleret med PlatformIO. Fysisk maalenoejagtighed,
ledningsfejl, EMC og udgangenes sikkerhed er ikke verificeret paa hardware her.

## Referencer

- [MAX31865-datablad](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX31865.pdf)
- [Adafruit referenceskema og PCB](https://github.com/adafruit/Adafruit-MAX31865-PCB)
- [Adafruit 3-leder-tilslutning](https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/rtd-wiring-config)
- [ESP32-S3-DevKitC-1 hardwareguide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide.html)
