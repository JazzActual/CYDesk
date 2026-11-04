// CYDesk
//
// Pages (tap anywhere for the next one):
//   1. Home      greeting, clock, date, weather icon, temperature, humidity
//   2. Hourly    next 12 hours: hour, icon, temperature, rain chance
//   3. Badge     WDGWars stats
//   4. Status    settings / Wi-Fi / weather diagnostics
//
// Setup happens in two parts:
//   1. No Wi-Fi saved -> the Wi-Fi portal opens by itself (DeskBuddy-Setup).
//      Holding a finger on the screen for 5 seconds reopens it.
//   2. On the home Wi-Fi, the board serves a settings page at
//      http://cydesk.local for name, ZIP, WeatherAPI key, WDGWars key and units.

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

#include "badge_page.h"
#include "fonts.h"
#include "home_page.h"
#include "hourly_page.h"
#include "pages.h"
#include "pins.h"
#include "portal.h"
#include "settings.h"
#include "settings_web.h"
#include "theme.h"
#include "timekeeping.h"
#include "touch.h"
#include "weather_service.h"
#include "wdgwars_service.h"
#include "wifi_link.h"

// Landscape. If everything comes out upside down, change this to 3.
constexpr uint8_t SCREEN_ROTATION = 1;

// Don't show the hold bar for ordinary taps, only once a press lasts this long.
constexpr uint32_t HOLD_BAR_DELAY_MS = 1000;
constexpr int HOLD_BAR_HEIGHT = 6;

TFT_eSPI tft;
TouchInput touch;
PageManager pages(tft);
Settings settings;

// Pages, in tap order.
HomePage homePage(settings);
HourlyPage hourlyPage(settings);
BadgePage badgePage(settings);
StatusPage statusPage(settings, weather::lastError());

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
  fonts::use(tft, fonts::Id::BuiltIn);  // the portal screen uses the built-in fonts
  const bool connected = runWifiPortal(tft, reason);
  // The portal turns power-save back on. Don't call prepareWifi() here: that
  // forces station mode and would drop a connection the portal just made.
  holdWifiPolicy();
  if (!connected && hasSavedWifi()) connectSavedWifi();
  tft.setRotation(SCREEN_ROTATION);
  pages.redraw();
}

// Things that start once Wi-Fi is up: the settings page and the clock.
// Returns true once they're running (the weather waits for this, so the
// clock's saved offset is loaded before the first weather update).
bool startNetworkServices() {
  static bool started = false;
  if (!started && WiFi.status() != WL_CONNECTED) return false;
  if (!settings_web::isRunning() && WiFi.status() == WL_CONNECTED) {
    settings_web::begin(settings, weather::lastError());
  }
  timekeeping::begin();  // only does anything the first time
  started = true;
  return true;
}

// Answers the settings page and reacts when its form is saved.
void serviceSettingsPage() {
  settings_web::handle();
  if (settings_web::takeChanged()) {
    weather::refreshSoon();  // ZIP/key/units may have changed
    wdg::prepare(settings);   // drop the previous account before the redraw if the key changed
    pages.redraw();
  }
  if (settings_web::takeIconTestRequest()) {
    pages.show(0);  // the Home page
    homePage.startIconDemo();
  }
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
#if defined(BOARD_CYD35)
  Serial.println("\nCYDesk: 3.5 inch");
#else
  Serial.println("\nCYDesk: 2.8 inch");
#endif

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  touch.begin(tft);

  pages.add(&homePage);
  pages.add(&hourlyPage);
  pages.add(&badgePage);
  pages.add(&statusPage);

  settings.load();
  prepareWifi();

  if (!hasSavedWifi()) {
    openWifiSetup("Wi-Fi setup");
  } else {
    // Connect with the saved network in the background; the status page
    // updates itself once it's connected. Any mesh node with that name is fine.
    connectSavedWifi();
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

  maintainWifi();
  // A scan can leave the panel mirrored, and a reconnect redraws whatever
  // page is up. Put the rotation back and paint the page again once the
  // radio has been quiet for a moment.
  if (takeSettledWifiChange()) {
    tft.setRotation(SCREEN_ROTATION);
    pages.redraw();
  }
  const bool networkReady = startNetworkServices();
  serviceSettingsPage();
  // One HTTPS download at a time. Weather first, then the WDGWars stats.
  if (networkReady) weather::update(settings);
  if (weather::takeChanged()) pages.dataChanged();
  if (networkReady) wdg::update(settings);
  updateHoldBar();
  pages.update();
}
