#include "ProcessHandler.h"
#include "EEPROMHandler.h"  // For Config
#include "StatusLED.h"
#include "TemperatureHandler.h"
#include <Arduino.h>
#include <stdio.h>
#include "ControlRuntime.h"
#include "RuntimeLog.h"
#include "ClockService.h"
#include "ProcessStorage.h"
#include "PinConfig.h"
#include "TemperatureControlPolicy.h"
#include <Preferences.h>

// Definer BOILHEATUP-tiden (i sekunder). Her er den sat til 10 minutter.
static unsigned long boilHeatupTime = 10 * 60;

namespace {
  bool blackout = false, recoveryTimed = false, recoveryInvalid = false, expiredStep = false;
  ProcessStorage::Record interrupted;
  uint32_t recoveryAt = 0, recoveryElapsed = 0, checkpointAt = 0;
  bool buttonArmed = false, longHandled = false, shortPress = false;
  uint32_t pressedAt = 0;
  bool manualGas = false;
  bool useValveTemperature = !POT_ONLY_CONTROL;
  struct SensorPolicyRecord { uint8_t version = 1; uint8_t enabled = 0; };
  bool loadValvePolicy() {
    Preferences p; SensorPolicyRecord record;
    if (!p.begin("sensor-policy", true)) return !POT_ONLY_CONTROL;
    const bool valid = p.getBytesLength("selection") == sizeof(record) &&
      p.getBytes("selection", &record, sizeof(record)) == sizeof(record) &&
      record.version == 1 && record.enabled <= 1;
    p.end();
    return valid ? record.enabled != 0 : !POT_ONLY_CONTROL;
  }
  bool rawButton = false, stableButton = false;
  unsigned long buttonChangedAt = 0, pulseUntil = 0;
  bool pulseActive = false;
  bool buttonPressed() { return shortPress; }
  constexpr uint8_t BUZZER_CHANNEL = 7;
constexpr uint32_t BUZZER_FREQUENCY_HZ = 750;
constexpr uint8_t BUZZER_RESOLUTION_BITS = 10;

  void buzzerOn() {
    if (!BUZZER_ENABLED) return;
    ledcWriteTone(BUZZER_CHANNEL, BUZZER_FREQUENCY_HZ);
    ledcWrite(BUZZER_CHANNEL, 256); // ca. 25% duty for blødere lyd
  }

  void buzzerOff() {
    if (!BUZZER_ENABLED) return;
    ledcWrite(BUZZER_CHANNEL, 0);
  }
}

// ============================
// STATISKE MEDLEMMER
// ============================

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
HopAddition ProcessHandler::hopSchedule[MAX_HOP_ADDITIONS] = {};
bool ProcessHandler::hopTriggered[MAX_HOP_ADDITIONS] = {};
uint8_t ProcessHandler::hopCount = 0;
int8_t ProcessHandler::activeHopIndex = -1;
bool ProcessHandler::hopAlarmActive = false;
bool ProcessHandler::hopAlarmAwaitingRelease = false;

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
  ControlRuntime::Guard guard;
  if (epoch < 1609459200UL) return "--:--:--";
  unsigned long rawTime = epoch % 86400UL;
  int hours   = rawTime / 3600;
  int minutes = (rawTime % 3600) / 60;
  int seconds = rawTime % 60;
  char buf[10];
  sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buf);
}

String ProcessHandler::getProcessStep() {
  ControlRuntime::Guard guard;
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
  const bool savedValvePolicy = loadValvePolicy(); // Before control task starts.
  ControlRuntime::Guard guard;
  useValveTemperature = savedValvePolicy;
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
  if (BUZZER_ENABLED) {
    ledcSetup(BUZZER_CHANNEL, BUZZER_FREQUENCY_HZ, BUZZER_RESOLUTION_BITS);
    ledcAttachPin(pinBuzzer, BUZZER_CHANNEL);
  }
  buzzerOff();

  blackout = recoveryTimed = recoveryInvalid = expiredStep = false;
  checkpointAt = millis();
  buttonArmed = longHandled = shortPress = false;
  buzzerActive = userConfirmed = boilingComplete = false;
  previousState = BrewState::IDLE;
  resetHopAlarms();
  manualGas = false;
  rawButton = stableButton = false;
  gasValveOn = pumpOn = false;
  buttonChangedAt = millis();
  pulseActive = false;

  // Hent den gemte konfiguration fra EEPROM
  Config cfg = EEPROMHandler::getConfig();
  setMashTime(cfg.mashTime);
  setMashoutTime(cfg.mashoutTime);
  setBoilTime(cfg.boilTime);
  setMashSetpoint(cfg.mashSetpoint);
  setMashoutSetpoint(cfg.mashoutSetpoint);
  setHysteresis(cfg.hysteresis);
  setValveOffset(cfg.tempOffset);

  // Reset starts with outputs OFF; persisted state never resumes heating.
  if (!restoreProcessState()) {
    currentState = BrewState::IDLE;
    timerStarted = false;
  }
  
  RuntimeLog.println("[ProcessHandler] begin() -> " + getProcessStatus());
}

