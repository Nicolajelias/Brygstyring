#include "ProcessHandler.h"
#include "EEPROMHandler.h"  // For Config
#include <Arduino.h>
#include <stdio.h>
#include <EEPROM.h>
#define EEPROM_PROCESS_STATE_START 100

// Definer BOILHEATUP-tiden (i sekunder). Her er den sat til 10 minutter.
static unsigned long boilHeatupTime = 10 * 60;

struct ProcessState {
  unsigned long processStartEpoch;
  uint8_t currentState; // gemt som uint8_t svarende til BrewState
  bool timerStarted;
};

// ============================
// STATISKE MEDLEMMER
// ============================

// NTP-klient
WiFiUDP ProcessHandler::ntpUDP;
NTPClient ProcessHandler::timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);

// Pins
uint8_t ProcessHandler::pinGas    = 0;
uint8_t ProcessHandler::pinPump   = 0;
uint8_t ProcessHandler::pinBuzzer = 0;
uint8_t ProcessHandler::pinButton = 0;

// Status for pumpe og gasventil
bool ProcessHandler::pumpOn     = false;
bool ProcessHandler::gasValveOn = false;

// Tidsstyring (brug af både epoch- og millis-tider)
unsigned long ProcessHandler::processStartEpoch = 0;  // NTP-tid (sekunder)
unsigned long ProcessHandler::processStartMillis = 0; // millis() – til nedtælling
bool ProcessHandler::timerStarted = false;            // Angiver om nedtællingen er startet
unsigned long ProcessHandler::pauseOffset = 0;        // Offset ved pause

// Procestrinvarigheder (i sekunder)
unsigned long ProcessHandler::mashTime    = 90 * 60;
unsigned long ProcessHandler::mashoutTime = 10 * 60;
unsigned long ProcessHandler::boilTime    = 60 * 60;

// Setpoints
float ProcessHandler::mashSetpoint    = 64.0f;
float ProcessHandler::mashoutSetpoint = 75.0f;
float ProcessHandler::hysteresis      = 1.0f;
// Ventil offset – den absolutte margin (f.eks. 5°C)
// Denne værdi opdateres i begin() ud fra EEPROM (tempOffset)
float ProcessHandler::valveOffset     = 5.0f;

// Buzzer
bool ProcessHandler::userConfirmed    = false;
bool ProcessHandler::buzzerActive     = false;
unsigned long ProcessHandler::buzzerStartTime = 0;

// Bryg-tilstand
ProcessHandler::BrewState ProcessHandler::currentState = ProcessHandler::BrewState::IDLE;
ProcessHandler::BrewState ProcessHandler::previousState = ProcessHandler::BrewState::IDLE;
bool ProcessHandler::boilingComplete = false;

// Flag til mashout – kan bruges til at tjekke, om mashout er gennemført, men i denne version springer vi direkte videre
// Derfor anvender vi ikke et ekstra flag her.
  
// Tidsstrenge til visning
String ProcessHandler::startTimeStr = "";
String ProcessHandler::endTimeStr   = "";

// ----------------------------
// Hjælpefunktion: Formatterer en epoch-tid (sekunder) til HH:MM:SS
// ----------------------------
String ProcessHandler::formatEpochTime(unsigned long epoch) {
  unsigned long rawTime = epoch % 86400UL;
  int hours   = rawTime / 3600;
  int minutes = (rawTime % 3600) / 60;
  int seconds = rawTime % 60;
  char buf[10];
  sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buf);
}

String ProcessHandler::getProcessStep() {
  switch (currentState) {
    case BrewState::IDLE:
      return "Idle";
    case BrewState::MASHING:
      return "M\x91skning";    // Brug CP437-koden for æ (0x91)
    case BrewState::MASHOUT:
      return "Udm\x91skning";  // Hvis du ønsker at vise æ her, hvis det er relevant
    case BrewState::BOILHEATUP:
      return "Opvarmning";
    case BrewState::BOILING:
      return timerStarted ? "Kogning" : "Venter p\x86 kogepunkt"; // Brug \x91 for æ
    case BrewState::PAUSED:
      return "PAUSE";
    default:
      return "Ukendt";
  }
}


