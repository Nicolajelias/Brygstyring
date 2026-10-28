# PCB-forbindelser: to 3-leder PT100

> Review 28. oktober 2026: Se [den samlede designrapport](pt100-design-review.md)
> for valideringsstatus, fysiske DevKit-headerben og fund F1-F7. Sensorernes
> netforbindelser er kontrolleret mod databladet, og firmwarebuildet lykkes.
> Firmwarefund F1-F4 er nu rettet og regressionstestet med simulerede perifere
> enheder. N16R8 er bekraeftet af brugeren. Se den aktuelle firmwarebeskrivelse
> nedenfor og [kalibreringsvejledningen](pt100-calibration.md). Fysisk
> validering paa PCB og sensorer er fortsat ikke udfoert.

## Forudsaetninger

Firmware er aendret til to IEC 60751 PT100 (100 ohm ved 0 C, alpha 0.00385),
hver med sin MAX31865 og en 430 ohm referencemodstand. PT1000 og moduler med
4.3 kohm reference passer ikke til denne konfiguration.

Oversigten tager udgangspunkt i det bekraeftede ESP32-S3-DevKitC-1-N16R8 som
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
Officiel revision 1.1 bruger GPIO38 som RGB-data. Vaelg 48 eller 38 med
`-DBRYG_RGB_LED_GPIO=48` i platformio.ini. Revisionen skal stadig fastlaegges
ved kontrol af det fysiske board. Boardfilens betegnelse er rettet til N16R8
og svarer til 16 MB QIO flash og 8 MB OPI PSRAM.

## Valgt MAX31865-modul fra brugerens foto

Brugeren har valgt det lilla breakout-modul med teksten "MAX31865 2/3/4 Wire
Platinum RTD Converter", "Vin: 3-5VDC" og "Vlogic: 3-5VDC". Carrier-PCB'et
planlaegges med to af disse moduler, et pr. PT100. Brugeren har nu vist baade
under- og komponentsiden. Paa komponentsiden ses jumperne "2/3 Wire",
"2 Wire" og en trepad-vaelger maerket "24" og "3" over Rref.
Rref-maerkningen ser ud til at vaere `431`, dvs. 430 ohm efter den
almindelige trecifrede modstandskode. Det stemmer med PT100-firmwaren;
komponenttolerance og faktisk modstandsvaerdi er ikke maalt ud fra fotoet.
Producent og praecis modulrevision er fortsat ikke dokumenteret.

Benraekken er, laest fra VIN-enden mod DRDY-enden paa billedet:
`VIN - GND - 3V3 - CLK - SDO - SDI - CS - DRDY`.
Dette er en signalraekke, ikke en bekraeftet fysisk pin-1-nummerering.
Undersidefotoet maa ikke bruges direkte som et top-view-footprint: orientering
skal kontrolleres mod komponent-/monteringssiden, saa footprintet ikke spejles.

| Maerkning paa det viste modul | Gryde-modul | Ventil-modul |
| --- | --- | --- |
| VIN | ESP32 3V3 | ESP32 3V3 |
| GND | Logik-GND | Logik-GND |
| 3V3 | Lad uforbundet | Lad uforbundet |
| CLK | GPIO12 | GPIO12 |
| SDO | GPIO14 (MISO) | GPIO14 (MISO) |
| SDI | GPIO13 (MOSI) | GPIO13 (MOSI) |
| CS | GPIO4; 10 kohm pull-up til ESP32 3V3 | GPIO5; 10 kohm pull-up til ESP32 3V3 |
| DRDY | Separat testpunkt eller uforbundet | Separat testpunkt eller uforbundet |

VIN til 3.3 V ligger inden for det fotograferede moduls paatrykte
indgangsomraade. Brug 3.3 V logik, selv om modulet ogsaa er maerket 5 V.
Paa Adafruits referencedesign er den ekstra 3.3 V-pin en regulatorudgang;
det lilla moduls konkrete kredsloeb er endnu ikke dokumenteret. Derfor
forbindes dets 3V3-ben hverken til ESP32-forsyningen eller det andet moduls
3V3-ben. Foer brug kontrolleres faktisk forsyning ved MAX31865 (3.0-3.6 V)
og at SPI-signaler/pull-ups er kompatible med ESP32 ved VIN = 3.3 V.
Se [Adafruits pinbeskrivelse som reference, ikke som identifikation af klonen](https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/pinouts).