void ProcessHandler::update(float tGryde, float tVentil) {
  ControlRuntime::Guard guard;
  enforceTemperatureSafety();
  shortPress = false;
  const bool pressed = digitalRead(pinButton) == LOW;
  if (pressed != rawButton) { rawButton = pressed; buttonChangedAt = millis(); }
  if (millis() - buttonChangedAt >= 50 && stableButton != rawButton) {
    stableButton = rawButton;
    if (stableButton) { pressedAt = millis(); longHandled = false; }
    else { shortPress = buttonArmed && !longHandled; buttonArmed = true; }
  }
  if (!pressed && !stableButton) buttonArmed = true;
  if (stableButton && buttonArmed && !longHandled && millis()-pressedAt >= 3000) {
    longHandled = true;
    restartProcess();
    return;
  }
  if (blackout) { gasControl(false); pumpControl(false); recoveryReady(); return; }
  if (currentState != BrewState::IDLE && millis()-checkpointAt >= 30000) saveProcessState();
  if (expiredStep && currentState != BrewState::PAUSED) {
    gasControl(false); pumpControl(false); StatusLED::setAwaitingConfirmation(true);
    if (shortPress) { expiredStep = false; nextStep(); }
    return;
  }
  handleBuzzer();

  switch (currentState) {
    case BrewState::IDLE:
    case BrewState::PAUSED:
      break;

    case BrewState::MASHING:
      pumpControl(true);
      temperatureControl(tGryde, mashSetpoint, tVentil);
      if (!timerStarted && (tGryde >= (mashSetpoint - hysteresis))) {
        if (!buzzerActive) {
          buzzerActive = true;
          StatusLED::setAwaitingConfirmation(true);
          RuntimeLog.println("[ProcessHandler] Mæskning setpoint nået. Tryk på knappen for at starte nedtælling.");
        }
        if (userConfirmed) {
          processStartMillis = millis();
          processStartEpoch  = ClockService::epoch();
          timerStarted = true;
          startTimeStr = getFormattedTime();
          endTimeStr = formatEpochTime(processStartEpoch + mashTime);
          buzzerActive = false;
          userConfirmed = false;
          StatusLED::setAwaitingConfirmation(false);
          saveProcessState();
          RuntimeLog.println("[ProcessHandler] Mæskningsnedtælling startet.");
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
            StatusLED::setAwaitingConfirmation(true);
            RuntimeLog.println("[ProcessHandler] Udmæskning setpoint nået. Tryk på knappen for at starte nedtælling af udmæskningstiden.");
        }
        if (userConfirmed) {
            processStartMillis = millis();
            processStartEpoch  = ClockService::epoch();
            timerStarted = true;
            startTimeStr = getFormattedTime();
            endTimeStr = formatEpochTime(processStartEpoch + mashoutTime);
            buzzerActive = false;
            userConfirmed = false;
            StatusLED::setAwaitingConfirmation(false);
            saveProcessState();
            RuntimeLog.println("[ProcessHandler] Udmæskningsnedtælling startet.");
        }
    }
    if (timerStarted) {
        checkTimeAndNextStep(mashoutTime);
    }
    break;

    case BrewState::BOILHEATUP:
      // Under BOILHEATUP skal gasventilen være tændt og pumpen slukket.
      if (!isfinite(tGryde) || (useValveTemperature && !isfinite(tVentil))) {
          gasControl(false);
          pumpControl(false);
          return;
      }
      gasControl(true);
      pumpControl(false);
      if (!timerStarted) {
          processStartMillis = millis();
          processStartEpoch  = ClockService::epoch();
          timerStarted = true;
          startTimeStr = getFormattedTime();
          endTimeStr = formatEpochTime(processStartEpoch + boilHeatupTime);
          // For at indikere, at opvarmningen er startet, kan du eventuelt aktivere buzzeren kort.
          buzzerActive = true;
          StatusLED::setAwaitingConfirmation(true);
          buzzerOn();
          pulseUntil = millis() + 200;
          StatusLED::notify();
          pulseActive = true;
          buzzerActive = false;
          saveProcessState();
          RuntimeLog.println("[ProcessHandler] BOILHEATUP nedtælling startet.");
      } else {
          if (getRemainingTime() == 0) {
              StatusLED::setAwaitingConfirmation(true);
              // Når de 30 min er udløbet, skal systemet vente på brugerens tryk for at starte kogetiden.
              // Tjek knappen direkte, da handleBuzzer() ikke vil sætte userConfirmed, hvis buzeren er slukket.
              if (buttonPressed()) {
                  if (buttonPressed()) {
                      currentState = BrewState::BOILING;
                      timerStarted = false; // Kogetiden starter ved næste bekræftelse i BOILING
                      StatusLED::setAwaitingConfirmation(false);
                      RuntimeLog.println("[ProcessHandler] BOILHEATUP bekræftet. Skifter til BOILING (venter på kogepunktbekræftelse).");
                      saveProcessState();
                  }
              }
          } else {
              StatusLED::setAwaitingConfirmation(false);
              checkTimeAndNextStep(boilHeatupTime);
          }
      }
      break;

      case BrewState::BOILING:
      if (!isfinite(tGryde) || (useValveTemperature && !isfinite(tVentil))) {
          gasControl(false);
          pumpControl(false);
          return;
      }
      // Under kogning skal gasventilen være tændt indtil kogetiden udløber.
      if (!boilingComplete) {
          // Sørg for, at gasventilen er tændt, og pumpen slukket.
          gasControl(true);
          pumpControl(false);
          if (!timerStarted) {
              // Indtil kogetiden starter, signalér med buzzeren, at der skal bekræftes for at starte kogetiden.
              if (!buzzerActive) {
                  buzzerActive = true;
                  StatusLED::setAwaitingConfirmation(true);
                  RuntimeLog.println("[ProcessHandler] Kogepunkt: Vent på bekræftelse. Tryk på knappen for at starte kogetidsnedtællingen.");
              }
              if (userConfirmed) {
                  // Start kogetidsnedtællingen
                  processStartMillis = millis();
                  processStartEpoch  = ClockService::epoch();
                  timerStarted = true;
                  resetHopAlarms();
                  startTimeStr = getFormattedTime();
                  endTimeStr = formatEpochTime(processStartEpoch + boilTime);
                  buzzerActive = false;
                  userConfirmed = false;
                  StatusLED::setAwaitingConfirmation(false);
                  saveProcessState();
                  RuntimeLog.println("[ProcessHandler] Kogetidsnedtælling startet.");
              }
          } else {
              checkHopAlarms();
              // Når kogetiden er i gang, hvis den udløber:
              if (getRemainingTime() == 0) {
                  boilingComplete = true;    // Markér at kogetiden er udløbet
                  gasControl(false);         // Sluk for gasventilen automatisk
                  // Start buzzerlyden for at signalere, at kogetiden er udløbet
                  if (!buzzerActive) {
                      buzzerActive = true;
                      StatusLED::setAwaitingConfirmation(true);
                      RuntimeLog.println("[ProcessHandler] Kogetid udløbet. Buzzeren lyder indtil bruger bekræfter.");
                  }
              } else {
                  checkTimeAndNextStep(boilTime);
              }
          }
      } else {
          // Når kogetiden er udløbet (boilingComplete er true), vent på brugerbekræftelse.
          // Her tjekkes knappen direkte (du kan evt. bruge userConfirmed, hvis handleBuzzer() virker i denne fase).
          if (buttonPressed()) {
              if (buttonPressed()) {
                  // Ved knaptryk afsluttes kogetidsfasen: buzzeren slukkes, og tilstanden sættes til IDLE.
                  boilingComplete = false;
                  timerStarted = false;
                  buzzerActive = false;
                  userConfirmed = false;
                  currentState = BrewState::IDLE;
                  StatusLED::setAwaitingConfirmation(false);
                  saveProcessState();
                  RuntimeLog.println("[ProcessHandler] Kogetid bekræftet. Skifter til IDLE.");
              }
          }
      }
      break;

  }
}

