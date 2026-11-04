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
}  // namespace

PlaceCheck checkPlace(const Settings& settings) {
  PlaceCheck result;
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
  // current.json is the smallest request that tells us the place name.
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
  filter["location"]["country"] = true;
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
  result.ok = true;
  result.label = region.length() ? name + ", " + region : name;
  return result;
}
