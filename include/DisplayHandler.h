#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

#include <Arduino.h>

class DisplayHandler {
public:
  static void begin();
  static void showMessage(const String &msg);
  // Nu med 5 parametre: tGryde, tVentil, showVentil, processStatus og remainingTime
  static void update(float tGryde, float tVentil, bool showVentil, const String &processStep, unsigned long remainingTime);
  static void displayBeerAnimation();
};

#endif // DISPLAYHANDLER_H