void ProcessHandler::checkTimeAndNextStep(unsigned long stepTimeSec) {
  ControlRuntime::Guard guard;
  unsigned long nowMs = millis();
  unsigned long elapsedSec = (nowMs - processStartMillis) / 1000;
  if (elapsedSec >= stepTimeSec) {
    // For MASHING og MASHOUT kræves der bekræftelse før vi fortsætter
    if (currentState == BrewState::MASHING || currentState == BrewState::MASHOUT) {
      if (!userConfirmed) {
        if (!buzzerActive) {
          buzzerActive = true;
          StatusLED::setAwaitingConfirmation(true);
          RuntimeLog.println("[ProcessHandler] Tiden udløbet. Vent på bekræftelse for at skifte til næste trin.");
        }
        return; // Vent på at brugeren trykker.
      }
    }
    // For BOILHEATUP og BOILING (eller hvis bekræftelse allerede er givet) sker overgangen:
    nextStep();
  }
}

void ProcessHandler::startMashing() {
  ControlRuntime::Guard guard;
  if (blackout) return;
  if (ControlRuntime::maintenance()) return;
  gasControl(false);
  if (!TemperatureHandler::isGrydeValid() || (useValveTemperature && !TemperatureHandler::isVentilValid())) {
    gasControl(false);
    RuntimeLog.println("[Safety] Mashing rejected: temperature data unavailable");
    return;
  }
  expiredStep = false;
  currentState = BrewState::MASHING;
  timerStarted = false;
  saveProcessState();
  RuntimeLog.println("[ProcessHandler] startMashing -> MASHING");
}

