// The settings page: a small web page served by the board itself on the home
// network, at http://cydesk.local (or its IP address). Open it on a phone or
// computer to set the name, ZIP, WeatherAPI key, WDGWars key, WiGLE login,
// Wardrift login, units, and the accent color.
//
// Because the phone stays on the normal home Wi-Fi, you can switch apps to
// copy/paste a key without losing anything.
//
// Note: anyone on the same home network can open this page. The API key is
// never sent back to the browser, only "saved" / "not set".
#pragma once
#include <Arduino.h>

#include "settings.h"

namespace settings_web {

constexpr const char* HOSTNAME = "cydesk";  // -> http://cydesk.local

// Start the web server and the "cydesk.local" name. Call once Wi-Fi is
// connected. `placeError` is shown on the page if the weather check failed.
void begin(Settings& settings, const String& placeError);

// Stop the server (needed before the Wi-Fi portal runs, which uses the same port).
void stop();

bool isRunning();

// Call every loop(): answers any waiting browser requests.
void handle();

// Returns true once after the form was saved, so the caller can react
// (re-check the weather location, redraw the screen).
bool takeChanged();

// Returns true once after someone opened http://cydesk.local/icons
// (starts the weather icon test on the Home page).
bool takeIconTestRequest();

}  // namespace settings_web