Sensorenden paa fotoet har fire forbindelser: `F+`, `RTD+`, `RTD-`, `F-`.
Til hver 3-leder PT100 anvendes:

| Modulben | Sensorforbindelse |
| --- | --- |
| F+ | Foerste leder fra den dobbelte ende af PT100-elementet |
| RTD+ | Anden leder fra samme ende; separat fra F+ paa PCB |
| RTD- | Enkelt leder fra modsatte ende |
| F- | Forbindes lokalt til RTD- via korrekt jumper eller kort bro |

### Jumpere paa det viste modul til 3-leder PT100

Udfoer dette paa begge moduler uden forsyning og med sensoren afbrudt.
Placeringerne nedenfor henviser til komponentsiden med sensorhullerne
oeverst og VIN/GND/SPI-headeren nederst:

| Synlig jumper | Placering | Faerdig indstilling til 3-leder |
| --- | --- | --- |
| `2/3 Wire` | Under sensorhullerne ved F-/RTD-, til venstre paa fotoet | Luk de to pads med en loddebro. Det forbinder F- og RTD-. |
| `2 Wire` | Under sensorhullerne ved F+/RTD+, til hoejre paa fotoet | Lad den vaere aaben. F+ og RTD+ skal have hver sin sensorleder. |
| `24` / `3` | Tre pads lige over Rref | Fjern en eventuel fabriksbro mellem midterpad og `24`-siden. Lod midterpad til `3`-siden. |

Ved trepad-vaelgeren maa midterpad ikke vaere forbundet til begge sider.
Hvis fabriksbroen er en kobberbane, skal den brydes; er det loddetin,
fjernes tinnet. Kontroller med multimeter, hvilken forbindelse der faktisk
er til stede, foer du aendrer noget. Fotoet alene beviser ikke, om en tynd
fabriksbro allerede er afbrudt.

Jumperplacering og maerkning svarer til den viste
[Adafruit-konfiguration for tre ledere](https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/pinouts).
Den tilsigtede elektriske forbindelse er FORCE2 til FORCE+, ikke til GND.
FORCE2 er ikke et separat ben i den eksterne header. Efter aendringen
kontrolleres kontinuitet paa det faktiske modul: F- til RTD- skal vaere
kortsluttet, F+ til RTD+ maa ikke vaere kortsluttet, og vaelgerens midterpad
skal kun vaere kortsluttet til `3`-siden. En loddebro mellem alle tre
vaelgerpads er forkert. Foretag kontrollen uden sensor, da lederparret
ellers kan ligne en utilsigtet bro mellem F+ og RTD+.

Kontroller disse punkter inden footprint og BOM laases:

- PT100-udgaven skal have Rref = 430 ohm til den aktuelle firmware.
  Den fotograferede maerkning laeses som `431` (43 x 10 = 430 ohm).
  Andre mulige maerkninger er `4300` = 430 ohm, mens `432` og `4301`
  betyder 4.3 kohm. Kontroller de faktisk leverede moduler; et produktfoto
  dokumenterer ikke komponenttolerance eller maalenoejagtighed.
- 3-lederkonfiguration kontrolleres paa hvert modul: FORCE2 til FORCE+,
  RTD- til F-, og ingen bro mellem F+ og RTD+. Kontroller uden tilsluttet
  sensor og uden spaending, da sensorens lederpar ellers kan ligne en bro.
- Headerpitch, sensorheaderpitch, raekkeafstand, boardkontur, hulplaceringer,
  stik-/terminalhoejde og frigang skal maales eller hentes fra en maalsat
  tegning for dette modul. Fotoet har ingen maalestok.