void ProcessHandler::startMashout() {
  ControlRuntime::Guard guard;
  if (blackout) return;
  if (ControlRuntime::maintenance()) return;
  gasControl(false);
  if (!TemperatureHandler::isGrydeValid() || (useValveTemperature && !TemperatureHandler::isVentilValid())) {
    gasControl(false);
    RuntimeLog.println("[Safety] Mashout rejected: temperature data unavailable");
    return;
  }
  expiredStep = false;
  currentState = BrewState::MASHOUT;
  timerStarted = false;
  saveProcessState();
  RuntimeLog.println("[ProcessHandler] startMashout -> MASHOUT");
}

void ProcessHandler::startBoiling() {
  ControlRuntime::Guard guard;
  if (blackout) return;
  if (ControlRuntime::maintenance()) return;
  gasControl(false);
  if (!TemperatureHandler::isGrydeValid() || (useValveTemperature && !TemperatureHandler::isVentilValid())) {
    gasControl(false);
    RuntimeLog.println("[Safety] Boiling rejected: temperature data unavailable");
    return;
  }
  expiredStep = false;
  boilingComplete = false;
  currentState = BrewState::BOILING;
  timerStarted = true;
  resetHopAlarms();
  processStartMillis = millis();
  processStartEpoch  = ClockService::epoch();
  startTimeStr = getFormattedTime();
  endTimeStr   = formatEpochTime(processStartEpoch + boilTime);
  buzzerActive = false;
  userConfirmed = false;
  gasControl(true);
  pumpControl(false);
  saveProcessState();
  RuntimeLog.println("[ProcessHandler] startBoiling -> BOILING (kogetid startet med det samme)");
}

void ProcessHandler::stopProcess() {
  ControlRuntime::Guard guard;
  blackout = expiredStep = false;
  currentState = BrewState::IDLE;
  pulseActive = false;
  timerStarted = false;
  gasControl(false);
  pumpControl(false);
  buzzerActive = false;
  hopAlarmActive = false;
  hopAlarmAwaitingRelease = false;
  activeHopIndex = -1;
  buzzerOff();
  StatusLED::setAwaitingConfirmation(false);
  saveProcessState();
  RuntimeLog.println("[ProcessHandler] stopProcess -> IDLE");
}

void ProcessHandler::pauseProcess() {
  ControlRuntime::Guard guard;
  if (blackout) return;
  if (currentState != BrewState::IDLE && currentState != BrewState::PAUSED) {
    pauseOffset = millis() - processStartMillis;
    previousState = currentState;
    currentState = BrewState::PAUSED;
    pulseActive = false;
    gasControl(false);
    pumpControl(false);
    buzzerActive = false;
    buzzerOff();
    StatusLED::setAwaitingConfirmation(false);
    saveProcessState();
    RuntimeLog.println("[ProcessHandler] Proces pauset.");
  }
}

void ProcessHandler::resumeProcess() {
  ControlRuntime::Guard guard;
  if (blackout) return;
  if (ControlRuntime::maintenance()) return;
  gasControl(false);
  if (currentState == BrewState::PAUSED) {
    if (!TemperatureHandler::isGrydeHealthy() || (useValveTemperature && !TemperatureHandler::isVentilHealthy())) return;
    processStartMillis = millis() - pauseOffset;
    currentState = previousState;
    saveProcessState();
    RuntimeLog.println("[ProcessHandler] Proces genoptaget.");
  }
}

void ProcessHandler::saveProcessState() {
  ControlRuntime::Guard guard;
  if (blackout) return; // Keep the original checkpoint through repeated restarts.
  checkpointAt = millis();
  ProcessStorage::Record r;
  r.epoch = ClockService::epoch();
  r.state = static_cast<uint8_t>(currentState); r.previous = static_cast<uint8_t>(previousState);
  r.timer = timerStarted; r.completed = boilingComplete; r.expired = expiredStep;
  r.elapsed = timerStarted ? (currentState == BrewState::PAUSED ? pauseOffset : millis()-processStartMillis)/1000 : 0;
  r.elapsed = min(r.elapsed, uint32_t(86400));
  r.recipe.mashTime = mashTime; r.recipe.mashoutTime = mashoutTime; r.recipe.boilTime = boilTime;
  r.recipe.mashSetpoint = mashSetpoint; r.recipe.mashoutSetpoint = mashoutSetpoint;
  r.recipe.hysteresis = hysteresis; r.recipe.valveOffset = valveOffset;
  r.recipe.hopCount = hopCount;
  for (unsigned i=0; i<hopCount; ++i) { r.recipe.hops[i] = hopSchedule[i]; if (hopTriggered[i]) r.hopMask |= 1U<<i; }
  r.activeHop = hopAlarmActive ? activeHopIndex : -1;
  ProcessStorage::stage(r);
}

