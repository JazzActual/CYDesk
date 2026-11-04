// Talks to WeatherAPI.com (https://www.weatherapi.com), using the user's own
// free API key. Everything weather-related goes through this file, so
// switching to another weather service later means rewriting only this.
//
// Free plan (checked Nov 2026): 100K calls/month, 3-day hourly forecast,
// astronomy data. They ask free users to credit "WeatherAPI.com".
#pragma once
#include <Arduino.h>
#include <time.h>

#include "settings.h"

// One hour of the forecast.
struct HourForecast {
  time_t epoch = 0;      // start of the hour (UTC timestamp)
  float tempF = NAN, tempC = NAN;
  int conditionCode = 0;
  bool isDay = true;
  int precipChance = 0;  // percent: the higher of chance of rain / chance of snow
};

struct WeatherReport {
  bool ok = false;
  String error;          // on failure: short message for the screen

  String place;          // "Chicago, Illinois"
  int32_t utcOffsetSec = 0;  // local time minus UTC, e.g. -18000 for UTC-5

  // Right now.
  float tempF = NAN, tempC = NAN;
  int humidity = -1;     // percent
  int conditionCode = 0; // WeatherAPI condition code (1000 = sunny/clear, ...)
  String conditionText;  // "Partly cloudy"
  bool isDay = true;     // WeatherAPI decides this from sunrise/sunset

  // Sunrise and sunset for today [0] and tomorrow [1], as UTC timestamps.
  // 0 = unknown (or no sunrise/sunset that day, near the poles).
  time_t sunrise[2] = {0, 0};
  time_t sunset[2] = {0, 0};

  // Today's moon, for the night icon. 0 = new, 0.25 = first quarter,
  // 0.5 = full, 0.75 = last quarter. NAN if the reply had no phase we know.
  float moonPhase = NAN;

  // The next hours, starting with the current one.
  static constexpr int MAX_HOURS = 48;
  HourForecast hours[MAX_HOURS];
  int hourCount = 0;

  // Chance of rain/snow (percent) for the hour containing `when`; -1 if unknown.
  int precipChanceAt(time_t when) const;
};

// Fetches current weather + a 2-day forecast for settings.zip. Needs Wi-Fi.
// Takes ~2–3 s (the forecast reply is large; only the needed parts are kept).
WeatherReport fetchWeather(const Settings& settings);