// ============================
// PUBLIC METODER
// ============================
void ProcessHandler::begin(uint8_t gasP, uint8_t pumpP, uint8_t buzzerP, uint8_t buttonP) {
  pinGas    = gasP;
  pinPump   = pumpP;
  pinBuzzer = buzzerP;
  pinButton = buttonP;

  pinMode(pinGas, OUTPUT);
  pinMode(pinPump, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinButton, INPUT_PULLUP);

  digitalWrite(pinGas, LOW);
  digitalWrite(pinPump, LOW);
  digitalWrite(pinBuzzer, LOW);

  timeClient.begin();
  timeClient.update();

  // Hent den gemte konfiguration fra EEPROM
  Config cfg = EEPROMHandler::getConfig();
  setMashTime(cfg.mashTime);
  setMashoutTime(cfg.mashoutTime);
  setBoilTime(cfg.boilTime);
  setMashSetpoint(cfg.mashSetpoint);
  setMashoutSetpoint(cfg.mashoutSetpoint);
  setHysteresis(cfg.hysteresis);
  setValveOffset(cfg.tempOffset);

  // Forsøg at genoptage en eventuel gemt proces state
  if (!restoreProcessState()) {
    currentState = BrewState::IDLE;
    timerStarted = false;
  }
  
  Serial.println("[ProcessHandler] begin() -> " + getProcessStatus());
}

