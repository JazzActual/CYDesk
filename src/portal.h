// The Wi-Fi setup portal: the board starts its own Wi-Fi network, and a phone
// that joins it gets a page for choosing the home Wi-Fi network and password.
//
// That's ALL it does. Everything else (name, ZIP, API key, WDGWars key, units) is
// set on the settings page at http://cydesk.local once the board is on the
// home network (see settings_web.h). That way the phone keeps its internet
// connection and you can switch apps to copy/paste things.
//
// While the portal is open the rest of the app is paused. It closes once the
// board has joined a network, or by itself after PORTAL_TIMEOUT_S seconds.
#pragma once
#include <TFT_eSPI.h>

constexpr const char* PORTAL_AP_NAME = "DeskBuddy-Setup";
constexpr int PORTAL_TIMEOUT_S = 300;  // 5 minutes

// Shows instructions on screen and runs the portal.
// `reason` is shown as the heading, e.g. "Wi-Fi setup".
// Returns true if Wi-Fi is connected afterwards.
bool runWifiPortal(TFT_eSPI& tft, const char* reason);

// True if a Wi-Fi network and password have been saved before.
bool hasSavedWifi();
