#ifndef PROCESS_HANDLER_H
#define PROCESS_HANDLER_H

#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

class ProcessHandler {
public:
  enum class BrewState { IDLE, MASHING, MASHOUT, BOILHEATUP, BOILING, PAUSED };

  // Initiering og opdatering
  static void begin(uint8_t gasP, uint8_t pumpP, uint8_t buzzerP, uint8_t buttonP);
  static void update(float tGryde, float tVentil);
  static String getProcessStep();

  // Start/stop for de enkelte trin
  static void startMashing();
  static void startMashout();
  static void startBoiling();
  static void stopProcess();
  static void pauseProcess();
  static void resumeProcess();
  
  // Funktioner til at gemme og gendanne proces state
  static void saveProcessState();
  static bool restoreProcessState();
  static void resetProcessState();

  // Status og tid
  static String getProcessStatus();
  static unsigned long getRemainingTime();
  static String getRemainingTimeFormatted();
  static String getStartTime();
  static String getEndTime();
  static String getFormattedTime();
  static String getOLEDStatus();
  static String getProcessSymbol();
  static bool mashoutComplete;
  static bool boilingComplete;

  // Ekstra getters (valgfrit)
  static BrewState getCurrentState();
  static bool isTimerStarted();

  // Toggle-funktioner
  static bool togglePump();
  static bool toggleGasValve();
  static bool isPumpOn();
  static bool isGasValveOn();

  // Konfigurationsparametre
  static void setHysteresis(float value);
  static float getHysteresis();
  static void setValveOffset(float value);
  static float getValveOffset();
  static void setMashTime(unsigned long time);       // i sekunder
  static unsigned long getMashTime();
  static void setMashoutTime(unsigned long time);
  static unsigned long getMashoutTime();
  static void setBoilTime(unsigned long time);
  static unsigned long getBoilTime();
  static void setMashSetpoint(float temp);
  static float getMashSetpoint();
  static void setMashoutSetpoint(float temp);
  static float getMashoutSetpoint();

private:
  // Hardwarestyring
  static void gasControl(bool state);
  static void pumpControl(bool state);
  static void handleBuzzer();
  // Ændret signatur for at inkludere ventiltemperatur
  static void temperatureControl(float currentTemp, float setpoint, float tVentil);

  // Tidsstyring
  static void checkTimeAndNextStep(unsigned long stepTimeSec);
  static void nextStep();
  static String formatEpochTime(unsigned long epoch);

  // NTP
  static WiFiUDP ntpUDP;
  static NTPClient timeClient;

  // Hardware pins
  static uint8_t pinGas;
  static uint8_t pinPump;
  static uint8_t pinBuzzer;
  static uint8_t pinButton;

  // Status for pumpe og gasventil
  static bool pumpOn;
  static bool gasValveOn;

  // Tidsvariabler
  static unsigned long processStartEpoch;
  static unsigned long processStartMillis;
  static bool timerStarted;
  static unsigned long pauseOffset;       // Tid der var forløbet, da pausen aktiveres
  
  // Procestrinvarigheder (i sekunder)
  static unsigned long mashTime;
  static unsigned long mashoutTime;
  static unsigned long boilTime;

  // Temperatur-setpoints og hysterese
  static float mashSetpoint;
  static float mashoutSetpoint;
  static float hysteresis;
  // Tilføjet: ventil offset (max ventiltemperatur = setpoint + valveOffset)
  static float valveOffset;

  // Buzzer
  static bool userConfirmed;
  static bool buzzerActive;
  static unsigned long buzzerStartTime;

  // Bryg-tilstand og visningstider
  static BrewState currentState;
  static BrewState previousState;         // Gemmer den aktive tilstand før pause
  static String startTimeStr;
  static String endTimeStr;
};

#endif // PROCESS_HANDLER_H
