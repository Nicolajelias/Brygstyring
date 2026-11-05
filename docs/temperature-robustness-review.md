# Temperaturmåling under drift: robusthedsgennemgang

Dato: 5. november 2026. Grundlag: den aktuelle arbejdsmappe inklusive de lokale
Wi-Fi-, NTP- og MAX31865-rettelser, PlatformIO espressif32 7.0.1, Arduino-ESP32
2.0.17 og de faktisk installerede biblioteker. Begge fysiske sensorer er nu
tilsluttet og giver ifølge brugeren gyldige målinger; enheden er på normalt Wi-Fi.

## Implementeret 5. november 2026

Den historiske analyse nedenfor beskriver koden **før** denne rettelse.
Aktuel drift og undtagelsen for manuel gas er nu:

| Område | Implementeret adfærd |
| --- | --- |
| Måling og proces | Selvstændig FreeRTOS-opgave på core 1, prioritet 3, planlagt hver 5 ms. Begge MAX31865, automatisk gasinterlock, proces, knap og buzzer serviceres her. |
| Web og delt tilstand | Arduino-opgaven håndterer HTTP. Korte rekursive låse beskytter proces- og sensoradgang; låsen frigives før socket-I/O. Status samles sammenhængende under låsen. |
| Wi-Fi | Forbindelse uden venteløkke. Efter 15 sekunders mislykket forsøg åbnes BrygAP; genforsøg efter 30 sekunder. Forbindelsestab udløser ikke systemreset. AP forbliver åbent efter genforbindelse, hvis fallback tidligere er startet. |
| Internet og tid | SNTP kører i baggrunden. Proceskode læser kun lokal tid. Ingen gyldig tid vises som `--:--:--`; procesvarigheder, målealder og deadlines bruger fortsat `millis()`. Fast UTC+1-visning er bevaret. |
| Manuel gas/pumpe | I Klar/IDLE og PAUSE kan begge udgange tændes/slukkes uden temperaturdata. Manuelt tændt gas bliver ved med at være tændt ved sensorfejl eller gamle målinger. UI viser `MANUEL`. |
| Automatisk gas | Alle automatiske gas-ON-veje kræver begge gyldige sensorer. Første registrerede fejl eller alder over 5 s lukker automatisk gas. Overgang/genoptagelse til automatisk drift ophæver manuel gas. Automatisk recovery kan åbne igen; fejlen er ikke kvitteringslåst. |
| Vedligeholdelse/flash | Indstillinger, opskrifter, kalibrering og OTA kræver IDLE med gas og pumpe OFF. En særskilt vedligeholdelsesmarkering spærrer samtidig start fra knap/web; måleservice fortsætter. Flash/cache kan stadig kortvarigt standse begge kerner. |
| Lagring | Procesændringer opdaterer kun RAM; baggrundskoden kan skrive en versioneret post i NVS `brew-process/state-v1`, når udgange er OFF. Ingen proceskode skriver længere EEPROM offset 100. Ugyldige gamle IP/netmaskefelter nulstilles til DHCP uden at slette gyldige credentials. |
| Opstart | Udgange sættes OFF tidligt; kontrolopgaven starter før netværk/display. Genstart starter altid i IDLE/OFF; gemt proces genoptages ikke automatisk. |
| Display og log | OLED probes på 0x3C, manglende display springes over. I2C-timeout er 5 ms pr. transaktion; displayfejl deaktiverer display til næste opstart. Logkø med 16 poster; fyldt kø taber logtekst frem for at vente. |
| Overvågning | Kontrolopgaven tilmeldes task-watchdog med 5 s timeout/panic. `/status` viser kontroltæller, maksimal pause/arbejdstid, heartbeat, watchdog-status og resetårsag samt accepterede samples, samlet fejltal og seneste fejl pr. kanal. |

**Manuel betjening er bevidst undtaget fra temperaturinterlock efter brugerens
instruktion.** Aktiv automatisk proces skal stadig pauses/stoppes før manuel
betjening. En langsom HTTP-klient kan forsinke andre webkommandoer; dette giver
ikke længere samme ventetid i måle-/procesopgaven. Koden er ikke en dokumentation
af hård realtid eller fysisk maksimal ventilafbrydelsestid.