void ProcessHandler::update(float tGryde, float tVentil) {
  timeClient.update();
  handleBuzzer();

  switch (currentState) {
    case BrewState::IDLE:
      break;

    case BrewState::MASHING:
      pumpControl(true);
      temperatureControl(tGryde, mashSetpoint, tVentil);
      if (!timerStarted && (tGryde >= (mashSetpoint - hysteresis))) {
        if (!buzzerActive) {
          buzzerActive = true;
          Serial.println("[ProcessHandler] Mæskning setpoint nået. Tryk på knappen for at starte nedtælling.");
        }
        if (userConfirmed) {
          processStartMillis = millis();
          processStartEpoch  = timeClient.getEpochTime();
          timerStarted = true;
          startTimeStr = getFormattedTime();
          endTimeStr = formatEpochTime(processStartEpoch + mashTime);
          buzzerActive = false;
          userConfirmed = false;
          saveProcessState();
          Serial.println("[ProcessHandler] Mæskningsnedtælling startet.");
        }
      }
      if (timerStarted) {
        checkTimeAndNextStep(mashTime);
      }
      break;

    case BrewState::MASHOUT:
    pumpControl(true);
    temperatureControl(tGryde, mashoutSetpoint, tVentil);
    // Vent på at mashout-temperaturen er nået
    if (!timerStarted && (tGryde >= mashoutSetpoint)) {
        if (!buzzerActive) {
            buzzerActive = true;
            Serial.println("[ProcessHandler] Udmæskning setpoint nået. Tryk på knappen for at starte nedtælling af udmæskningstiden.");
        }
        if (userConfirmed) {
            processStartMillis = millis();
            processStartEpoch  = timeClient.getEpochTime();
            timerStarted = true;
            startTimeStr = getFormattedTime();
            endTimeStr = formatEpochTime(processStartEpoch + mashoutTime);
            buzzerActive = false;
            userConfirmed = false;
            saveProcessState();
            Serial.println("[ProcessHandler] Udmæskningsnedtælling startet.");
        }
    }
    if (timerStarted) {
        checkTimeAndNextStep(mashoutTime);
    }
    break;

    case BrewState::BOILHEATUP:
      // Under BOILHEATUP skal gasventilen være tændt og pumpen slukket.
      gasControl(true);
      pumpControl(false);
      if (!timerStarted) {
          processStartMillis = millis();
          processStartEpoch  = timeClient.getEpochTime();
          timerStarted = true;
          startTimeStr = getFormattedTime();
          endTimeStr = formatEpochTime(processStartEpoch + boilHeatupTime);
          // For at indikere, at opvarmningen er startet, kan du eventuelt aktivere buzzeren kort.
          buzzerActive = true;
          delay(200); // Buzz kort (200 ms)
          buzzerActive = false;
          saveProcessState();
          Serial.println("[ProcessHandler] BOILHEATUP nedtælling startet.");
      } else {
          if (getRemainingTime() == 0) {
              // Når de 30 min er udløbet, skal systemet vente på brugerens tryk for at starte kogetiden.
              // Tjek knappen direkte, da handleBuzzer() ikke vil sætte userConfirmed, hvis buzeren er slukket.
              if (digitalRead(pinButton) == LOW) {
                  delay(50); // Debounce
                  if (digitalRead(pinButton) == LOW) {
                      currentState = BrewState::BOILING;
                      timerStarted = false; // Kogetiden starter ved næste bekræftelse i BOILING
                      Serial.println("[ProcessHandler] BOILHEATUP bekræftet. Skifter til BOILING (venter på kogepunktbekræftelse).");
                      saveProcessState();
                  }
              }
          } else {
              checkTimeAndNextStep(boilHeatupTime);
          }
      }
      break;

      case BrewState::BOILING:
      // Under kogning skal gasventilen være tændt indtil kogetiden udløber.
      if (!boilingComplete) {
          // Sørg for, at gasventilen er tændt, og pumpen slukket.
          gasControl(true);
          pumpControl(false);
          if (!timerStarted) {
              // Indtil kogetiden starter, signalér med buzzeren, at der skal bekræftes for at starte kogetiden.
              if (!buzzerActive) {
                  buzzerActive = true;
                  Serial.println("[ProcessHandler] Kogepunkt: Vent på bekræftelse. Tryk på knappen for at starte kogetidsnedtællingen.");
              }
              if (userConfirmed) {
                  // Start kogetidsnedtællingen
                  processStartMillis = millis();
                  processStartEpoch  = timeClient.getEpochTime();
                  timerStarted = true;
                  startTimeStr = getFormattedTime();
                  endTimeStr = formatEpochTime(processStartEpoch + boilTime);
                  buzzerActive = false;
                  userConfirmed = false;
                  saveProcessState();
                  Serial.println("[ProcessHandler] Kogetidsnedtælling startet.");
              }
          } else {
              // Når kogetiden er i gang, hvis den udløber:
              if (getRemainingTime() == 0) {
                  boilingComplete = true;    // Markér at kogetiden er udløbet
                  gasControl(false);         // Sluk for gasventilen automatisk
                  // Start buzzerlyden for at signalere, at kogetiden er udløbet
                  if (!buzzerActive) {
                      buzzerActive = true;
                      Serial.println("[ProcessHandler] Kogetid udløbet. Buzzeren lyder indtil bruger bekræfter.");
                  }
              } else {
                  checkTimeAndNextStep(boilTime);
              }
          }
      } else {
          // Når kogetiden er udløbet (boilingComplete er true), vent på brugerbekræftelse.
          // Her tjekkes knappen direkte (du kan evt. bruge userConfirmed, hvis handleBuzzer() virker i denne fase).
          if (digitalRead(pinButton) == LOW) {
              delay(50); // debounce
              if (digitalRead(pinButton) == LOW) {
                  // Ved knaptryk afsluttes kogetidsfasen: buzzeren slukkes, og tilstanden sættes til IDLE.
                  boilingComplete = false;
                  timerStarted = false;
                  buzzerActive = false;
                  userConfirmed = false;
                  currentState = BrewState::IDLE;
                  saveProcessState();
                  Serial.println("[ProcessHandler] Kogetid bekræftet. Skifter til IDLE.");
              }
          }
      }
      break;

  }
}

