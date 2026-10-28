# Kalibrering af de to PT100

Åbn **System → Temperaturkalibrering**, eller gå til `/calibration` på controlleren.
Gryde og ventil har hver deres kalibrering. Display, dashboard og processtyring
bruger den korrigerede temperatur; kalibreringssiden viser også den ukorrigerede
temperatur fra PT100/MAX31865.

## Indtastning

1. Stop brygprocessen og sluk gas. Pumpen må gerne cirkulere under kalibreringen.
   Kalibrering kan kun gemmes i IDLE med gas slukket og uden igangværende OTA.
2. Anbring den sensor, der kalibreres, og et egnet referencetermometer ved samme
   stabile temperatur. Vent på temperaturudligning. Brug den faktiske reference,
   ikke en antagelse om at kogende vand altid er præcis 100 °C.
3. Vælg **Ét punkt** for en konstant korrektion. Indtast sidens **ukorrigerede**
   temperatur og referencetermometerets temperatur ved det samme punkt.
4. Vælg **To punkter**, hvis både offset og hældning skal justeres. Registrér
   et lavt og et højt punkt, der omfatter det ønskede arbejdsområde. Både de
   målte temperaturer og referencerne skal have mindst 10 °C afstand.
5. Tryk **Gem kalibrering for begge sensorer**. Den anden sensors værdier
   beholdes, hvis du ikke ændrer dens felter. Vent på en ny gyldig måling,
   og kontrollér ved en tredje temperatur, som ikke blev brugt til kalibreringen.

Brug altid den ukorrigerede temperatur som måleværdi. Brug af den allerede
kalibrerede temperatur ved en ny kalibrering giver en forkert korrektion.
Kalibrering kan ikke rette forkert 3-lederforbindelse, forkert Rref eller en
defekt sensor. Begge rå temperaturer skal være plausible, før kalibrering bruges.

## Beregning og grænser

Korrektionen er `T_korrigeret = gain × T_ukorrigeret + offset`.

- Ingen: gain = 1, offset = 0.
- Ét punkt: gain = 1, offset = reference1 − måling1.
- To punkter: gain = (reference2 − reference1) / (måling2 − måling1),
  offset = reference1 − gain × måling1.

Eksempel: ukorrigeret 64,2 °C og reference 65,0 °C giver offset +0,8 °C.
Et topunktseksempel med måling/reference 1/0 °C og 101/102 °C giver gain 1,02
og offset −1,02 °C. Korrektionen interpolerer lineært mellem punkterne og
ekstrapolerer uden for dem; nøjagtigheden skal verificeres i hele arbejdsområdet.

Serveren kræver endelige tal mellem −20 og +125 °C, gain mellem 0,9 og 1,1,
offset højst ±10 °C og en afvigelse på højst ±10 °C ved hvert aktivt punkt.
Ugyldige, manglende og ikke-numeriske felter afvises uden at ændre kalibreringen.
Både ukorrigeret og korrigeret temperatur kontrolleres mod −20 til +125 °C
ved hver måling; kalibrering kan ikke gøre en åben/kortsluttet sensor gyldig.

## Lagring og nulstilling

Begge kanalers oprindelige måle-/referencepunkter gemmes samlet som én
versionsmærket post i ESP32 NVS (`pt100-cal`, nøgle `channels`). Der skrives
kun, når du gemmer. Ved fejl under lagring returneres en fejl, og den aktive
kalibrering i RAM ændres ikke. De eksisterende EEPROM-bryg-/netværksfelter
udvides ikke. Kalibrering følger sensorkanalen; flyttes eller udskiftes en sensor,
skal den tilhørende kalibrering vurderes på ny.

På første opstart uden kalibreringsdata bruges ingen korrektion. En ugyldig
eller ukendt lagringsversion afvises, og standardkalibrering anvendes med
en seriel logbesked. Almindelig genstart og en firmwareopdatering, som bevarer
NVS, beholder kalibreringen. Fuld sletning af flash fjerner den.

Vælg **Ingen** for den ønskede sensor og gem for at nulstille den. Systemets
**Nulstil alt** nulstiller også begge sensorers kalibrering og slukker processen.
Brygprofilens **Ventil-offset** er fortsat en separat procesgrænse og er ikke
en sensorkalibrering.

## API og verifikation

`GET /api/calibration` returnerer `pot` og `valve` med `points`, `measured1`,
`reference1`, `measured2`, `reference2`, `gain`, `offset`, `uncalibrated` og
`value`. Utilgængelige målinger repræsenteres som JSON `null`. `canSave` angiver,
om proces-/gas-/OTA-tilstanden tillader en ændring. Polling af denne rute
foretager ingen NTP-opdatering.

`POST /api/calibration` bruger form-urlencoded felter `potPoints` og
`valvePoints` (0/1/2), samt for hver aktiv kanal eksempelvis `potmeasured1`
og `potreference1`; to punkter kræver også `potmeasured2` og `potreference2`.
Samme mønster gælder for `valve`. Der returneres 400 ved ugyldige data, 409
ved aktiv proces/gas, 503 under OTA og 500 ved lagringsfejl. Gemning gælder
begge kanaler samlet. Alle ruter følger projektets eksisterende lokale
webinterface og har samme adgangsmodel som bryg-/indstillingsruterne.

Regressionstestene i `test/host` kompilerer de faktiske `TemperatureHandler`,
`ProcessHandler`, `TemperatureCalibrationWeb` og Adafruit MAX31865 1.6.2 med
simulerede perifere enheder. Kør `python scripts/test_pt100.py` på Windows
med PlatformIOs `toolchain-gccmingw32` installeret. Kør først `platformio run`,
så biblioteksafhængigheden er hentet. Mockens ADC afslutter efter 66 ms.

Testene dækker timing, command/readback, fejl og timeout, friske data,
rollover, alle seks procesfaser, beregning, separat kalibrering, genindlæsning,
NVS-fejl og HTTP-validering/adgangsbetingelser. De erstatter ikke en hardwaretest
med præcisionsmodstande, de faktiske kabler og måling af den fysiske gasudgang.