DRDY-forbindelserne er nu tilføjet af brugeren: gryde GPIO6 og ventil GPIO7.
Firmware læser/kasserer det gamle RTD-register og kræver DRDY HIGH før start,
LOW ved ny færdigmelding og HIGH efter RTD-aflæsning. Mindst 70 ms ventetid,
110 ms ready-timeout og alle eksisterende plausibilitets-/faultkontroller bevares.
R7/F3 er dermed håndteret for en lydløst ignoreret start med gammel plausibel
værdi og fastlåst ready-signal. Det er ikke en garanti mod vilkårlige kombinerede
chip-, SPI- og signalfejl. DRDY er push-pull; den interne GPIO-pull-up giver
defineret HIGH ved kabelbrud. Ingen ekstra ekstern pull-up er nødvendig.
Grundlag: [MAX31865 Rev 3, benbeskrivelse, tDRDYH og figur 9](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX31865.pdf).

### Regressionstest

- `python scripts/test_pt100.py`: 11 testgrupper består. Den faktiske kontroltask
  køres 60 simulerede sekunder uden web-/netværksarbejde og med manglende gryde;
  ventilens samples forbliver friske. Automatiske fejlstop, manuelle undtagelser,
  kalibrering, lagring, vedligeholdelseseksklusion og `millis()`-wrap testes. DRDY-testen dækker
  ignoreret start, fast HIGH/LOW, udebleven ready-clear, recovery og konstant temperatur.
- `python scripts/test_network.py`: migration efter korruption ved offset 100,
  bevarede credentials, AP-fallback, gentagne forbindelsesfejl og recovery består.
- Hostperiferi, RTOS og netværk er simuleret. Fysiske netværksudfald, blokeret
  DNS/NTP og faktisk ventil-OFF-latens kræver fortsat særskilte anlægsprøver.

### Build og fysisk kontrol i denne ændring

PlatformIO-build og alle 14 hosttestgrupper bestod; de tre indlejrede
JavaScript-programmer består syntakskontrol. `git diff --check` er ren.
Brugeren har afklaret, at strømafbrydelsen/USB-fraværet under forrige kontrol
skyldtes lodning af de nye DRDY-forbindelser. Efter genforbindelse kunne pc'en
igen hente status via 192.168.50.133; begge sensorer var gyldige, fejltaellere
var nul og kontrolopgavens største interval var 5 ms. De tidligere netværks-
timeouts er derfor ikke i sig selv evidens for en firmwarefejl.

Endelig DRDY-firmware er bygget og uploadet til COM10 med verificeret flashhash.
Version `2.2.0-dev.6+g9cdb23b.dirty`, build 19, bygget UTC 2026-11-05 20:55:41.
SHA-256: `9a7209e1e9e1b21b8844825a6bc8c8adb60758ce912c1a73c078b04bded640f5`.
RAM 53.448 byte, program 979.817 byte. Gemte indstillinger er bevaret.

Fysisk test: `python scripts/check_runtime.py http://192.168.50.133 --seconds 10`
**bestod**. En langsom POST-body holdt webserverens parsing optaget i 10 sekunder.
I hele observationsvinduet inklusive afslutning blev der godkendt 14 nye samples
pr. kanal. Begge målte til sidst 24,21 °C; målealderen var 375 ms. Kontrolopgavens
største målte interval var 5 ms og største arbejdstid 1 ms; watchdog var aktiv.
Ingen nye sensorfejl eller reset blev registreret under testen, og gas/pumpe
forblev OFF. Manuelle ON/OFF-forløb ved fejl er testet med simulerede udgange.

Ved denne opstart registreredes én kortvarig `fault cycle failed or timed out`
pr. kanal. Begge gendannede automatisk gyldige målinger; de samlede fejltællere
forblev på 1 gennem efterfølgende statuskontrol og belastningstest. Årsagen til
den første fault er ikke fastslået. Der blev ikke registreret DRDY-fejl.

Resultatet verificerer DRDY-forløbet på begge monterede moduler og isolation
fra den afprøvede HTTP-blokering. Det er en kort funktionstest, ikke en garanteret
worst-case-grænse. Router-/DNS-/NTP-fejl og fysisk ventil-OFF-latens mangler fortsat
særskilte anlægsprøver; ingen netværksindstillinger blev ændret for testen.

## Historisk konklusion før rettelsen


Kravet om målinger uafhængigt af Wi-Fi, internet og brugerflade er **ikke opfyldt**.
MAX31865-driveren bruger en tilstandsmaskine, men den kaldes fra samme Arduino-løkke
som synkrone netværks-, display- og flashoperationer. Når løkken venter, står både
måleforløbet og kontrollen af gasudgangen stille. De tidligere rettelser har fjernet
gentagne NTP-ventetider i AP-tilstand og en fejlagtig kontrol af startbitten; de har
ikke fjernet denne fælles afhængighed.

Denne gennemgang ændrer ikke firmware og uploader eller genstarter ikke boardet.
Tider nedenfor er kodebaserede ventetider eller udtrykkeligt angivne simuleringer,
ikke målte worst-case-tider på anlægget.

