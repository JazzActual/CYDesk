#include "wdgwars_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace wdg {
namespace {

constexpr const char* ME_URL = "https://wdgwars.pl/api/me";
constexpr uint32_t HTTP_TIMEOUT_MS = 8000;

Stats lastGood;
String error;
String activeKey;
uint32_t epoch = 0;
bool soon = true;
bool fetchedOnce = false;
uint32_t lastAttempt = 0;

// The header is sent as-is, so the key has to be a single token. A pasted
// sentence or a line break must not go out on the wire.
bool keyLooksSafe(const String& key) {
  if (key.length() < 8 || key.length() > 80) return false;
  for (unsigned i = 0; i < key.length(); i++) {
    const unsigned char c = static_cast<unsigned char>(key[i]);
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '_';
    if (!ok) return false;
  }
  return true;
}

void fail(const char* why) {
  Serial.printf("[wdg] failed: %s\n", why);
  if (error == why) return;
  error = why;
  epoch++;
}

void applyKey(const Settings& settings) {
  if (settings.wdgKey == activeKey) return;
  activeKey = settings.wdgKey;
  lastGood = Stats();
  error = "";
  soon = activeKey.length() > 0;
  epoch++;
}

void fetch(const Settings& settings) {
  Serial.printf("[wdg] fetching (heap %u, max block %u)\n", ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());
  if (!keyLooksSafe(settings.wdgKey)) {
    fail("That key doesn't look right");
    return;
  }
  if (ESP.getMaxAllocHeap() < 40 * 1024) {
    fail("Not enough memory");
    return;
  }

  WiFiClientSecure tls;
  tls.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.useHTTP10(true);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(tls, ME_URL)) {
    fail("Couldn't load WDGWars");
    return;
  }
  http.addHeader("X-API-Key", settings.wdgKey);
  http.addHeader("Accept", "application/json");

  const int status = http.GET();
  if (status <= 0) {
    Serial.printf("[wdg] request failed: %s\n", http.errorToString(status).c_str());
    http.end();
    fail("Couldn't load WDGWars");
    return;
  }
  if (status == 401) {
    http.end();
    fail("Key not accepted");
    return;
  }
  if (status != 200) {
    Serial.printf("[wdg] HTTP %d\n", status);
    http.end();
    fail("Couldn't load WDGWars");
    return;
  }

  // Keep the counters and the badge list. Skip everything else in the reply.
  JsonDocument filter;
  filter["ok"] = true;
  filter["error"] = true;
  filter["username"] = true;
  filter["gang"] = true;
  filter["wifi"] = true;
  filter["ble"] = true;
  filter["aircraft"] = true;
  filter["mesh"] = true;
  filter["total"] = true;
  filter["recent_today"] = true;
  filter["recent_7d"] = true;
  filter["badges"] = true;

  JsonDocument doc;
  const DeserializationError parseError =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (parseError || !doc.is<JsonObject>()) {
    Serial.printf("[wdg] unreadable reply (%s)\n", parseError.c_str());
    fail("Couldn't load WDGWars");
    return;
  }
  // A missing "ok" still counts: the counters are the part we draw.
  // "ok": false is a rejected key.
  if (doc["ok"].is<bool>() && doc["ok"] == false) {
    fail("Key not accepted");
    return;
  }

  Stats fresh;
  fresh.ok = true;
  fresh.username = doc["username"] | "";
  fresh.gang = doc["gang"] | "";
  fresh.total = doc["total"] | 0;
  fresh.wifi = doc["wifi"] | 0;
  fresh.ble = doc["ble"] | 0;
  fresh.aircraft = doc["aircraft"] | 0;
  fresh.mesh = doc["mesh"] | 0;
  fresh.haveRecent = !doc["recent_today"].isNull() || !doc["recent_7d"].isNull();
  fresh.today = doc["recent_today"] | 0;
  fresh.week = doc["recent_7d"] | 0;
  fresh.badges = doc["badges"].is<JsonArray>() ? doc["badges"].size() : 0;

  lastGood = fresh;
  error = "";
  epoch++;
  Serial.printf("[wdg] ok: total %ld wifi %ld ble %ld air %ld mesh %ld badges %d\n",
                static_cast<long>(fresh.total), static_cast<long>(fresh.wifi),
                static_cast<long>(fresh.ble), static_cast<long>(fresh.aircraft),
                static_cast<long>(fresh.mesh), fresh.badges);
}

}  // namespace

void prepare(const Settings& settings) { applyKey(settings); }

void update(const Settings& settings) {
  applyKey(settings);
  if (!activeKey.length() || WiFi.status() != WL_CONNECTED) return;

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

}  // namespace wdg
