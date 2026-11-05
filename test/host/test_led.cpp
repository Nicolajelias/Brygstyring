#include <cassert>
#include <iostream>
#include "Arduino.h"
#include "StatusLED.h"
#include "esp32-hal-rgb-led.h"
#include "freertos/FreeRTOS.h"
uint32_t mockNow=0;
int mockPins[64]{};
SerialMock Serial;
unsigned mockTaskTicks=0;
void (*mockControlTask)(void*)=nullptr;
std::vector<std::array<int,3>> mockColors;
void digitalWrite(int,int) {}
int digitalRead(int) {return HIGH;}
void tick(unsigned count) {
  mockTaskTicks=count;
  try {mockControlTask(nullptr);} catch (TaskFinished&) {}
}
int main() {
  StatusLED::begin(48);
  StatusLED::setNetworkMode(StatusLED::Mode::WifiConnected);
  tick(1); assert((mockColors.back()==std::array<int,3>{0,0,64}));
  StatusLED::setProcessActive(true);
  tick(1); assert((mockColors.back()==std::array<int,3>{0,64,0}));
  StatusLED::setAwaitingConfirmation(true);
  tick(1); assert((mockColors.back()==std::array<int,3>{255,0,0}));
  tick(13); assert((mockColors.back()==std::array<int,3>{0,0,255}));
  tick(13); assert((mockColors.back()==std::array<int,3>{255,0,0}));
  StatusLED::setAwaitingConfirmation(false);
  tick(1); assert((mockColors.back()==std::array<int,3>{0,64,0}));
  StatusLED::notify();
  tick(1); assert((mockColors.back()==std::array<int,3>{255,0,0}));
  tick(55); assert((mockColors.back()==std::array<int,3>{0,64,0}));
  mockNow=UINT32_MAX-500;
  StatusLED::notify(); tick(1);
  assert((mockColors.back()==std::array<int,3>{255,0,0}));
  tick(55); assert((mockColors.back()==std::array<int,3>{0,64,0}));
  std::cout<<"PASS independent LED task: alarm priority, red/blue blink, acknowledgement, notification, millis rollover\n";
}
