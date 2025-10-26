#include "DisplayHandler.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "ProcessHandler.h"  // For getProcessSymbol()
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSerifBoldItalic9pt7b.h>
#include "BeerFrames.h"
#include "Version.h"
#include "PinConfig.h"

#ifndef OLED_RESET
#define OLED_RESET -1
#endif

static Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

namespace {
  unsigned long lastUpdate = 0;
  
  void drawText(const String &text, int16_t x, int16_t y, uint8_t size = 1) {
    display.setCursor(x, y);
    display.setTextSize(size);
    display.print(text);
  }
}

void DisplayHandler::begin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setFont(&FreeSans9pt7b);
  display.cp437(true);
  display.display();
}

void DisplayHandler::showMessage(const String &msg) {
  display.clearDisplay();
  drawText(msg, 0, 20);
  display.display();
}

/*
 * Update OLED-displayet med følgende layout:
 * - Linje 0: Venstre: processStep (fx "Mæskning", "Venter på kogepunkt" osv.)
 *          Højre: et status-symbol (f.eks. ">>", "||" eller "[ ]")
 * - Linje 1: Resterende tid i mm:ss-format.
 * - Linje 2: Grydetemperatur.
 * - Linje 3: Ventiltemperatur (hvis showVentil er true, ellers tomt).
 */
void DisplayHandler::update(float tGryde, float tVentil, bool showVentil, const String &processStep, unsigned long remainingTime) {
  unsigned long now = millis();
  if (now - lastUpdate < 500)
    return;
  lastUpdate = now;
  
  display.clearDisplay();
  display.setFont(); // Standardfonten (typisk 5x7)
  display.setTextSize(1);
  
  // Linje 0: ProcessStep og status-symbol
  drawText(processStep, 0, 0, 1);
  String symbol = ProcessHandler::getProcessSymbol();
  drawText(symbol, 100, 0, 1);
  
  // Linje 1: Resterende tid i mm:ss-format
  uint16_t mm = remainingTime / 60;
  uint16_t ss = remainingTime % 60;
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", mm, ss);
  display.setFont(&FreeSans9pt7b);
  drawText("Tid: " + String(timeBuf), 0, 27, 1);
  display.setFont(); // Standardfonten (typisk 5x7)
  
  // Linje 2: Grydetemperatur
  drawText("Gryde:", 0, 38, 1);
  drawText(String(tGryde, 1) + " C", 48, 38, 1);
  
  // Linje 3: Ventiltemperatur (vises kun, hvis showVentil er true)
  drawText("Ventil: ", 0, 48, 1);
  if (showVentil) {
    drawText(String(tVentil, 1) + " C", 48, 48, 1);
  } else {
    drawText("     ", 48, 48, 1);
  }
  
  display.display();
}

void drawCenteredText(const String &text, int16_t y, const GFXfont *font = NULL) {
  if (font != NULL) {
    display.setFont(font);
  } else {
    display.setFont(); // Sætter standardfonten
  }
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int16_t x = (display.width() - w) / 2;
  display.setCursor(x, y);
  display.print(text);
}


void DisplayHandler::displayBeerAnimation() {
  // Tegn animationen midt på skærmen
  // Beregn centreringskoordinater for animationen baseret på dine bitmap-dimensioner (beerFrameWidth og beerFrameHeight)
  int animX = (display.width() - beerFrameWidth) / 2;
  int animY = (display.height() - beerFrameHeight) / 2;
  
  const uint8_t frameCount = 23;      // Antal frames i animationen
  const uint16_t frameDelay = 100;     // Ventetid mellem frames i millisekunder

  // For hver frame skal vi tegne den på en hvide baggrund, så den sortte bitmap vises tydeligt.
  for (uint8_t i = 0; i < frameCount; i++) {
    // Hent pointeren til den aktuelle frame fra PROGMEM
    const unsigned char* frame = (const unsigned char*) pgm_read_ptr(&(beerFrames[i]));
    // Tegn bitmaptet med BLACK for de "aktive" pixels (så animationen vises sort på hvid baggrund)
    display.drawBitmap(animX, animY, frame, beerFrameWidth, beerFrameHeight, WHITE);
    display.display();
    delay(frameDelay);
  }

    // Fyld hele skærmen med WHITE
    display.fillScreen(WHITE);
    display.setTextColor(BLACK);
    display.setFont(&FreeSans9pt7b);
    drawCenteredText("STOUBY", 16, &FreeSans9pt7b);
    drawCenteredText("BRYGLAUG", 31, &FreeSans9pt7b);
    display.setFont(&FreeSerifBoldItalic9pt7b);
    drawCenteredText("brygstyring", 47, &FreeSerifBoldItalic9pt7b);
    display.setFont(&FreeSans9pt7b);
    drawCenteredText("Version: " + String(SOFTWARE_VERSION), 53);

    display.display();
    delay(2000); // Vent i 2 sekunder
    display.setFont(); // Standardfonten (typisk 5x7)
    display.setTextColor(WHITE);
}