bool ProcessHandler::restoreProcessState() {
  ControlRuntime::Guard guard;
  currentState = BrewState::IDLE; timerStarted = false;
  blackout = ProcessStorage::load(interrupted) && interrupted.state != 0;
  recoveryTimed = recoveryInvalid = false;
  return blackout;
}

bool ProcessHandler::recoveryPending() { ControlRuntime::Guard guard; return blackout; }
bool ProcessHandler::recoveryReady() {
  ControlRuntime::Guard guard;
  if (!blackout) return false;
  if (!recoveryTimed && ClockService::synchronized()) {
    const uint32_t now = ClockService::epoch();
    recoveryAt = millis(); recoveryTimed = true;
    // Conservative upper bound: checkpoint age, including reboot/NTP wait, must be < 1h.
    recoveryInvalid = interrupted.epoch < 1609459200UL || now < interrupted.epoch || now-interrupted.epoch >= 3600;
    recoveryElapsed = interrupted.elapsed;
    if (!recoveryInvalid && interrupted.timer && interrupted.state != static_cast<uint8_t>(BrewState::PAUSED))
      recoveryElapsed += now-interrupted.epoch;
  }
  return recoveryTimed && !recoveryInvalid && !ControlRuntime::maintenance() &&
    TemperatureHandler::isGrydeHealthy() && (!useValveTemperature || TemperatureHandler::isVentilHealthy());
}
const char* ProcessHandler::recoveryMessage() {
  ControlRuntime::Guard guard;
  recoveryReady();
  if (!blackout) return "";
  if (!recoveryTimed) return "BLACKOUT - afventer internettid";
  if (recoveryInvalid) return "BLACKOUT - tidsdata ugyldige eller afbrydelse mindst 1 time. Afslut eller start forfra.";
  if (!TemperatureHandler::isGrydeHealthy() || (useValveTemperature && !TemperatureHandler::isVentilHealthy()))
    return "BLACKOUT - afventer gyldige sensorer";
  return "BLACKOUT - tryk Fortsaet paa webinterface";
}
const char* ProcessHandler::recoveryStep() {
  ControlRuntime::Guard guard;
  const uint8_t state = interrupted.state == 5 ? interrupted.previous : interrupted.state;
  switch (state) { case 1: return "Maeskning"; case 2: return "Udmaeskning";
    case 3: return "Opvarmning"; case 4: return "Kogning"; default: return "Ukendt"; }
}
unsigned long ProcessHandler::recoveryRemaining() {
  ControlRuntime::Guard guard;
  recoveryReady();
  const uint8_t state = interrupted.state == 5 ? interrupted.previous : interrupted.state;
  uint32_t duration = state == 1 ? interrupted.recipe.mashTime : state == 2 ? interrupted.recipe.mashoutTime :
    state == 3 ? boilHeatupTime : interrupted.recipe.boilTime;
  uint32_t elapsed = recoveryTimed ? recoveryElapsed : interrupted.elapsed;
  if (recoveryTimed && interrupted.timer && interrupted.state != 5) elapsed += (millis()-recoveryAt)/1000;
  return !interrupted.timer ? duration : elapsed >= duration ? 0 : duration-elapsed;
}
bool ProcessHandler::continueRecovery() {
  ControlRuntime::Guard guard;
  if (!recoveryReady()) return false;
  const uint32_t remaining = recoveryRemaining();
  const auto& r = interrupted;
  mashTime=r.recipe.mashTime; mashoutTime=r.recipe.mashoutTime; boilTime=r.recipe.boilTime;
  mashSetpoint=r.recipe.mashSetpoint; mashoutSetpoint=r.recipe.mashoutSetpoint;
  hysteresis=r.recipe.hysteresis; valveOffset=r.recipe.valveOffset;
  hopCount=r.recipe.hopCount;
  for (unsigned i=0; i<hopCount; ++i) { hopSchedule[i]=r.recipe.hops[i]; hopTriggered[i]=(r.hopMask & (1U<<i)) != 0; }
  activeHopIndex=r.activeHop; hopAlarmActive=r.activeHop>=0; hopAlarmAwaitingRelease=false;
  currentState=static_cast<BrewState>(r.state); previousState=static_cast<BrewState>(r.previous);
  timerStarted=r.timer; boilingComplete=r.completed;
  uint32_t elapsed=recoveryElapsed;
  if (r.timer && r.state != 5) elapsed += (millis()-recoveryAt)/1000;
  elapsed=min(elapsed,uint32_t(86400));
  processStartMillis=millis()-elapsed*1000UL; pauseOffset=elapsed*1000UL;
  processStartEpoch=ClockService::epoch()-elapsed;
  startTimeStr=formatEpochTime(processStartEpoch); endTimeStr=formatEpochTime(ClockService::epoch()+remaining);
  userConfirmed=buzzerActive=pulseActive=false;
  // An elapsed step waits for normal step acknowledgement, without any output pulse.
  expiredStep=r.expired || (r.timer && remaining == 0);
  blackout=false;
  gasControl(false); pumpControl(false);
  saveProcessState();
  return true;
}
void ProcessHandler::restartProcess() {
  ControlRuntime::Guard guard;
  if (ControlRuntime::maintenance()) return;
  resetProcessState();
  startMashing();
}