void ProcessHandler::checkTimeAndNextStep(unsigned long stepTimeSec) {
  unsigned long nowMs = millis();
  unsigned long elapsedSec = (nowMs - processStartMillis) / 1000;
  if (elapsedSec >= stepTimeSec) {
    // For MASHING og MASHOUT kræves der bekræftelse før vi fortsætter
    if (currentState == BrewState::MASHING || currentState == BrewState::MASHOUT) {
      if (!userConfirmed) {
        if (!buzzerActive) {
          buzzerActive = true;
          Serial.println("[ProcessHandler] Tiden udløbet. Vent på bekræftelse for at skifte til næste trin.");
        }
        return; // Vent på at brugeren trykker.
      }
    }
    // For BOILHEATUP og BOILING (eller hvis bekræftelse allerede er givet) sker overgangen:
    nextStep();
  }
}

void ProcessHandler::startMashing() {
  currentState = BrewState::MASHING;
  timerStarted = false;
  saveProcessState();
  Serial.println("[ProcessHandler] startMashing -> MASHING");
}

void ProcessHandler::startMashout() {
  currentState = BrewState::MASHOUT;
  timerStarted = false;
  saveProcessState();
  Serial.println("[ProcessHandler] startMashout -> MASHOUT");
}

void ProcessHandler::startBoiling() {
  boilingComplete = false;
  currentState = BrewState::BOILING;
  timerStarted = true;
  processStartMillis = millis();
  timeClient.update();
  processStartEpoch  = timeClient.getEpochTime();
  startTimeStr = getFormattedTime();
  endTimeStr   = formatEpochTime(processStartEpoch + boilTime);
  buzzerActive = false;
  userConfirmed = false;
  gasControl(true);
  pumpControl(false);
  saveProcessState();
  Serial.println("[ProcessHandler] startBoiling -> BOILING (kogetid startet med det samme)");
}

void ProcessHandler::stopProcess() {
  currentState = BrewState::IDLE;
  timerStarted = false;
  gasControl(false);
  pumpControl(false);
  saveProcessState();
  Serial.println("[ProcessHandler] stopProcess -> IDLE");
}

void ProcessHandler::pauseProcess() {
  if (currentState != BrewState::IDLE && currentState != BrewState::PAUSED) {
    pauseOffset = millis() - processStartMillis;
    previousState = currentState;
    currentState = BrewState::PAUSED;
    gasControl(false);
    pumpControl(false);
    saveProcessState();
    Serial.println("[ProcessHandler] Proces pauset.");
  }
}

void ProcessHandler::resumeProcess() {
  if (currentState == BrewState::PAUSED) {
    processStartMillis = millis() - pauseOffset;
    currentState = previousState;
    saveProcessState();
    Serial.println("[ProcessHandler] Proces genoptaget.");
  }
}

void ProcessHandler::saveProcessState() {
  ProcessState ps;
  ps.processStartEpoch = processStartEpoch;
  ps.currentState = static_cast<uint8_t>(currentState);
  ps.timerStarted = timerStarted;
  EEPROM.put(EEPROM_PROCESS_STATE_START, ps);
  EEPROM.commit();
}

bool ProcessHandler::restoreProcessState() {
  ProcessState ps;
  EEPROM.get(EEPROM_PROCESS_STATE_START, ps);
  if (ps.processStartEpoch == 0)
    return false;
  
  timeClient.update();
  unsigned long currentEpoch = timeClient.getEpochTime();
  if (currentEpoch - ps.processStartEpoch < 3600) {
    currentState = static_cast<BrewState>(ps.currentState);
    timerStarted = ps.timerStarted;
    processStartEpoch = ps.processStartEpoch;
    unsigned long elapsed = currentEpoch - processStartEpoch;
    processStartMillis = millis() - (elapsed * 1000);
    Serial.println("[ProcessHandler] Process state restored.");
    return true;
  }
  return false;
}

void ProcessHandler::resetProcessState() {
  currentState = BrewState::IDLE;
  timerStarted = false;
  processStartEpoch = 0;
  processStartMillis = 0;
  gasControl(false);
  pumpControl(false);
  ProcessState ps = {0, static_cast<uint8_t>(BrewState::IDLE), false};
  EEPROM.put(EEPROM_PROCESS_STATE_START, ps);
  EEPROM.commit();
  Serial.println("[ProcessHandler] Process state reset.");
}

