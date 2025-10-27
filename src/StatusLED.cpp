#include "StatusLED.h"
#include <Arduino.h>
#include <esp32-hal-rgb-led.h>
#include <math.h>
#include "PinConfig.h"

namespace {
  StatusLED::Mode networkMode = StatusLED::Mode::Off;
  bool processActive = false;
  bool ledReady = false;

  uint8_t maxBrightness = 64;

  bool blinkOn = false;
  unsigned long lastBlinkToggle = 0;
  unsigned long lastRainbowUpdate = 0;
  float rainbowHue = 0.0f; // 0.0 - 1.0

  uint8_t currentR = 0;
  uint8_t currentG = 0;
  uint8_t currentB = 0;

  uint8_t clampBrightness(int value) {
    return value < 0 ? 0 : (value > 255 ? 255 : static_cast<uint8_t>(value));
  }

  void hsvToRgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
    h = fmodf(h, 1.0f);
    if (h < 0.0f) {
      h += 1.0f;
    }
    s = constrain(s, 0.0f, 1.0f);
    v = constrain(v, 0.0f, 1.0f);

    float i = floorf(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    float rf, gf, bf;
    switch (static_cast<int>(i) % 6) {
      case 0: rf = v; gf = t; bf = p; break;
      case 1: rf = q; gf = v; bf = p; break;
      case 2: rf = p; gf = v; bf = t; break;
      case 3: rf = p; gf = q; bf = v; break;
      case 4: rf = t; gf = p; bf = v; break;
      default: rf = v; gf = p; bf = q; break;
    }

    r = clampBrightness(lroundf(rf * 255.0f));
    g = clampBrightness(lroundf(gf * 255.0f));
    b = clampBrightness(lroundf(bf * 255.0f));
  }

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
    showColor(
      clampBrightness(lroundf(baseR * scale)),
      clampBrightness(lroundf(baseG * scale)),
      clampBrightness(lroundf(baseB * scale))
    );
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
}

void StatusLED::setNetworkMode(StatusLED::Mode mode) {
  if (networkMode != mode) {
    blinkOn = false;
    lastBlinkToggle = 0;
  }
  networkMode = mode;
}

void StatusLED::setProcessActive(bool active) {
  processActive = active;
}

void StatusLED::update() {
  if (!ledReady) {
    return;
  }

  unsigned long now = millis();

  if (processActive) {
    if (now - lastRainbowUpdate >= 30) {
      lastRainbowUpdate = now;
      rainbowHue += 0.0025f; // just under 4 sekunder for en fuld cyklus
      if (rainbowHue >= 1.0f) {
        rainbowHue -= 1.0f;
      }

      float pulse = (sinf(now / 500.0f) + 1.0f) * 0.5f; // 0..1
      float brightnessFactor = 0.25f + pulse * 0.75f;
      uint8_t value = clampBrightness(static_cast<int>(maxBrightness * brightnessFactor));

      uint8_t r, g, b;
      hsvToRgb(rainbowHue, 1.0f, 1.0f, r, g, b);
      showScaledColor(r, g, b, value);
    }
    return;
  }

  switch (networkMode) {
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