## Prioriterede fund

### R1 — Kritisk: sikkerhed og målinger deler en blokérbar løkke

[main.cpp](../src/main.cpp:65) kalder temperaturservice og sikkerhed før webserveren
og igen bagefter. Proces, display og netværk udføres stadig sekventielt i samme task.
[ProcessHandler::enforceTemperatureSafety](../src/ProcessHandler.cpp:715) er en
almindelig funktion, ikke en uafhængig overvågning.

Fem sekunders maksimal målealder er derfor **ikke fem sekunders maksimal tid til
gas-OFF**. En GPIO, der allerede er HIGH, forbliver HIGH under et blokerende kald,
indtil kode faktisk udfører afbrydelsen. Det gælder også ved manuel gas i IDLE.

Bekræftet i en hosttest med den faktiske proces- og temperaturkode: først gyldige
målinger og gas ON; derefter 6000 ms simuleret løkkestop. Temperaturens gyldighed
var false, men gasflag og GPIO var fortsat ON. Først det næste kald til
`enforceTemperatureSafety()` slukkede udgangen. Testen simulerer blokeringen;
den er ikke en fysisk prøve med gas eller en fremprovokeret netværksfejl.

**Retning:** giv temperaturservice, gyldighed og gasinterlock en selvstændig,
periodisk kontroltask uden netværk, display, logudskrift eller flashskrivning.
En separat sensortask alene er utilstrækkelig, hvis gasinterlock stadig kan blokere.

### R2 — Høj: Wi-Fi-tab genstarter hele controlleren

[WiFiHandler.cpp](../src/WiFiHandler.cpp:163) kontrollerer forbindelsen hvert
60000 ms og kalder `ESP.restart()` ved manglende STA-forbindelse. Et udfald, som
rammer en kontrol, afbryder dermed måling og proces. Perioden er 60 sekunder,
selv om kommentaren siger 10 sekunder.

Ved genstart venter [WiFiHandler::begin](../src/WiFiHandler.cpp:87) op til 60 sekunder
på routeren. Derefter kommer mindst 14,3 sekunders eksplicitte skærmventetider
(23 × 100 ms, 2 sekunder og 2 × 5 sekunder), plus initialisering og I/O.
Temperaturdriveren initialiseres først til sidst i `setup()`. Det kan derfor give
over 74 sekunder uden nye målinger ved utilgængelig router, og længere ved andre
blokeringer. Det er ikke en fast øvre grænse.

**Retning:** fjern automatisk systemgenstart ved netværksfejl. Brug en Wi-Fi-
tilstandsmaskine med tidsstyrede genforsøg og eventuel AP-fallback. Måling og
kontrol skal starte før netværksforbindelse og opstartsanimation. Netværkstab må
ikke nulstille brygprocessens monotone timer eller målehistorik.

### R3 — Høj: NTP/DNS blokerer stadig, når Wi-Fi har forbindelse

[updateClock](../src/ProcessHandler.cpp:17) undlader NTP uden STA-forbindelse og
begrænser forsøg til ét pr. minut. Men `WL_CONNECTED` siger ikke, at internet,
DNS eller NTP-server er tilgængelige. `clock.update()` kaldes stadig direkte fra
procesløkken.

Den installerede `NTPClient.cpp:84` sender en forespørgsel og venter cirka
1020 ms ved manglende svar. Før det kan `WiFiGeneric.cpp:1566` vente op til
15000 ms på DNS-svar; der er desuden en særskilt ventemulighed på op til 16000 ms
på DNS-idle. Ventetiderne er betingede, ikke en garanteret pause ved hvert kald.
Allerede et DNS-vent på 15 sekunder overstiger målealderens fem sekunder betydeligt.
Ét forsøg pr. minut begrænser hyppigheden, ikke varigheden af blokeringen.

**Retning:** brug baggrundssynkronisering via SNTP eller en særskilt netværkstask.
Kontrolløkken læser kun et tids-snapshot. Den skal aldrig vente på DNS eller tid.
Bevar `millis()`/monoton tid til målealder, procesvarigheder og timeouts; markér
kalendertid som usynkroniseret, når ingen gyldig synkronisering findes.
Genoptagelse af gemt processtatus kræver også eksplicit gyldig kalendertid;
`restoreProcessState()` bruger i dag epoch uden at kontrollere synkronisering.

### R4 — Høj: langsomme HTTP-klienter kan holde løkken optaget

