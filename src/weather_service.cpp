#include "weather_service.h"

#include <WiFi.h>

#include "timekeeping.h"

namespace weather {
namespace {
CurrentWeather lastGood;
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

  CurrentWeather fresh = fetchCurrent(settings);
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
  Serial.printf("[weather] ok: %.0fF, %d%% humidity, code %d, %s\n", fresh.tempF, fresh.humidity,
                fresh.conditionCode, fresh.isDay ? "day" : "night");
}

bool takeChanged() {
  const bool was = changed;
  changed = false;
  return was;
}

const CurrentWeather& current() { return lastGood; }
const String& lastError() { return error; }

}  // namespace weather
