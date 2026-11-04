// Talks to WeatherAPI.com (https://www.weatherapi.com), using the user's own
// free API key. Everything weather-related goes through this file, so
// switching to another weather service later means rewriting only this.
//
// Free plan (checked Nov 2026): 100K calls/month, 3-day hourly forecast,
// astronomy data. They ask free users to credit "WeatherAPI.com".
#pragma once
#include <Arduino.h>

#include "settings.h"

struct PlaceCheck {
  bool ok = false;
  String label;  // on success: "Chicago, Illinois"
  String error;  // on failure: short message for the screen
};

// Asks WeatherAPI for the current weather at settings.zip, just to confirm the
// key works and the ZIP is a real place. Needs Wi-Fi. Takes ~1–2 s.
PlaceCheck checkPlace(const Settings& settings);