String ProcessHandler::getProcessStatus() {
  switch (currentState) {
    case BrewState::IDLE:
      return "Idle";
    case BrewState::MASHING:
      return timerStarted ? "Mæskning - Tid: " + getRemainingTimeFormatted() : "Varmer op til " + String(mashSetpoint, 1) + " °C - Tid: " + String(mashTime / 60) + " min";
    case BrewState::MASHOUT:
      return timerStarted ? "Udmæskning - Tid: " + getRemainingTimeFormatted() : "Varmer op til " + String(mashoutSetpoint, 1) + " °C - Tid: " + String(mashoutTime / 60) + " min";
    case BrewState::BOILHEATUP:
      return timerStarted ? "Opvarmning - Tid: " + getRemainingTimeFormatted() : "Opvarmning (30 min)";
    case BrewState::BOILING:
      return timerStarted ? "Kogning - Tid: " + getRemainingTimeFormatted() : "Venter på kogepunkt - Tid: " + String(boilTime / 60) + " min";
    case BrewState::PAUSED:
      return "PAUSE";
    default:
      return "Ukendt";
  }
}

unsigned long ProcessHandler::getRemainingTime() {
  unsigned long duration = 0;
  switch (currentState) {
    case BrewState::MASHING:
      duration = mashTime;
      break;
    case BrewState::MASHOUT:
      duration = mashoutTime;
      break;
    case BrewState::BOILHEATUP:
      duration = boilHeatupTime;
      break;
    case BrewState::BOILING:
      duration = boilTime;
      break;
    default:
      return 0;
  }
  if (!timerStarted) {
    return duration;
  }
  unsigned long elapsed = (millis() - processStartMillis) / 1000;
  return (elapsed >= duration) ? 0 : (duration - elapsed);
}

String ProcessHandler::getRemainingTimeFormatted() {
  unsigned long rem = getRemainingTime();
  unsigned long mm = rem / 60;
  unsigned long ss = rem % 60;
  char buf[16];
  sprintf(buf, "%02lu:%02lu", mm, ss);
  return String(buf);
}

String ProcessHandler::getFormattedTime() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  return formatEpochTime(epochTime);
}

String ProcessHandler::getProcessSymbol() {
  switch (currentState) {
    case BrewState::IDLE:    return "\xB0";
    case BrewState::PAUSED:  return "\xBA";
    default:                return "\x10";
  }
}

ProcessHandler::BrewState ProcessHandler::getCurrentState() {
  return currentState;
}

bool ProcessHandler::isTimerStarted() {
  return timerStarted;
}

bool ProcessHandler::togglePump() {
  if (currentState == BrewState::IDLE || currentState == BrewState::PAUSED) {
    pumpOn = !pumpOn;
    digitalWrite(pinPump, pumpOn ? HIGH : LOW);
  }
  return pumpOn;
}

bool ProcessHandler::toggleGasValve() {
  if (currentState == BrewState::IDLE || currentState == BrewState::PAUSED) {
    gasValveOn = !gasValveOn;
    digitalWrite(pinGas, gasValveOn ? HIGH : LOW);
  }
  return gasValveOn;
}

bool ProcessHandler::isPumpOn() {
  return pumpOn;
}

bool ProcessHandler::isGasValveOn() {
  return gasValveOn;
}

String ProcessHandler::getStartTime() {
  return startTimeStr;
}

String ProcessHandler::getEndTime() {
  if (!timerStarted)
    return "Venter på at setpoint er nået";
  return endTimeStr;
}

void ProcessHandler::setHysteresis(float value) {
  hysteresis = value;
}
float ProcessHandler::getHysteresis() {
  return hysteresis;
}

void ProcessHandler::setValveOffset(float offset) {
  valveOffset = offset;
}

float ProcessHandler::getValveOffset() {
  return valveOffset;
}

void ProcessHandler::setMashTime(unsigned long time) {
  mashTime = time;
}
unsigned long ProcessHandler::getMashTime() {
  return mashTime;
}

void ProcessHandler::setMashoutTime(unsigned long time) {
  mashoutTime = time;
}
unsigned long ProcessHandler::getMashoutTime() {
  return mashoutTime;
}

