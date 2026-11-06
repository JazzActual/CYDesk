// CYDesk
//
// Pages (tap anywhere for the next one):
//   1. Home      greeting, clock, date, weather icon, temperature, humidity
//   2. Hourly    next 12 hours: hour, icon, temperature, rain chance
//   3. Badge     WDGWars stats
//   4. WiGLE     rank and discovery counts
//   5. Wardrift  character level and wardrive counts
//   6. Stars     first GitHub repo, big count and daily line
//   7. Repos     the list of GitHub repos, once two or more are saved
//   8. Status    settings / Wi-Fi / weather diagnostics
//
// Setup happens in two parts:
//   1. No Wi-Fi saved -> the Wi-Fi portal opens by itself (CYDesk).
//      Holding a finger on the screen for 5 seconds reopens it.
//   2. On the home Wi-Fi, the board serves a settings page at
//      http://cydesk.local for name, ZIP, WeatherAPI key, WDGWars key,
//      WiGLE login, Wardrift username and password, GitHub repos, and units.

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

#include "badge_page.h"
#include "fonts.h"
#include "github_page.h"
#include "github_service.h"
#include "home_page.h"
#include "hourly_page.h"
#include "ota.h"
#include "pages.h"
#include "pins.h"
#include "portal.h"
#include "settings.h"
#include "settings_web.h"
#include "theme.h"
#include "timekeeping.h"
#include "touch.h"
#include "weather_service.h"
#include "wardrift_page.h"
#include "wardrift_service.h"
#include "wdgwars_service.h"
#include "wigle_page.h"
#include "wigle_service.h"
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
WiglePage wiglePage(settings);
WardriftPage wardriftPage(settings);
GithubPage githubPage(settings);
GithubListPage githubListPage(settings);
StatusPage statusPage(settings, weather::lastError());

void applyScreens() {
  for (uint8_t i = 0; i < ScreenCount && i < pages.count(); i++) {
    pages.setEnabled(i, settings.screenOn(i));
  }
  pages.settle();
}

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
  ota::stop();           // free the OTA listener while the portal owns Wi-Fi
  fonts::use(tft, fonts::Id::BuiltIn);  // the portal screen uses the built-in fonts
  const bool connected = runWifiPortal(tft, reason);
  // The portal turns power-save back on. Don't call prepareWifi() here: that
  // forces station mode and would drop a connection the portal just made.
  holdWifiPolicy();
  if (!connected && hasSavedWifi()) connectSavedWifi();
  tft.setRotation(SCREEN_ROTATION);
  pages.redraw();
  pages.keepPage();
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
  ota::begin();
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
    wigle::prepare(settings);
    wardrift::prepare(settings);
    github::prepare(settings);
    applyScreens();
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
    tft.fillRect(shownWidth, y, wanted - shownWidth, HOLD_BAR_HEIGHT, theme::accent());
  } else if (wanted == 0 && !touch.longPressFired()) {
    // Let go early: put the page back the way it was.
    pages.redraw();
  }
  shownWidth = wanted;
}

void setup() {
  ledsOff();
  Serial.begin(115200);
#if defined(BOARD_CYD40)
  Serial.println("\nCYDesk: 4.0 inch resistive (E32R40T)");
#elif defined(BOARD_CYD35)
  Serial.println("\nCYDesk: 3.5 inch resistive");
#elif defined(BOARD_NICEMCU28)
  Serial.println("\nCYDesk: 2.8 inch capacitive (NiceMCU-32S-DEV_2.8IPS)");
#else
  Serial.println("\nCYDesk: 2.8 inch resistive");
#endif

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  touch.begin(tft);
  ota::setProgressDisplay(&tft);

  pages.add(&homePage);
  pages.add(&hourlyPage);
  pages.add(&badgePage);
  pages.add(&wiglePage);
  pages.add(&wardriftPage);
  pages.add(&githubPage);
  pages.add(&githubListPage);
  pages.add(&statusPage);

  settings.load();
  applyScreens();
  prepareWifi();

  if (!hasSavedWifi()) {
    // SoftAP/portal held off until explicitly re-enabled. Keep the radio quiet
    // so a first-boot desk does not broadcast CYDesk and knock nearby devices
    // off their home channel.
    WiFi.mode(WIFI_OFF);
    Serial.println("[wifi] no saved network; SoftAP/portal disabled (radio off)");
    pages.redraw();
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
    case TouchEvent::Tap: {
      // On Home, a tap on the top-right stats strip jumps to that page.
      int16_t tx = 0, ty = 0;
      if (pages.currentIndex() == ScreenHome && touch.tapPoint(tx, ty)) {
        const int dest = homePage.tapTarget(tx, ty);
        if (dest >= 0) {
          pages.show(static_cast<size_t>(dest));
          break;
        }
      }
      pages.tap();
      break;
    }
    case TouchEvent::LongPress:
      // SoftAP/portal held off until explicitly re-enabled.
      Serial.println("[wifi] long-press setup ignored (SoftAP/portal disabled)");
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
  ota::handle();
  // One download at a time: weather, then WDGWars, then WiGLE, then Wardrift,
  // then one GitHub repo.
  if (networkReady) {
    const bool wantWeather = settings.screenOn(ScreenHome) || settings.screenOn(ScreenHourly) ||
                             settings.screenOn(ScreenStatus);
    if (wantWeather) weather::update(settings);
    if (weather::takeChanged()) pages.dataChanged();
    // Home's strip uses one chosen source; keep fetching it even if that
    // source's own page is switched off.
    const HomeStripSource strip = settings.stripSource();
    const bool homeOn = settings.screenOn(ScreenHome);
    if ((homeOn && strip == HomeStripSource::Wdg) || settings.screenOn(ScreenBadge)) {
      wdg::update(settings);
    }
    if ((homeOn && strip == HomeStripSource::Wigle) || settings.screenOn(ScreenWigle)) {
      wigle::update(settings);
    }
    if ((homeOn && strip == HomeStripSource::Wardrift) || settings.screenOn(ScreenWardrift)) {
      wardrift::update(settings);
    }
    if (settings.screenOn(ScreenStars) || settings.screenOn(ScreenRepos)) github::update(settings);
  } else if (weather::takeChanged()) {
    pages.dataChanged();
  }
  updateHoldBar();
  pages.update();
}
