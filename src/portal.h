// The setup portal: the board starts its own Wi-Fi network, and a phone that
// joins it gets a form for Wi-Fi details and the user's settings.
//
// While the portal is open the rest of the app is paused. It closes when the
// form is saved, or by itself after PORTAL_TIMEOUT_S seconds.
#pragma once
#include <TFT_eSPI.h>

#include "settings.h"

constexpr const char* PORTAL_AP_NAME = "DeskBuddy-Setup";
constexpr int PORTAL_TIMEOUT_S = 300;  // 5 minutes

// Shows instructions on screen, runs the portal, saves anything submitted.
// `reason` is shown as the heading, e.g. "First-time setup".
// Returns true if Wi-Fi is connected afterwards.
bool runSetupPortal(TFT_eSPI& tft, Settings& settings, const char* reason);

// True if a Wi-Fi network and password have been saved before.
bool hasSavedWifi();
