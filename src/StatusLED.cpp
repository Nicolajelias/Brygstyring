#include "StatusLED.h"
#include <Arduino.h>
#include <esp32-hal-rgb-led.h>
#include <math.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "PinConfig.h"

namespace {
  std::atomic<StatusLED::Mode> networkMode{StatusLED::Mode::Off};
  std::atomic<uint32_t> notificationUntil{0};
  std::atomic<bool> processActive{false};
  std::atomic<bool> awaitingConfirmation{false};
  bool ledReady = false;

  uint8_t maxBrightness = 64;

  bool blinkOn = false;
  unsigned long lastBlinkToggle = 0;

  uint8_t currentR = 0;
  uint8_t currentG = 0;
  uint8_t currentB = 0;

  void showColor(uint8_t r, uint8_t g, uint8_t b) {
    if (!ledReady) {
      return;
    }
    if (currentR == r && currentG == g && currentB == b) {
      return;
    }
    currentR = r;
    currentG = g;
    currentB = b;
    neopixelWrite(PIN_RGB_LED, r, g, b);
  }

  void showScaledColor(uint8_t baseR, uint8_t baseG, uint8_t baseB, uint8_t brightness) {
    float scale = brightness / 255.0f;
    auto clamp = [](int value) -> uint8_t {
      return value < 0 ? 0 : (value > 255 ? 255 : static_cast<uint8_t>(value));
    };
    showColor(
      clamp(lroundf(baseR * scale)),
      clamp(lroundf(baseG * scale)),
      clamp(lroundf(baseB * scale))
    );
  }
  void ledTask(void*) {
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
      StatusLED::update();
      vTaskDelayUntil(&wake, pdMS_TO_TICKS(20));
    }
  }
}

void StatusLED::begin(uint8_t pin, uint8_t brightness) {
  (void)pin; // vi bruger PIN_RGB_LED fra PinConfig
  maxBrightness = brightness;

  if (PIN_RGB_LED_PWR >= 0) {
    pinMode(PIN_RGB_LED_PWR, OUTPUT);
    digitalWrite(PIN_RGB_LED_PWR, HIGH);
    delay(10);
  }

  ledReady = true;
  networkMode = StatusLED::Mode::Off;
  processActive = false;
  showColor(0, 0, 0);
  if (xTaskCreatePinnedToCore(ledTask, "status-led", 2048, nullptr, 1, nullptr, 0) != pdPASS)
    Serial.println("[LED] Could not start visual alarm task");
}

void StatusLED::setNetworkMode(StatusLED::Mode mode) {
  networkMode = mode;
}

void StatusLED::notify() { notificationUntil.store((uint32_t(millis()) + 1000U) | 1U); }

void StatusLED::setProcessActive(bool active) {
  processActive = active;
}

void StatusLED::setAwaitingConfirmation(bool active) {
  awaitingConfirmation = active;
}

void StatusLED::update() {
  if (!ledReady) {
    return;
  }

  unsigned long now = millis();
  uint32_t until = notificationUntil.load();
  if (until && static_cast<int32_t>(uint32_t(now) - until) >= 0) {
    notificationUntil.compare_exchange_strong(until, 0);
    until = notificationUntil.load();
  }
  static bool wasAwaiting = false;
  static Mode previousMode = Mode::Off;
  const Mode mode = networkMode.load();
  const bool awaiting = awaitingConfirmation.load() || until != 0;
  if (awaiting != wasAwaiting || mode != previousMode) {
    blinkOn = true; lastBlinkToggle = now; wasAwaiting = awaiting; previousMode = mode;
  }

  if (awaiting) {
    if (now - lastBlinkToggle >= 250) {
      lastBlinkToggle = now;
      blinkOn = !blinkOn;
    }
    if (blinkOn) {
      showColor(255, 0, 0);
    } else {
      showColor(0, 0, 255);
    }
    return;
  }

  if (processActive) {
    showScaledColor(0, 255, 0, maxBrightness);
    return;
  }

  switch (mode) {
    case StatusLED::Mode::Off:
      showColor(0, 0, 0);
      break;

    case StatusLED::Mode::WifiConnecting:
      if (now - lastBlinkToggle >= 400) {
        lastBlinkToggle = now;
        blinkOn = !blinkOn;
        showScaledColor(0, 0, 255, blinkOn ? maxBrightness : 0);
      }
      break;

    case StatusLED::Mode::WifiConnected:
      showScaledColor(0, 0, 255, maxBrightness);
      break;

    case StatusLED::Mode::AccessPoint:
      if (now - lastBlinkToggle >= 400) {
        lastBlinkToggle = now;
        blinkOn = !blinkOn;
        showScaledColor(255, 0, 0, blinkOn ? maxBrightness : 0);
      }
      break;
  }
}
