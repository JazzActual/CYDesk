#include "weather_api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>

namespace {
constexpr const char* API_BASE = "https://api.weatherapi.com/v1";
constexpr uint32_t HTTP_TIMEOUT_MS = 8000;

// WeatherAPI's error codes (from their docs) turned into short screen text.
String describeApiError(int code, const String& message) {
  switch (code) {
    case 1002: return "No API key saved";
    case 2006: return "API key not accepted";
    case 2007: return "Monthly call limit reached";
    case 2008: return "API key disabled";
    case 2009: return "Key's plan can't use this";
    case 1006: return "ZIP code not found";
    default:   return message.length() ? message : String("Weather error ") + code;
  }
}
// Days since 1970-01-01 for a calendar date (the standard "days from civil"
// algorithm). Lets us turn "2026-11-04 12:30" into seconds without needing
// any timezone settings.
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

// WeatherAPI gives the local time as text ("2026-11-04 12:30") plus the same
// moment as a UTC timestamp. The difference is the local UTC offset, which
// already includes daylight saving time.
bool computeUtcOffset(const char* localText, int64_t utcEpoch, int32_t& offsetOut) {
  int y, mo, d, h, mi;
  if (!localText || sscanf(localText, "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5) return false;
  const int64_t localAsIfUtc = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60;
  const int64_t diff = localAsIfUtc - utcEpoch;
  // The text has no seconds, so round to the nearest 15 minutes (every real
  // timezone is a multiple of 15 min).
  offsetOut = static_cast<int32_t>(((diff + (diff >= 0 ? 450 : -450)) / 900) * 900);
  return true;
}

// Turns a forecast date ("2026-11-04") and a sunrise/sunset time ("07:12 AM",
// local time) into a UTC timestamp. Returns 0 for anything else, e.g.
// "No sunrise" near the poles.
time_t parseSunTime(const char* date, const char* clock, int32_t utcOffset) {
  int y, mo, d, h, mi;
  char ampm[3] = {0};
  if (!date || !clock || sscanf(date, "%d-%d-%d", &y, &mo, &d) != 3) return 0;
  if (sscanf(clock, "%d:%d %2s", &h, &mi, ampm) != 3) return 0;
  if (h == 12) h = 0;                           // 12:xx AM is just after midnight
  if (ampm[0] == 'P' || ampm[0] == 'p') h += 12;  // PM
  const int64_t local = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60;
  return static_cast<time_t>(local - utcOffset);
}

// WeatherAPI names one of eight phases and says what percent of the disc is
// lit. The icon wants one number around the cycle (0 new, 0.25 first quarter,
// 0.5 full, 0.75 last quarter). Percent lit can't tell waxing from waning —
// both quarters are half lit — so the name picks the half, and the percent
// places the moon inside it. Returns NAN for a name we don't recognise.
float moonPhaseFromAstro(const char* name, int illumination) {
  struct Named {
    const char* name;
    bool waxing;      // lit area growing; false = shrinking
    float midpoint;   // used only when the percent is missing
  };
  const Named known[] = {
      {"New Moon", true, 0.0f},
      {"Waxing Crescent", true, 0.125f},
      {"First Quarter", true, 0.25f},
      {"Waxing Gibbous", true, 0.375f},
      {"Full Moon", false, 0.5f},
      {"Waning Gibbous", false, 0.625f},
      {"Last Quarter", false, 0.75f},
      {"Waning Crescent", false, 0.875f},
  };
  const Named* match = nullptr;
  for (const Named& phase : known) {
    if (name && strcmp(name, phase.name) == 0) {
      match = &phase;
      break;
    }
  }
  if (!match) return NAN;
  if (illumination < 0 || illumination > 100) return match->midpoint;

  // Undo the icon's lighting: lit fraction = (1 - cos(2π · phase)) / 2
  // on the waxing half. 0% lands on new moon, 100% on full moon.
  float cosine = 1.0f - 2.0f * (illumination / 100.0f);
  if (cosine > 1.0f) cosine = 1.0f;
  if (cosine < -1.0f) cosine = -1.0f;
  const float waxingPhase = acosf(cosine) / (2.0f * PI);
  return match->waxing ? waxingPhase : 1.0f - waxingPhase;
}
}  // namespace

int WeatherReport::precipChanceAt(time_t when) const {
  // Clock not synced yet: the first forecast hour is the current one.
  if (when < 1700000000) return hourCount > 0 ? hours[0].precipChance : -1;
  for (int i = 0; i < hourCount; i++) {
    if (when >= hours[i].epoch && when < hours[i].epoch + 3600) return hours[i].precipChance;
  }
  return -1;
}

WeatherReport fetchWeather(const Settings& settings) {
  WeatherReport result;
  if (WiFi.status() != WL_CONNECTED) {
    result.error = "No Wi-Fi";
    return result;
  }
  // The secure connection needs one contiguous block of about 40 KB. Starting
  // it with less fails inside the library and still uses the last good weather.
  if (ESP.getMaxAllocHeap() < 40 * 1024) {
    result.error = "Not enough memory";
    Serial.printf("[weather] not enough memory (max block %u)\n", ESP.getMaxAllocHeap());
    return result;
  }

  // HTTPS without checking the server's certificate. Fine for a desk toy
  // (worst case someone on the network fakes the weather); pinning the
  // certificate is a possible later upgrade.
  WiFiClientSecure tls;
  tls.setInsecure();

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  // Ask for HTTP/1.0. With 1.1, WeatherAPI sends the reply "chunked" (with
  // little length markers mixed into the data), which the JSON reader below
  // can't handle when reading straight from the network stream.
  http.useHTTP10(true);
  // forecast.json with 2 days: current conditions, the place and its local
  // time, sunrise/sunset and today's moon phase, and 48 hourly forecasts.
  const String url = String(API_BASE) + "/forecast.json?key=" + settings.weatherKey +
                     "&q=" + settings.zip + "&days=2&aqi=no&alerts=no";
  if (!http.begin(tls, url)) {
    result.error = "Couldn't start request";
    return result;
  }

  const int status = http.GET();
  if (status <= 0) {
    // Negative numbers are connection problems (DNS, timeout, TLS...).
    result.error = "Can't reach WeatherAPI";
    Serial.printf("[weather] request failed: %s\n", http.errorToString(status).c_str());
    http.end();
    return result;
  }

  // Only keep the fields we need; the rest of the reply is skipped while
  // reading, which saves a lot of memory.
  JsonDocument filter;
  filter["location"]["name"] = true;
  filter["location"]["region"] = true;
  filter["location"]["localtime"] = true;
  filter["location"]["localtime_epoch"] = true;
  filter["current"]["temp_f"] = true;
  filter["current"]["temp_c"] = true;
  filter["current"]["humidity"] = true;
  filter["current"]["is_day"] = true;
  filter["current"]["condition"]["code"] = true;
  filter["current"]["condition"]["text"] = true;
  // For arrays, the filter's first element applies to every element.
  JsonObject dayFilter = filter["forecast"]["forecastday"].add<JsonObject>();
  dayFilter["date"] = true;
  dayFilter["astro"]["sunrise"] = true;
  dayFilter["astro"]["sunset"] = true;
  dayFilter["astro"]["moon_phase"] = true;
  dayFilter["astro"]["moon_illumination"] = true;
  JsonObject hourFilter = dayFilter["hour"].add<JsonObject>();
  hourFilter["time_epoch"] = true;
  hourFilter["temp_f"] = true;
  hourFilter["temp_c"] = true;
  hourFilter["is_day"] = true;
  hourFilter["condition"]["code"] = true;
  hourFilter["chance_of_rain"] = true;
  hourFilter["chance_of_snow"] = true;
  filter["error"]["code"] = true;
  filter["error"]["message"] = true;

  JsonDocument doc;
  const DeserializationError parseError =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (parseError || !doc.is<JsonObject>()) {
    result.error = "Unreadable reply";
    Serial.printf("[weather] HTTP %d, unreadable reply (%s)\n", status, parseError.c_str());
    return result;
  }

  if (!doc["error"].isNull()) {
    const int code = doc["error"]["code"] | 0;
    result.error = describeApiError(code, doc["error"]["message"] | "");
    Serial.printf("[weather] HTTP %d, API error %d\n", status, code);
    return result;
  }

  const String name = doc["location"]["name"] | "";
  const String region = doc["location"]["region"] | "";
  if (name.isEmpty()) {
    result.error = "No place in reply";
    return result;
  }
  result.place = region.length() ? name + ", " + region : name;

  if (!computeUtcOffset(doc["location"]["localtime"].as<const char*>(),
                        doc["location"]["localtime_epoch"].as<long>(), result.utcOffsetSec)) {
    result.error = "No local time in reply";
    return result;
  }

  JsonObject current = doc["current"];
  result.tempF = current["temp_f"] | NAN;
  result.tempC = current["temp_c"] | NAN;
  result.humidity = current["humidity"] | -1;
  result.isDay = (current["is_day"] | 1) == 1;
  result.conditionCode = current["condition"]["code"] | 0;
  result.conditionText = current["condition"]["text"] | "";

  // The forecast: sunrise/sunset per day, and every hour from the current one on.
  const time_t now = doc["location"]["localtime_epoch"].as<long>();
  JsonArray days = doc["forecast"]["forecastday"];
  int dayIndex = 0;
  for (JsonObject day : days) {
    const char* date = day["date"];
    if (dayIndex < 2) {
      result.sunrise[dayIndex] = parseSunTime(date, day["astro"]["sunrise"], result.utcOffsetSec);
      result.sunset[dayIndex] = parseSunTime(date, day["astro"]["sunset"], result.utcOffsetSec);
    }
    // The moon barely moves in a day, so today's phase is enough for the icon.
    if (dayIndex == 0) {
      result.moonPhase = moonPhaseFromAstro(day["astro"]["moon_phase"].as<const char*>(),
                                             day["astro"]["moon_illumination"] | -1);
    }
    dayIndex++;
    for (JsonObject hour : day["hour"].as<JsonArray>()) {
      const time_t epoch = hour["time_epoch"].as<long>();
      if (epoch + 3600 <= now) continue;  // already over
      if (result.hourCount >= WeatherReport::MAX_HOURS) break;
      HourForecast& h = result.hours[result.hourCount++];
      h.epoch = epoch;
      h.tempF = hour["temp_f"] | NAN;
      h.tempC = hour["temp_c"] | NAN;
      h.isDay = (hour["is_day"] | 1) == 1;
      h.conditionCode = hour["condition"]["code"] | 0;
      h.precipChance = max(hour["chance_of_rain"] | 0, hour["chance_of_snow"] | 0);
    }
  }

  result.ok = true;
  return result;
}
