#include "wigle_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>

#include "timekeeping.h"

namespace wigle {
namespace {

constexpr const char* STATS_URL = "https://api.wigle.net/api/v2/stats/user";
constexpr uint32_t HTTP_TIMEOUT_MS = 8000;

Stats lastGood;
String error;
String activeName;
String activeToken;
uint32_t epoch = 0;
bool loginKnown = false;       // the saved login, seen once at boot, is not a change
bool soon = false;             // true right after a new login is saved
bool loadedDay = false;
uint32_t lastSuccessDay = 0;   // yyyymmdd of the last check that worked
uint32_t holdDay = 0;          // yyyymmdd to skip after WiGLE says slow down
uint32_t lastAttempt = 0;
bool attemptWas429 = false;

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

int dayNumber(const struct tm& now) {
  return (now.tm_year + 1900) * 10000 + (now.tm_mon + 1) * 100 + now.tm_mday;
}

void rememberDay(uint32_t day) {
  Preferences prefs;
  prefs.begin("cydesk", false);
  prefs.putUInt("wiglday", day);
  if (day == 0 || !lastGood.ok) {
    prefs.remove("wigluser");
    prefs.remove("wiglrank");
    prefs.remove("wiglmrank");
    prefs.remove("wiglwifi");
    prefs.remove("wiglcell");
    prefs.remove("wiglbt");
    prefs.remove("wiglmon");
  } else {
    // The numbers live in flash so a reboot doesn't ask WiGLE again today.
    prefs.putString("wigluser", lastGood.username);
    prefs.putUInt("wiglrank", static_cast<uint32_t>(lastGood.rank));
    prefs.putUInt("wiglmrank", static_cast<uint32_t>(lastGood.monthRank));
    prefs.putUInt("wiglwifi", static_cast<uint32_t>(lastGood.wifi));
    prefs.putUInt("wiglcell", static_cast<uint32_t>(lastGood.cell));
    prefs.putUInt("wiglbt", static_cast<uint32_t>(lastGood.bt));
    prefs.putUInt("wiglmon", static_cast<uint32_t>(lastGood.monthCount));
  }
  prefs.end();
}

void loadDay() {
  if (loadedDay) return;
  loadedDay = true;
  Preferences prefs;
  prefs.begin("cydesk", true);
  lastSuccessDay = prefs.isKey("wiglday") ? prefs.getUInt("wiglday", 0) : 0;
  if (prefs.isKey("wigluser")) {
    lastGood.ok = true;
    lastGood.username = prefs.getString("wigluser", "");
    lastGood.rank = static_cast<int32_t>(prefs.getUInt("wiglrank", 0));
    lastGood.monthRank = static_cast<int32_t>(prefs.getUInt("wiglmrank", 0));
    lastGood.wifi = static_cast<int32_t>(prefs.getUInt("wiglwifi", 0));
    lastGood.cell = static_cast<int32_t>(prefs.getUInt("wiglcell", 0));
    lastGood.bt = static_cast<int32_t>(prefs.getUInt("wiglbt", 0));
    lastGood.monthCount = static_cast<int32_t>(prefs.getUInt("wiglmon", 0));
    epoch++;
  }
  prefs.end();
}

void applyLogin(const Settings& settings) {
  const bool same = settings.wigleName == activeName && settings.wigleToken == activeToken;
  if (loginKnown && same) return;
  const bool changed = loginKnown && !same;
  loginKnown = true;
  activeName = settings.wigleName;
  activeToken = settings.wigleToken;
  if (!changed) return;
  lastGood = Stats();
  error = "";
  holdDay = 0;
  // A new login should show numbers now, not wait until the chosen time.
  soon = activeName.length() > 0 && activeToken.length() > 0;
  if (soon) {
    lastSuccessDay = 0;
    rememberDay(0);
  }
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
  if (status == 429) {
    http.end();
    attemptWas429 = true;
    fail("WiGLE is limiting requests");
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
  loadDay();
  if (!activeName.length() || !activeToken.length() || WiFi.status() != WL_CONNECTED) return;
  // The daily time is local, so wait until the clock knows the day.
  if (!timekeeping::ready()) return;

  struct tm now;
  if (!timekeeping::localNow(now)) return;
  const uint32_t today = dayNumber(now);
  const int minuteNow = now.tm_hour * 60 + now.tm_min;
  const bool windowOpen = soon || minuteNow >= static_cast<int>(settings.wigleMinute);
  if (lastSuccessDay == today || holdDay == today || !windowOpen) return;
  if (!soon && lastAttempt != 0 && millis() - lastAttempt < RETRY_MS) return;

  soon = false;
  attemptWas429 = false;
  lastAttempt = millis();
  fetch(settings);
  if (lastGood.ok && error.isEmpty()) {
    lastSuccessDay = today;
    rememberDay(today);
  } else if (attemptWas429 || error == "Login not accepted" ||
             error == "That login doesn't look right") {
    // A limit or a bad login will not get better by asking again today.
    holdDay = today;
  }
}

uint32_t generation() { return epoch; }
const Stats& current() { return lastGood; }
bool stale() { return lastGood.ok && error.length() > 0; }
const String& lastError() { return error; }

bool waitsUntilSetTime(const Settings& settings) {
  // Yesterday's numbers stay on screen. This is only for a blank page.
  if (soon || lastGood.ok || error.length() || !timekeeping::ready()) return false;
  struct tm now;
  if (!timekeeping::localNow(now)) return false;
  const int minuteNow = now.tm_hour * 60 + now.tm_min;
  return minuteNow < static_cast<int>(settings.wigleMinute);
}

}  // namespace wigle