void ProcessHandler::resetProcessState() {
  ControlRuntime::Guard guard;
  blackout = expiredStep = false;
  currentState = BrewState::IDLE;
  timerStarted = false;
  processStartEpoch = processStartMillis = 0;
  gasControl(false); pumpControl(false);
  pulseActive = false; buzzerActive = false; buzzerOff();
  resetHopAlarms();
  boilingComplete = false; previousState = BrewState::IDLE;
  StatusLED::setAwaitingConfirmation(false);
  userConfirmed = false;
  startTimeStr = endTimeStr = "";
  saveProcessState();
}

String ProcessHandler::getProcessStatus() {
  ControlRuntime::Guard guard;
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
  ControlRuntime::Guard guard;
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
  ControlRuntime::Guard guard;
  unsigned long rem = getRemainingTime();
  unsigned long mm = rem / 60;
  unsigned long ss = rem % 60;
  char buf[16];
  sprintf(buf, "%02lu:%02lu", mm, ss);
  return String(buf);
}

String ProcessHandler::getFormattedTime() {
  ControlRuntime::Guard guard;
  if (!ClockService::synchronized()) return "--:--:--";
  unsigned long epochTime = ClockService::epoch();
  return formatEpochTime(epochTime);
}

String ProcessHandler::getProcessSymbol() {
  ControlRuntime::Guard guard;
  switch (currentState) {
    case BrewState::IDLE:    return "\xB0";
    case BrewState::PAUSED:  return "\xBA";
    default:                return "\x10";
  }
}

ProcessHandler::BrewState ProcessHandler::getCurrentState() {
  ControlRuntime::Guard guard;
  return currentState;
}

bool ProcessHandler::isTimerStarted() {
  ControlRuntime::Guard guard;
  return timerStarted;
}

void ProcessHandler::setHopSchedule(const HopAddition* additions, uint8_t count) {
  ControlRuntime::Guard guard;
  hopCount = min(count, MAX_HOP_ADDITIONS);
  for (uint8_t index = 0; index < MAX_HOP_ADDITIONS; ++index) {
    hopSchedule[index] = index < hopCount && additions ? additions[index] : HopAddition{};
  }
  resetHopAlarms();
}

uint8_t ProcessHandler::getHopCount() {
  ControlRuntime::Guard guard; return hopCount; }
const HopAddition* ProcessHandler::getHop(uint8_t index) {
  ControlRuntime::Guard guard; return index < hopCount ? &hopSchedule[index] : nullptr; }
bool ProcessHandler::isHopTriggered(uint8_t index) {
  ControlRuntime::Guard guard; return index < hopCount && hopTriggered[index]; }
bool ProcessHandler::isHopAlarmActive() {
  ControlRuntime::Guard guard; return hopAlarmActive; }
const char* ProcessHandler::getActiveHopName() {
  ControlRuntime::Guard guard;
  return activeHopIndex >= 0 && activeHopIndex < hopCount ? hopSchedule[activeHopIndex].name : "";
}

void ProcessHandler::acknowledgeHopAlarm() {
  ControlRuntime::Guard guard;
  if (!hopAlarmActive) return;
  RuntimeLog.printf("[Hops] Addition acknowledged: %s\n", getActiveHopName());
  hopAlarmActive = false;
  hopAlarmAwaitingRelease = false;
  activeHopIndex = -1;
  saveProcessState();
  if (!buzzerActive) {
    buzzerOff();
    StatusLED::setAwaitingConfirmation(false);
  }
}

bool ProcessHandler::togglePump() {
  ControlRuntime::Guard guard;
  if (!blackout && !ControlRuntime::maintenance() && (currentState == BrewState::IDLE || currentState == BrewState::PAUSED)) {
    pumpOn = !pumpOn;
    digitalWrite(pinPump, pumpOn ? HIGH : LOW);
  }
  return pumpOn;
}

bool ProcessHandler::toggleGasValve() {
  ControlRuntime::Guard guard;
  if (!blackout && !ControlRuntime::maintenance() && (currentState == BrewState::IDLE || currentState == BrewState::PAUSED)) {
    gasValveOn = !gasValveOn;
    manualGas = gasValveOn;
    digitalWrite(pinGas, gasValveOn ? HIGH : LOW);
  }
  return gasValveOn;
}
bool ProcessHandler::isGasManual() {
  ControlRuntime::Guard guard; return manualGas && gasValveOn; }

