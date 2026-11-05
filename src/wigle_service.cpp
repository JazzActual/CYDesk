#include "wigle_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>

namespace wigle {
namespace {

constexpr const char* STATS_URL = "https://api.wigle.net/api/v2/stats/user";
constexpr uint32_t HTTP_TIMEOUT_MS = 8000;

Stats lastGood;
String error;
String activeName;
String activeToken;
uint32_t epoch = 0;
bool soon = true;
bool fetchedOnce = false;
uint32_t lastAttempt = 0;

// Basic auth is "name:token". A colon, space, or line break would break the
// header, so those never go out on the wire.
bool loginChar(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  return u >= 33 && u <= 126 && c != ':' && c != '"' && c != '\\';
}

bool loginLooksSafe(const String& name, const String& token) {
  if (name.length() < 1 || name.length() > 48) return false;
  if (token.length() < 8 || token.length() > 80) return false;
  for (unsigned i = 0; i < name.length(); i++) {
    if (!loginChar(name[i])) return false;
  }
  for (unsigned i = 0; i < token.length(); i++) {
    if (!loginChar(token[i])) return false;
  }
  return true;
}

void fail(const char* why) {
  Serial.printf("[wigle] failed: %s\n", why);
  if (error == why) return;
  error = why;
  epoch++;
}

void applyLogin(const Settings& settings) {
  if (settings.wigleName == activeName && settings.wigleToken == activeToken) return;
  activeName = settings.wigleName;
  activeToken = settings.wigleToken;
  lastGood = Stats();
  error = "";
  soon = activeName.length() > 0 && activeToken.length() > 0;
  epoch++;
}

bool basicHeader(const String& name, const String& token, String& header) {
  const String raw = name + ":" + token;
  unsigned char out[200];
  size_t olen = 0;
  const int rc = mbedtls_base64_encode(out, sizeof(out) - 1, &olen,
                                        reinterpret_cast<const unsigned char*>(raw.c_str()),
                                        raw.length());
  if (rc != 0 || olen == 0 || olen >= sizeof(out)) return false;
  out[olen] = 0;
  header = "Basic ";
  header += reinterpret_cast<char*>(out);
  return true;
}

void fetch(const Settings& settings) {
  Serial.printf("[wigle] fetching (heap %u, max block %u)\n", ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());
  if (!loginLooksSafe(settings.wigleName, settings.wigleToken)) {
    fail("That login doesn't look right");
    return;
  }
  if (ESP.getMaxAllocHeap() < 40 * 1024) {
    fail("Not enough memory");
    return;
  }

  String header;
  if (!basicHeader(settings.wigleName, settings.wigleToken, header)) {
    fail("Couldn't load WiGLE");
    return;
  }

  WiFiClientSecure tls;
  tls.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.useHTTP10(true);
  if (!http.begin(tls, STATS_URL)) {
    fail("Couldn't load WiGLE");
    return;
  }
  http.addHeader("Authorization", header);
  http.addHeader("Accept", "application/json");

  const int status = http.GET();
  if (status <= 0) {
    Serial.printf("[wigle] request failed: %s\n", http.errorToString(status).c_str());
    http.end();
    fail("Couldn't load WiGLE");
    return;
  }
  if (status == 401) {
    http.end();
    fail("Login not accepted");
    return;
  }
  if (status != 200) {
    Serial.printf("[wigle] HTTP %d\n", status);
    http.end();
    fail("Couldn't load WiGLE");
    return;
  }

  JsonDocument filter;
  filter["success"] = true;
  filter["user"] = true;
  filter["rank"] = true;
  filter["monthRank"] = true;
  filter["statistics"]["discoveredWiFi"] = true;
  filter["statistics"]["discoveredCell"] = true;
  filter["statistics"]["discoveredBt"] = true;
  filter["statistics"]["eventMonthCount"] = true;

  JsonDocument doc;
  const DeserializationError parseError =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (parseError || !doc.is<JsonObject>()) {
    Serial.printf("[wigle] unreadable reply (%s)\n", parseError.c_str());
    fail("Couldn't load WiGLE");
    return;
  }
  if (doc["success"].is<bool>() && doc["success"] == false) {
    fail("Login not accepted");
    return;
  }

  Stats fresh;
  fresh.ok = true;
  fresh.username = doc["user"] | "";
  fresh.rank = doc["rank"] | 0;
  fresh.monthRank = doc["monthRank"] | 0;
  fresh.wifi = doc["statistics"]["discoveredWiFi"] | 0;
  fresh.cell = doc["statistics"]["discoveredCell"] | 0;
  fresh.bt = doc["statistics"]["discoveredBt"] | 0;
  fresh.monthCount = doc["statistics"]["eventMonthCount"] | 0;

  lastGood = fresh;
  error = "";
  epoch++;
  Serial.printf("[wigle] ok: rank %ld month %ld wifi %ld cell %ld bt %ld monthCount %ld\n",
                static_cast<long>(fresh.rank), static_cast<long>(fresh.monthRank),
                static_cast<long>(fresh.wifi), static_cast<long>(fresh.cell),
                static_cast<long>(fresh.bt), static_cast<long>(fresh.monthCount));
}

}  // namespace

void prepare(const Settings& settings) { applyLogin(settings); }

void update(const Settings& settings) {
  applyLogin(settings);
  if (!activeName.length() || !activeToken.length() || WiFi.status() != WL_CONNECTED) return;

  const uint32_t wait = error.length() ? RETRY_MS : REFRESH_MS;
  if (!soon && fetchedOnce && millis() - lastAttempt < wait) return;

  soon = false;
  fetchedOnce = true;
  lastAttempt = millis();
  fetch(settings);
}

uint32_t generation() { return epoch; }
const Stats& current() { return lastGood; }
bool stale() { return lastGood.ok && error.length() > 0; }
const String& lastError() { return error; }

}  // namespace wigle
