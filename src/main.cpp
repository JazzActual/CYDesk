// CYDesk — Step 3a: setup portal + Wi-Fi
//
// First boot (no settings / no Wi-Fi saved) -> setup portal opens by itself.
// Tap anywhere          -> next page
// Hold for 5 seconds    -> setup portal (change Wi-Fi, name, city, badge, units)
//
// Page 1 is a temporary "Settings check" page that shows what was saved and
// whether Wi-Fi is connected.

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

#include "pages.h"
#include "pins.h"
#include "portal.h"
#include "settings.h"
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
Settings settings;

// Pages, in tap order. Home and Hourly are placeholders until steps 4–7.
StatusPage statusPage(settings);
PlaceholderPage hourlyPage("Hourly", theme::PINK, 2, 3);
PlaceholderPage badgePage("Badge", theme::TEXT, 3, 3);

void ledsOff() {
  for (int pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
}

// Runs the portal, then gets back to normal: reconnect Wi-Fi if needed and
// redraw the current page.
void openSetup(const char* reason) {
  const bool connected = runSetupPortal(tft, settings, reason);
  if (!connected && hasSavedWifi()) WiFi.begin();  // keep trying in the background
  pages.redraw();
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
  Serial.println("\nCYDesk step 3a: setup portal + Wi-Fi");

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  touch.begin();

  pages.add(&statusPage);
  pages.add(&hourlyPage);
  pages.add(&badgePage);

  settings.load();
  WiFi.mode(WIFI_STA);  // "station" = a normal Wi-Fi client
  WiFi.setAutoReconnect(true);

  if (!settings.isComplete() || !hasSavedWifi()) {
    openSetup("First-time setup");
  } else {
    // Connect with the saved network in the background; the status page
    // updates itself once it's connected.
    WiFi.begin();
    pages.redraw();
  }

  Serial.printf("Ready. Free heap: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  switch (touch.poll()) {
    case TouchEvent::Tap:
      pages.next();
      break;
    case TouchEvent::LongPress:
      openSetup("Setup");
      break;
    case TouchEvent::None:
      break;
  }

  updateHoldBar();
  pages.update();
}