bool ProcessHandler::isPumpOn() {
  ControlRuntime::Guard guard;
  return pumpOn;
}

bool ProcessHandler::isGasValveOn() {
  ControlRuntime::Guard guard;
  return gasValveOn;
}

String ProcessHandler::getStartTime() {
  ControlRuntime::Guard guard;
  return startTimeStr;
}

String ProcessHandler::getEndTime() {
  ControlRuntime::Guard guard;
  if (!timerStarted)
    return "Venter på at setpoint er nået";
  return endTimeStr;
}

void ProcessHandler::setHysteresis(float value) {
  ControlRuntime::Guard guard;
  hysteresis = value;
}
float ProcessHandler::getHysteresis() {
  ControlRuntime::Guard guard;
  return hysteresis;
}

void ProcessHandler::setValveOffset(float offset) {
  ControlRuntime::Guard guard;
  valveOffset = offset;
}

float ProcessHandler::getValveOffset() {
  ControlRuntime::Guard guard;
  return valveOffset;
}

void ProcessHandler::setMashTime(unsigned long time) {
  ControlRuntime::Guard guard;
  mashTime = time;
}
unsigned long ProcessHandler::getMashTime() {
  ControlRuntime::Guard guard;
  return mashTime;
}

void ProcessHandler::setMashoutTime(unsigned long time) {
  ControlRuntime::Guard guard;
  mashoutTime = time;
}
unsigned long ProcessHandler::getMashoutTime() {
  ControlRuntime::Guard guard;
  return mashoutTime;
}

void ProcessHandler::setBoilTime(unsigned long time) {
  ControlRuntime::Guard guard;
  boilTime = time;
}
unsigned long ProcessHandler::getBoilTime() {
  ControlRuntime::Guard guard;
  return boilTime;
}

void ProcessHandler::setMashSetpoint(float temp) {
  ControlRuntime::Guard guard;
  mashSetpoint = temp;
}
float ProcessHandler::getMashSetpoint() {
  ControlRuntime::Guard guard;
  return mashSetpoint;
}

void ProcessHandler::setMashoutSetpoint(float temp) {
  ControlRuntime::Guard guard;
  mashoutSetpoint = temp;
}
float ProcessHandler::getMashoutSetpoint() {
  ControlRuntime::Guard guard;
  return mashoutSetpoint;
}

// ============================
// PRIVATE METODER
// ============================
void ProcessHandler::gasControl(bool state) {
  ControlRuntime::Guard guard;
  manualGas = false;
  // Automatic gas only. Manual web operation in IDLE/PAUSED has a separate path.
  state = state && !blackout && TemperatureHandler::isGrydeHealthy() && (!useValveTemperature || TemperatureHandler::isVentilHealthy());
  gasValveOn = state;
  digitalWrite(pinGas, state ? HIGH : LOW);
}

bool ProcessHandler::usesValveTemperature() {
  ControlRuntime::Guard guard;
  return useValveTemperature;
}

bool ProcessHandler::setUseValveTemperature(bool enabled) {
  // Called by the web/main task only. Never hold the control mutex during NVS I/O.
  if (ControlRuntime::maintenance()) return false;
  Preferences p;
  SensorPolicyRecord record; record.enabled = enabled ? 1 : 0;
  if (!p.begin("sensor-policy", false)) return false;
  SensorPolicyRecord saved;
  const bool unchanged = p.getBytesLength("selection") == sizeof(saved) &&
    p.getBytes("selection", &saved, sizeof(saved)) == sizeof(saved) &&
    saved.version == record.version && saved.enabled == record.enabled;
  const bool ok = unchanged || p.putBytes("selection", &record, sizeof(record)) == sizeof(record);
  p.end();
  if (!ok) return false; // Keep the active choice if persistence fails.
  ControlRuntime::Guard guard;
  if (enabled == useValveTemperature) return true;
  useValveTemperature = enabled;
  if (enabled && !manualGas) gasControl(false);
  enforceTemperatureSafety();
  return true;
}

void ProcessHandler::enforceTemperatureSafety() {
  ControlRuntime::Guard guard;
  if (!manualGas && gasValveOn && (!TemperatureHandler::isGrydeHealthy() || (useValveTemperature && !TemperatureHandler::isVentilHealthy()))) {
    gasControl(false);
    RuntimeLog.println("[Safety] Gas closed: temperature fault or stale data");
  }
}

void ProcessHandler::pumpControl(bool state) {
  ControlRuntime::Guard guard;
  state = state && !blackout;
  pumpOn = state;
  digitalWrite(pinPump, state ? HIGH : LOW);
}

