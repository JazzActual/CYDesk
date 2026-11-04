// CYDesk — Step 2: touch + page switching
//
// Tap anywhere          -> next page (Home -> Hourly -> Badge -> Home ...)
// Hold for 5 seconds    -> "setup" (just a stub screen for now; the real
//                          Wi-Fi setup portal arrives in step 3)
//
// While you hold, a bar fills along the bottom so you can see the long
// press coming.

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "pages.h"
#include "pins.h"
#include "theme.h"
#include "touch.h"

// Landscape. If everything comes out upside down, change this to 3.
constexpr uint8_t SCREEN_ROTATION = 1;

// Don't show the hold bar for ordinary taps, only once a press lasts this long.
constexpr uint32_t HOLD_BAR_DELAY_MS = 1000;
constexpr int HOLD_BAR_HEIGHT = 6;

TFT_eSPI tft;
TouchInput touch;
PageManager pages(tft);

// Placeholders until the real pages exist.
PlaceholderPage homePage("Home", theme::CYAN, 1, 3);
PlaceholderPage hourlyPage("Hourly", theme::PINK, 2, 3);
PlaceholderPage badgePage("Badge", theme::TEXT, 3, 3);

void ledsOff() {
  for (int pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
}

// Step 3 replaces this with the real Wi-Fi setup portal.
void showSetupStub() {
  Serial.println("[setup] long press -> setup portal (stub)");
  tft.fillScreen(theme::BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(theme::CYAN, theme::BG);
  tft.drawString("Setup", tft.width() / 2, tft.height() / 2 - 14, 4);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("(portal comes in step 3)", tft.width() / 2, tft.height() / 2 + 16, 2);
}

// Draws the "keep holding..." progress bar along the bottom edge.
// Only redraws when the bar's width changes, to avoid flicker.
void updateHoldBar() {
  static int shownWidth = 0;  // how much of the bar is on screen right now

  int wanted = 0;
  if (touch.isDown() && !touch.longPressFired() && touch.heldMs() > HOLD_BAR_DELAY_MS) {
    const uint32_t progress = touch.heldMs() - HOLD_BAR_DELAY_MS;
    const uint32_t span = TouchInput::LONG_PRESS_MS - HOLD_BAR_DELAY_MS;
    wanted = min<uint32_t>(progress, span) * tft.width() / span;
  }

  if (wanted == shownWidth) return;
  const int y = tft.height() - HOLD_BAR_HEIGHT;
  if (wanted > shownWidth) {
    tft.fillRect(shownWidth, y, wanted - shownWidth, HOLD_BAR_HEIGHT, theme::CYAN);
  } else if (wanted == 0 && !touch.longPressFired()) {
    // Let go early: put the page back the way it was.
    pages.redraw();
  }
  shownWidth = wanted;
}

void setup() {
  ledsOff();
  Serial.begin(115200);
  Serial.println("\nCYDesk step 2: touch + pages");

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  touch.begin();

  pages.add(&homePage);
  pages.add(&hourlyPage);
  pages.add(&badgePage);
  pages.redraw();

  Serial.printf("Ready. Free heap: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  static bool inSetup = false;  // is the setup stub on screen?

  switch (touch.poll()) {
    case TouchEvent::Tap:
      if (inSetup) {
        inSetup = false;
        pages.redraw();  // back to the page we came from
      } else {
        pages.next();
      }
      break;
    case TouchEvent::LongPress:
      inSetup = true;
      showSetupStub();
      break;
    case TouchEvent::None:
      break;
  }

  updateHoldBar();
  pages.update();
}