Carrier-PCB'et skal ikke have et ekstra saet MAX31865-chips og
referencemodstande i parallel med modulerne. Chipbenkortet senere i dette
dokument er reference for kontrol og et alternativ til modulmontage.
Firmwarepins og kalibreringsfunktion passer til modulplanen ovenfor;
den elektriske kompatibilitet er betinget af de naevnte komponentkontroller.

## Generel reference for MAX31865-moduler

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

Hver kanal har sin egen ikke-blokerende tilstandsmaskine med cirka et sekund
mellem cyklusstarter. Konfiguration og elektriske threshold-registre
tilbageaflaeses. Bias aktiveres med 12 ms settling, derefter koeres den
manuelle fault-detection-cyklus med 12 ms ventetid ved aaben FORCE-switch,
kontrol af afslutning og fault-status, og 12 ms settling efter lukning.
Dette er dimensioneret til det beskrevne 100 nF PT100-filter; vaesentligt
andre filtre/kabler kraever ny kontrol af RC-tiderne.

One-shot-kommandoens 1SHOT-bit skal kunne aflaeses sat umiddelbart efter
start. Resultatet accepteres tidligst efter 70 ms, med 1SHOT nulstillet og
de oevrige konfigurationsbits intakte. Et stadig sat 1SHOT-bit giver fejl
ved 110 ms; en haengende fault-cyklus giver fejl ved 20 ms. Fristerne
kontrolleres ved naeste service af loekken. Bias slukkes efter maaling/fejl.
Sensorvaerdien tidsstemples ved konverteringsstart, saa sen aflaesning ikke
goer gamle data friske. Adfaerden skal ogsaa verificeres paa de fysiske chips.

Baade ukorrigeret og kalibreret temperatur skal ligge i -20 til 125 C.
Elektriske thresholds svarer omtrent til 50-200 ohm og supplerer
softwaregraenserne. Foerste registrerede fejl goer straks maalevaerdien
ugyldig (NAN); den tidligere gyldige vaerdi bruges ikke som reserve til gas.
Data bliver ogsaa ugyldige, naar alderen er over fem sekunder. En central
interlock lukker gas ved naeste sikkerhedskontrol i ALLE proces- og manuelle
tilstande. Alle indkoblingsveje kontrollerer begge sensorers sundhed.
Geninitialisering sker ved hver tredje fejl, ogsaa efter at fejltaelleren
har naaet 255. Firmwaredefault for pumpe/gas/buzzer saettes LOW tidligt i boot;
eksterne pulldowns er fortsat noedvendige under reset.

Manuel gas forbliver slukket efter recovery, indtil den taendes igen.
En aktiv automatisk proces kan genoptage varmen, naar begge sensorer igen
er gyldige; fejl er ikke laast til operatoerkvittering. Pumpens eksisterende
procespolitik er bevaret. Loekkens netvaerkskald kan stadig forsinke fysisk
reaktion, saa den maksimale afbrydelsestid skal maales paa hardwaren.

Kalibrering af begge PT100 kan indtastes via System -> Temperaturkalibrering.
Se [kalibreringsvejledningen](pt100-calibration.md) for metode, graenser,
flashlagring og nulstilling. Kalibrering aendrer ingen fysiske forbindelser.

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
   ved foerste registrerede fejl samt ved udloeb af gyldighedstiden. Gentag
   i IDLE/PAUSED med manuel gas og i alle automatiske faser. Test recovery.
5. Test under pumpe-/ventilskift, WiFi og OTA samt ved realistiske kabellaengder.
   Kontroller temperaturafvigelse, stoeg og fejlreaktion med en referencemaaler.

Firmwarebuild er kontrolleret med PlatformIO. Fysisk maalenoejagtighed,
ledningsfejl, EMC og udgangenes sikkerhed er ikke verificeret paa hardware her.

## Referencer

- [MAX31865-datablad](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX31865.pdf)
- [Adafruit referenceskema og PCB](https://github.com/adafruit/Adafruit-MAX31865-PCB)
- [Adafruit 3-leder-tilslutning](https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/rtd-wiring-config)
- [ESP32-S3-DevKitC-1 hardwareguide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide.html)
