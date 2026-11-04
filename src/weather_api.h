// Talks to WeatherAPI.com (https://www.weatherapi.com), using the user's own
// free API key. Everything weather-related goes through this file, so
// switching to another weather service later means rewriting only this.
//
// Free plan (checked Nov 2026): 100K calls/month, 3-day hourly forecast,
// astronomy data. They ask free users to credit "WeatherAPI.com".
#pragma once
#include <Arduino.h>

#include "settings.h"

struct CurrentWeather {
  bool ok = false;
  String error;          // on failure: short message for the screen

  String place;          // "Chicago, Illinois"
  int32_t utcOffsetSec = 0;  // local time minus UTC, e.g. -18000 for UTC-5

  float tempF = NAN, tempC = NAN;
  int humidity = -1;     // percent
  int conditionCode = 0; // WeatherAPI condition code (1000 = sunny/clear, ...)
  String conditionText;  // "Partly cloudy"
  bool isDay = true;     // WeatherAPI decides this from sunrise/sunset
};

// Fetches the current weather for settings.zip. Needs Wi-Fi. Takes ~1–2 s.
CurrentWeather fetchCurrent(const Settings& settings);