[WebServerHandler::handleClient](../src/WebServerHandler.cpp:434) bruger den
synkrone Arduino-WebServer. Den installerede `Parsing.cpp:44` venter op til
5000 ms på POST-data pr. venteforløb og starter ventetælleren igen, når mere data
ankommer. Headerlæsning bruger også synkrone streamkald. `WiFiClient.cpp:389`
har socket-skrivning med op til ti select-forsøg á ét sekund uden fremdrift;
delvis fremdrift nulstiller forsøgsbudgettet. Der er ikke en samlet kort
tidsgrænse for hele HTTP-anmodningen i applikationen.

Ikke al intern webserverventen blokerer: dens tilstand for en klient uden data
returnerer til løkken. Problemet er især parsing, body/modtagelse og afsendelse
inde i samme kald. Et `yield()` afvikler ikke automatisk den aktuelle Arduino-
løkkes temperaturfunktioner.

**Retning:** flyt webbet væk fra kontroltasken, brug afgrænsede kommandoer og
måle-snapshots, og begræns requeststørrelse og samlet behandlingstid. Webhandlers
må ikke ændre proces- eller sensorobjekter samtidigt med kontroltasken uden
beskedkø eller kort, veldefineret synkronisering.

### R5 — Høj: processtatus overskriver en del af netværkskonfigurationen

[Config](../include/EEPROMHandler.h:6) fylder 140 byte på den aktuelle 32-bit
platform og lagres fra offset 0. Feltet `sn` ligger ved offset 96–111.
[ProcessHandler.cpp](../src/ProcessHandler.cpp:9) bruger offset 100 til processtatus.
`saveProcessState()` og `resetProcessState()` skriver dermed ind over netmasken.
At gemme hele Config kan omvendt overskrive processtatus.

Layoutet er bekræftet ved kompilering med projektets 32-bit hosttoolchain:
`sizeof(Config)=140`, `offsetof(Config,sn)=96`. Dette er en konkret lagerfejl,
ikke blot en mulig flashforsinkelse. Den ændrer ikke nødvendigvis en aktiv
Wi-Fi-forbindelse med det samme, men kan ødelægge netværksopsætningen efter
genstart og dermed udløse flere forbindelsesproblemer.

**Retning:** adskil processtatus og konfiguration med forskellige NVS-nøgler eller
verificerede, ikke-overlappende områder. Tilføj versionering, validering og en
test af, at start/stop/pause/gem ikke ændrer netværksfelterne. Eksisterende data
skal håndteres ved migration; de må ikke antages at være intakte.

### R6 — Middel/høj: display, flash og lokale ventetider

- [DisplayHandler.cpp](../src/DisplayHandler.cpp:53) sender hele OLED-bufferen
  synkront hvert 500 ms, også når OLED ikke er monteret. Resultatet af
  `display.begin()` bruges ikke som guard. Wire har 50 ms timeout pr. transaktion,
  og en skærmoverførsel består af flere transaktioner. Et fraværende display kan
  give hurtig NACK; en låst I2C-bus kan give længere, gentagne ventetider.
- [ProcessHandler.cpp](../src/ProcessHandler.cpp:267) bruger `delay(200)` til buzzer
  og flere `delay(50)` til knapper. De er kortere end fem sekunder enkeltvis, men
  forsinker måletrinene og kan akkumulere med anden I/O.
- `EEPROM.commit()`, Preferences og SPIFFS udføres synkront. Processtatus gemmes
  under tilstandsskift; `/saveSettings` har ingen IDLE-guard. Opskrifter er
  begrænset til IDLE, men IDLE tillader manuel gas, så IDLE alene garanterer ikke
  slukkede udgange. Kalibrering kræver IDLE og gas OFF. OTA kræver IDLE og både
  pumpe/gas OFF; denne beskyttelse er allerede til stede.
- Temperaturfejl bruger `Serial.printf()` direkte i sensorservice. UART-buffer
  og SPI-driverlåse giver yderligere potentielle ventepunkter. Der er ikke målt
  nogen hård øvre grænse for disse kald.

**Retning:** displaytask med tilstedeværelseskontrol, begrænset I2C-timeout og
pause/genforsøg efter fejl; tidsstyret buzzer/debounce; logkø; flashoperationer
kun i eksplicit sikre perioder. En ekstra task/CPU løser ikke automatisk flash-
pauser: flash/cache-operationer kan påvirke begge kerner og skal indgå i design
og måling af maksimale forsinkelser.

### R7 — Friskhed er ikke det samme som registerplausibilitet

[TemperatureHandler.cpp](../src/TemperatureHandler.cpp:133) kontrollerer
konfiguration, tærskler, RTD-fejl og temperaturinterval og venter mindst 70 ms.
Kanalerne har separat fejlstatus, og sampletid sættes ved konverteringsstart.
Det er gode egenskaber, som skal bevares.