void ProcessHandler::setBoilTime(unsigned long time) {
  boilTime = time;
}
unsigned long ProcessHandler::getBoilTime() {
  return boilTime;
}

void ProcessHandler::setMashSetpoint(float temp) {
  mashSetpoint = temp;
}
float ProcessHandler::getMashSetpoint() {
  return mashSetpoint;
}

void ProcessHandler::setMashoutSetpoint(float temp) {
  mashoutSetpoint = temp;
}
float ProcessHandler::getMashoutSetpoint() {
  return mashoutSetpoint;
}

// ============================
// PRIVATE METODER
// ============================
void ProcessHandler::gasControl(bool state) {
  gasValveOn = state;
  digitalWrite(pinGas, state ? HIGH : LOW);
}

void ProcessHandler::pumpControl(bool state) {
  pumpOn = state;
  digitalWrite(pinPump, state ? HIGH : LOW);
}

void ProcessHandler::handleBuzzer() {
  if (buzzerActive) {
    digitalWrite(pinBuzzer, HIGH);
    if (digitalRead(pinButton) == LOW) {
      delay(50);
      if (digitalRead(pinButton) == LOW) {
        userConfirmed = true;
        buzzerActive = false;
        digitalWrite(pinBuzzer, LOW);
        Serial.println("[ProcessHandler] Bruger har bekræftet skift.");
      }
    }
  }
}

// Opdateret temperaturkontrol med hysterese og ventil offset
void ProcessHandler::temperatureControl(float currentTemp, float setpoint, float tVentil) {
  static unsigned long lastGasSwitchTime = 0;
  unsigned long now = millis();

  // Hvis vi ikke har ventet 5 sekunder siden sidste skift, gør intet.
  if (now - lastGasSwitchTime < 5000) {
    return;
  }

  // Hvis ventiltemperaturen overstiger setpoint + valveOffset, skal gassen være slukket.
  if (tVentil >= setpoint + valveOffset) {
    if (gasValveOn) {
      gasControl(false);
      lastGasSwitchTime = now;
    }
    return;
  }

  // Hvis grydetemperaturen er under setpoint, skal gassen være tændt.
  if (currentTemp < setpoint) {
    if (!gasValveOn) {
      gasControl(true);
      lastGasSwitchTime = now;
    }
  }
  else { // Hvis grydetemperaturen er lig med eller over setpoint, skal gassen være slukket.
    if (gasValveOn) {
      gasControl(false);
      lastGasSwitchTime = now;
    }
  }
}

void ProcessHandler::nextStep() {
  // Sluk outputs og deaktiver buzzeren
  gasControl(false);
  pumpControl(false);
  buzzerActive = false;
  userConfirmed = false;
  buzzerStartTime = millis();

  switch (currentState) {
    case BrewState::MASHING:
      currentState = BrewState::MASHOUT;
      timerStarted = false;  // Nulstil timer, så mashout-timeren starter fra 0
      Serial.println("[ProcessHandler] Skifter: MASHING -> MASHOUT");
      break;
    case BrewState::MASHOUT:
      // Når mashout-tiden udløber, skifter vi til BOILHEATUP
      currentState = BrewState::BOILHEATUP;
      timerStarted = false;
      Serial.println("[ProcessHandler] Mashout-tid udløbet. Skifter til BOILHEATUP (30 min nedtælling).");
      break;
    case BrewState::BOILHEATUP:
      // Når BOILHEATUP-tiden udløber, skifter vi til BOILING, men nedtællingen for kogetid starter først ved bekræftelse.
      currentState = BrewState::BOILING;
      timerStarted = false;
      Serial.println("[ProcessHandler] BOILHEATUP tid udløbet. Skifter til BOILING (venter på kogepunktbekræftelse).");
      break;
    case BrewState::BOILING:
      currentState = BrewState::IDLE;
      Serial.println("[ProcessHandler] Skifter: BOILING -> IDLE (færdig)");
      break;
    default:
      break;
  }
  saveProcessState();
}
