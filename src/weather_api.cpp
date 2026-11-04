#include "weather_api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

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
}  // namespace

CurrentWeather fetchCurrent(const Settings& settings) {
  CurrentWeather result;
  if (WiFi.status() != WL_CONNECTED) {
    result.error = "No Wi-Fi";
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
  // current.json: right-now conditions plus the place and its local time.
  const String url = String(API_BASE) + "/current.json?key=" + settings.weatherKey +
                     "&q=" + settings.zip + "&aqi=no";
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
  result.ok = true;
  return result;
}