DRDY bruges imidlertid ikke. Et nulstillet 1SHOT-bit beviser ikke alene en ny
konvertering. En lydløst ignoreret start og en plausibel gammel registerværdi
kan derfor ikke sikkert skelnes fra en ny måling. Dette er det allerede
dokumenterede resterende F3-problem; det løses ikke af netværksændringer.

**Retning:** hvis der kræves særskilt bekræftelse af nye data, tilslut hvert moduls
DRDY til en separat ledig GPIO og valider hele ready/konverteringsforløbet, ikke
blot et statisk LOW-niveau. Bevar deadlines og fejlreaktion ved udebleven ready.
En konstant temperatur er i sig selv ikke tegn på et fastfrosset register.

## Foreslået struktur og godkendelseskriterier

1. En kontroltask ejer SPI/sensorer, målevalidering, proces og aktuatorudgange.
   Kør den periodisk, eksempelvis hvert 5 ms, med eksplicit prioritet og målte
   køretider. 5 ms er et designmål, ikke en allerede verificeret garanti.
2. Netværk, SNTP, web, display og lagring må ikke ligge på kontroltaskens sti.
   Kommunikation sker via korte kommandoer og sammenhængende snapshots med værdi,
   timestamp, sekvensnummer og fejlstatus. Undgå låse, som holdes under I/O.
3. Start måling og OFF-sikring straks. Netværk må forbinde/genforbinde i baggrunden.
   Planlagt OTA/genstart er en særskilt vedligeholdelsestilstand med udgange OFF.
4. Mål største interval mellem kontrolkørsler, største samplealder, gyldige samples
   pr. kanal, fejlårsager, resetårsag og faktisk gas-OFF-latens. Watchdog skal
   overvåge den relevante task, men erstatter ikke en dokumenteret OFF-mekanisme.
5. Fasthold cirka ét sample pr. sekund og den eksisterende femsekunders afvisning
   af gamle samples. Som acceptmål bør netværksfejl alene ikke give sensorfejl
   eller samplealder over cirka 1,5 sekund. Aftal og mål maksimal OFF-latens, f.eks.
   højst 100 ms fra en fejl/udløbet gyldighed registreres, også under webbelastning.
   Sensorens detektionstid og eventuel fysisk ventilforsinkelse kommer særskilt.

## Validering udført og manglende driftstest

Udført uden at ændre eller afbryde det tilsluttede anlæg:

- Statisk gennemgang af projektkode og faktisk installerede bibliotekskilder.
- Alle ni eksisterende host-testgrupper bestod.
- Ekstra hosttest af 6000 ms løkkestop og forsinket gas-OFF bekræftede R1.
- Kompileret størrelses-/offsetkontrol bekræftede R5.

Hosttests simulerer periferi og beviser ikke Wi-Fi-driverens eller flashens
faktiske tidsgrænser. Før robusthedskravet kan godkendes, bør den ændrede firmware
prøves med begge sensorer under: router slukket i længere tid, gentagne Wi-Fi-
udfald, Wi-Fi med blokeret DNS, fungerende DNS uden NTP-svar, langsomme/afbrudte
HTTP-anmodninger, samtidig webtrafik, OLED fraværende og I2C-fejl, lovlige
flashoperationer samt fejl på hver enkelt sensor. Genetabler forbindelserne og
kontrollér recovery uden systemgenstart eller tab af procesvarighed.

Der er ikke udført disse forstyrrelsestest på den kørende enhed i denne gennemgang.
Der er ikke implementeret eller uploadet nye produktionsrettelser som del af analysen.

## Kilder

Projektets kildehenvisninger ovenfor og disse lokale bibliotekskilder:

- `framework-arduinoespressif32/libraries/WiFi/src/WiFiGeneric.cpp:1566`,
  `WiFiClient.cpp:389` og `WiFiUdp.cpp:beginPacket`.
- `framework-arduinoespressif32/libraries/WebServer/src/Parsing.cpp:44`,
  `WebServer.cpp:handleClient` og `WebServer.h:49`.
- `NTPClient/NTPClient.cpp:84`, Adafruit SSD1306 `display()` og Wire-konstruktøren.
- [Espressif: SNTP og systemtid, ESP-IDF 4.4](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/system/system_time.html).
- [Espressif: flash/cache og samtidighed](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-reference/storage/spi_flash_concurrency.html).

Versionsspecifikke bibliotekstal stammer fra den lokale installation; onlinekilderne
understøtter arkitekturvalgene, ikke en målt tidsgrænse for dette board.
