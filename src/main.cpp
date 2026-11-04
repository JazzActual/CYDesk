// CYDesk — Step 3: Wi-Fi portal + settings page + weather location check
//
// Setup happens in two parts:
//   1. No Wi-Fi saved -> the Wi-Fi portal opens by itself (DeskBuddy-Setup).
//      It only asks for the home Wi-Fi network and password.
//   2. On the home Wi-Fi, the board serves a settings page at
//      http://cydesk.local for name, ZIP, WeatherAPI key, badge and units.
//
// Tap anywhere          -> next page
// Hold for 5 seconds    -> Wi-Fi portal again (e.g. new router)
//
// Page 1 is a temporary "Settings check" page that shows what was saved,
// whether Wi-Fi is connected, and which place WeatherAPI found for the ZIP.

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

#include "pages.h"
#include "pins.h"
#include "portal.h"
#include "settings.h"
#include "settings_web.h"
#include "theme.h"
#include "touch.h"
#include "weather_api.h"

// Landscape. If everything comes out upside down, change this to 3.
constexpr uint8_t SCREEN_ROTATION = 1;

// Don't show the hold bar for ordinary taps, only once a press lasts this long.
constexpr uint32_t HOLD_BAR_DELAY_MS = 1000;
constexpr int HOLD_BAR_HEIGHT = 6;

// If checking the ZIP/key fails (e.g. no internet yet), try again this often.
constexpr uint32_t PLACE_RETRY_MS = 60 * 1000;

TFT_eSPI tft;
TouchInput touch;
PageManager pages(tft);
Settings settings;
String placeError;  // last reason the ZIP/key check failed, shown on screen

// Pages, in tap order. Home and Hourly are placeholders until steps 4–7.
StatusPage statusPage(settings, placeError);
PlaceholderPage hourlyPage("Hourly", theme::PINK, 2, 3);
PlaceholderPage badgePage("Badge", theme::TEXT, 3, 3);

void ledsOff() {
  for (int pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
}

// Runs the Wi-Fi portal, then gets back to normal: reconnect Wi-Fi if needed
// and redraw the current page.
void openWifiSetup(const char* reason) {
  settings_web::stop();  // the portal needs the web port for itself
  const bool connected = runWifiPortal(tft, reason);
  if (!connected && hasSavedWifi()) WiFi.begin();  // keep trying in the background
  pages.redraw();
}

// Starts the settings page the first time Wi-Fi is up, and reacts when the
// form is saved.
void serviceSettingsPage() {
  if (!settings_web::isRunning() && WiFi.status() == WL_CONNECTED) {
    settings_web::begin(settings, placeError);
  }
  settings_web::handle();
  if (settings_web::takeChanged()) {
    placeError = "";  // settings changed: check the ZIP/key again
    pages.redraw();
  }
}

// Once Wi-Fi is up, ask WeatherAPI whether the ZIP and key work and remember
// the place name. Only runs while the place is unknown; retries on failure.
void checkPlaceIfNeeded() {
  static uint32_t lastAttempt = 0;
  static bool attempted = false;

  if (settings.placeLabel.length() || !settings.isComplete()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (attempted && placeError.length() && millis() - lastAttempt < PLACE_RETRY_MS) return;

  attempted = true;
  lastAttempt = millis();
  Serial.println("[weather] checking ZIP + key...");
  const PlaceCheck check = checkPlace(settings);  // blocks ~1–2 s
  if (check.ok) {
    settings.placeLabel = check.label;
    settings.save();
    placeError = "";
    Serial.println("[weather] place found");
  } else {
    placeError = check.error;
    Serial.printf("[weather] check failed: %s\n", check.error.c_str());
  }
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
  Serial.println("\nCYDesk step 3: Wi-Fi portal + settings page");

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  touch.begin();

  pages.add(&statusPage);
  pages.add(&hourlyPage);
  pages.add(&badgePage);

  settings.load();
  WiFi.mode(WIFI_STA);  // "station" = a normal Wi-Fi client
  WiFi.setAutoReconnect(true);

  if (!hasSavedWifi()) {
    openWifiSetup("Wi-Fi setup");
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
      openWifiSetup("Wi-Fi setup");
      break;
    case TouchEvent::None:
      break;
  }

  serviceSettingsPage();
  checkPlaceIfNeeded();
  updateHoldBar();
  pages.update();
}