void ProcessHandler::handleBuzzer() {
  ControlRuntime::Guard guard;
  if (pulseActive) {
    if (static_cast<int32_t>(millis() - pulseUntil) < 0) return;
    pulseActive = false; buzzerOff(); StatusLED::setAwaitingConfirmation(false);
  }
  if (hopAlarmActive) {
    buzzerOn();
    StatusLED::setAwaitingConfirmation(true);
    if (hopAlarmAwaitingRelease) {
      if (!buttonPressed()) hopAlarmAwaitingRelease = false;
      return;
    }
    if (buttonPressed()) {
      if (buttonPressed()) acknowledgeHopAlarm();
    }
    return;
  }
  if (buzzerActive) {
    buzzerOn();
    StatusLED::setAwaitingConfirmation(true);
    if (buttonPressed()) {
      if (buttonPressed()) {
        userConfirmed = true;
        buzzerActive = false;
        buzzerOff();
        StatusLED::setAwaitingConfirmation(false);
        RuntimeLog.println("[ProcessHandler] Bruger har bekræftet skift.");
      }
    }
  }
  else {
    buzzerOff();
  }
}

void ProcessHandler::resetHopAlarms() {
  ControlRuntime::Guard guard;
  for (uint8_t index = 0; index < MAX_HOP_ADDITIONS; ++index) hopTriggered[index] = false;
  hopAlarmActive = false;
  hopAlarmAwaitingRelease = false;
  activeHopIndex = -1;
}

void ProcessHandler::checkHopAlarms() {
  ControlRuntime::Guard guard;
  if (currentState != BrewState::BOILING || !timerStarted || hopAlarmActive) return;
  const unsigned long remaining = getRemainingTime();
  for (uint8_t index = 0; index < hopCount; ++index) {
    if (!hopTriggered[index] && remaining <= hopSchedule[index].secondsBeforeBoilEnd) {
      hopTriggered[index] = true;
      activeHopIndex = index;
      hopAlarmActive = true;
      saveProcessState();
      hopAlarmAwaitingRelease = true;
      RuntimeLog.printf("[Hops] Add now: %s (%lu min before boil end)\n", hopSchedule[index].name,
                    hopSchedule[index].secondsBeforeBoilEnd / 60UL);
      return;
    }
  }
}

// Opdateret temperaturkontrol med hysterese og ventil offset
void ProcessHandler::temperatureControl(float currentTemp, float setpoint, float tVentil) {
  ControlRuntime::Guard guard;
  static unsigned long lastGasSwitchTime = 0;
  unsigned long now = millis();

  // Missing or stale sensor data always wins over the minimum switch interval.
  if (!isfinite(currentTemp) || (useValveTemperature && !isfinite(tVentil))) {
    if (gasValveOn) {
      gasControl(false);
      lastGasSwitchTime = now;
      RuntimeLog.println("[Safety] Gas closed: temperature data unavailable");
    }
    return;
  }

  // Hvis vi ikke har ventet 5 sekunder siden sidste skift, gør intet.
  if (now - lastGasSwitchTime < 5000) {
    return;
  }

  // Hvis ventiltemperaturen overstiger setpoint + valveOffset, skal gassen være slukket.
  if (useValveTemperature && tVentil >= setpoint + valveOffset) {
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
  ControlRuntime::Guard guard;
  // Sluk outputs og deaktiver buzzeren
  gasControl(false);
  pumpControl(false);
  buzzerActive = false;
  buzzerOff();
  StatusLED::setAwaitingConfirmation(false);
  userConfirmed = false;
  buzzerStartTime = millis();

  switch (currentState) {
    case BrewState::MASHING:
      currentState = BrewState::MASHOUT;
      timerStarted = false;  // Nulstil timer, så mashout-timeren starter fra 0
      RuntimeLog.println("[ProcessHandler] Skifter: MASHING -> MASHOUT");
      break;
    case BrewState::MASHOUT:
      // Når mashout-tiden udløber, skifter vi til BOILHEATUP
      currentState = BrewState::BOILHEATUP;
      timerStarted = false;
      RuntimeLog.println("[ProcessHandler] Mashout-tid udløbet. Skifter til BOILHEATUP (30 min nedtælling).");
      break;
    case BrewState::BOILHEATUP:
      // Når BOILHEATUP-tiden udløber, skifter vi til BOILING, men nedtællingen for kogetid starter først ved bekræftelse.
      currentState = BrewState::BOILING;
      timerStarted = false;
      RuntimeLog.println("[ProcessHandler] BOILHEATUP tid udløbet. Skifter til BOILING (venter på kogepunktbekræftelse).");
      break;
    case BrewState::BOILING:
      currentState = BrewState::IDLE;
      RuntimeLog.println("[ProcessHandler] Skifter: BOILING -> IDLE (færdig)");
      break;
    default:
      break;
  }
  saveProcessState();
}
