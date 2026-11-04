#include "weather_service.h"

#include <WiFi.h>

#include "timekeeping.h"

namespace weather {
namespace {
WeatherReport lastGood;
String error;
bool changed = false;
bool fetchedOnce = false;
bool soon = true;          // fetch at the first chance after boot
uint32_t lastAttempt = 0;
}  // namespace

void refreshSoon() {
  soon = true;
  error = "";
}

void update(Settings& settings) {
  if (!settings.isComplete() || WiFi.status() != WL_CONNECTED) return;

  const uint32_t wait = error.length() ? RETRY_MS : REFRESH_MS;
  if (!soon && fetchedOnce && millis() - lastAttempt < wait) return;

  soon = false;
  fetchedOnce = true;
  lastAttempt = millis();
  Serial.println("[weather] fetching...");

  WeatherReport fresh = fetchWeather(settings);
  changed = true;
  if (!fresh.ok) {
    error = fresh.error;
    Serial.printf("[weather] failed: %s\n", error.c_str());
    return;
  }

  error = "";
  lastGood = fresh;
  timekeeping::setUtcOffset(fresh.utcOffsetSec);
  if (settings.placeLabel != fresh.place) {
    settings.placeLabel = fresh.place;
    settings.save();
  }
  Serial.printf("[weather] ok: %.0fF, %d%% humidity, code %d, %s; %d forecast hours, "
                "precip now %d%%\n",
                fresh.tempF, fresh.humidity, fresh.conditionCode, fresh.isDay ? "day" : "night",
                fresh.hourCount, fresh.precipChanceAt(time(nullptr)));
  for (int d = 0; d < 2; d++) {
    // Printed as minutes past local midnight, so the log shows no dates or places.
    auto local = [&](time_t t) { return t ? (int)(((t + fresh.utcOffsetSec) % 86400) / 60) : -1; };
    Serial.printf("[weather] day %d: sunrise %02d:%02d, sunset %02d:%02d (local)\n", d,
                  local(fresh.sunrise[d]) / 60, local(fresh.sunrise[d]) % 60,
                  local(fresh.sunset[d]) / 60, local(fresh.sunset[d]) % 60);
  }
}

bool takeChanged() {
  const bool was = changed;
  changed = false;
  return was;
}

const WeatherReport& current() { return lastGood; }
const String& lastError() { return error; }

}  // namespace weather
