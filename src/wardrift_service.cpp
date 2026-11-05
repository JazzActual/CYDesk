#include "wardrift_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

namespace wardrift {
namespace {

constexpr const char* SIGNIN_URL = "https://wardrift.net/v1/auth/sign-in/username";
constexpr const char* DASHBOARD_URL = "https://wardrift.net/v1/dashboard";
constexpr uint32_t HTTP_TIMEOUT_MS = 8000;

Stats lastGood;
String error;
String activeUser;
String activePass;
String token;
time_t tokenExpiry = 0;
bool tokenLoaded = false;
bool loginKnown = false;
uint32_t epoch = 0;
bool soon = true;
bool fetchedOnce = false;
uint32_t lastAttempt = 0;

bool loginChar(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || c == '_' ||
         c == '.';
}

bool loginLooksSafe(const String& user, const String& pass) {
  if (user.length() < 3 || user.length() > 30) return false;
  if (pass.length() < 1 || pass.length() > 80) return false;
  for (unsigned i = 0; i < user.length(); i++) {
    if (!loginChar(user[i])) return false;
  }
  for (unsigned i = 0; i < pass.length(); i++) {
    const unsigned char c = static_cast<unsigned char>(pass[i]);
    if (c < 32 || c > 126) return false;
  }
  return true;
}

const char* factionName(int id) {
  switch (id) {
    case 1: return "Arcanum";
    case 2: return "Crusade";
    case 3: return "Technocracy";
    default: return "";
  }
}

void fail(const char* why) {
  Serial.printf("[wardrift] failed: %s\n", why);
  if (error == why) return;
  error = why;
  epoch++;
}

void loadToken() {
  if (tokenLoaded) return;
  tokenLoaded = true;
  Preferences prefs;
  prefs.begin("cydesk", true);
  token = prefs.isKey("wardtok") ? prefs.getString("wardtok", "") : "";
  tokenExpiry = prefs.isKey("wardexp") ? static_cast<time_t>(prefs.getUInt("wardexp", 0)) : 0;
  prefs.end();
}

void storeToken() {
  Preferences prefs;
  prefs.begin("cydesk", false);
  prefs.putString("wardtok", token);
  prefs.putUInt("wardexp", static_cast<uint32_t>(tokenExpiry));
  prefs.end();
}

void clearToken() {
  token = "";
  tokenExpiry = 0;
  Preferences prefs;
  prefs.begin("cydesk", false);
  prefs.remove("wardtok");
  prefs.remove("wardexp");
  prefs.end();
}

bool tokenFresh() {
  if (!token.length()) return false;
  const time_t now = time(nullptr);
  // No trustworthy clock yet: use the saved token and replace it if Wardrift says no.
  if (now < 1700000000) return true;
  return tokenExpiry == 0 || now + 3600 < tokenExpiry;
}

time_t parseExpiry(const char* iso) {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (!iso || sscanf(iso, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
    return 0;
  }
  // Days from civil date to Unix time, without changing the clock's timezone.
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int days = era * 146097 + static_cast<int>(doe) - 719468;
  return static_cast<time_t>(days) * 86400 + hour * 3600 + minute * 60 + second;
}

void applyLogin(const Settings& settings) {
  const bool same =
      settings.wardriftUser == activeUser && settings.wardriftPass == activePass;
  if (loginKnown && same) return;
  const bool changed = loginKnown && !same;
  loginKnown = true;
  activeUser = settings.wardriftUser;
  activePass = settings.wardriftPass;
  if (!changed) return;
  lastGood = Stats();
  error = "";
  clearToken();
  soon = activeUser.length() > 0 && activePass.length() > 0;
  epoch++;
}

bool signIn(const Settings& settings) {
  if (!loginLooksSafe(settings.wardriftUser, settings.wardriftPass)) {
    fail("That login doesn't look right");
    return false;
  }
  JsonDocument body;
  body["username"] = settings.wardriftUser;
  body["password"] = settings.wardriftPass;
  body["remember_me"] = true;
  String payload;
  serializeJson(body, payload);

  WiFiClientSecure tls;
  tls.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.useHTTP10(true);
  if (!http.begin(tls, SIGNIN_URL)) {
    fail("Couldn't load Wardrift");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  const int status = http.POST(payload);
  if (status == 401 || status == 403) {
    http.end();
    fail("Login not accepted");
    return false;
  }
  if (status != 200) {
    Serial.printf("[wardrift] sign-in HTTP %d\n", status);
    http.end();
    fail("Couldn't load Wardrift");
    return false;
  }

  JsonDocument filter;
  filter["session"]["token"] = true;
  filter["session"]["expires_at"] = true;
  JsonDocument doc;
  const DeserializationError parseError =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  const char* got = doc["session"]["token"] | "";
  if (parseError || got[0] == '\0') {
    fail("Login not accepted");
    return false;
  }
  token = got;
  tokenExpiry = parseExpiry(doc["session"]["expires_at"] | "");
  storeToken();
  error = "";
  return true;
}

void fetch(const Settings& settings) {
  Serial.printf("[wardrift] fetching (heap %u, max block %u)\n", ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());
  if (!tokenFresh()) {
    Serial.println("[wardrift] signing in");
    if (!signIn(settings)) return;
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
  if (!http.begin(tls, DASHBOARD_URL)) {
    fail("Couldn't load Wardrift");
    return;
  }
  http.addHeader("Authorization", "Bearer " + token);
  http.addHeader("Accept", "application/json");

  const int status = http.GET();
  if (status <= 0) {
    Serial.printf("[wardrift] request failed: %s\n", http.errorToString(status).c_str());
    http.end();
    fail("Couldn't load Wardrift");
    return;
  }
  if (status == 401) {
    http.end();
    clearToken();
    Serial.println("[wardrift] session rejected, signing in");
    if (!signIn(settings)) return;
    // One fresh try. A second 401 means the account itself was refused.
    WiFiClientSecure again;
    again.setInsecure();
    HTTPClient retry;
    retry.setTimeout(HTTP_TIMEOUT_MS);
    retry.useHTTP10(true);
    if (!retry.begin(again, DASHBOARD_URL)) {
      fail("Couldn't load Wardrift");
      return;
    }
    retry.addHeader("Authorization", "Bearer " + token);
    retry.addHeader("Accept", "application/json");
    const int retryStatus = retry.GET();
    if (retryStatus != 200) {
      Serial.printf("[wardrift] HTTP %d\n", retryStatus);
      retry.end();
      fail(retryStatus == 401 ? "Login not accepted" : "Couldn't load Wardrift");
      return;
    }
    // Fall through by reading this second reply. The first body is already closed.
    JsonDocument filter;
    filter["characters"][0]["name"] = true;
    filter["characters"][0]["level"] = true;
    filter["characters"][0]["currency"] = true;
    filter["characters"][0]["exp_to_next_level"] = true;
    filter["characters"][0]["faction_id"] = true;
    filter["characters"][0]["class_name"] = true;
    filter["wardrive"]["lifetime"]["routes_logged"] = true;
    filter["wardrive"]["lifetime"]["access_points_discovered"] = true;
    filter["wardrive"]["lifetime"]["threats_flagged"] = true;
    filter["wardrive"]["streak_days"] = true;
    JsonDocument doc;
    const DeserializationError parseError =
        deserializeJson(doc, retry.getStream(), DeserializationOption::Filter(filter),
                        DeserializationOption::NestingLimit(50));
    retry.end();
    if (parseError || !doc.is<JsonObject>() || doc["characters"][0].isNull()) {
      fail("Couldn't load Wardrift");
      return;
    }
    JsonObject character = doc["characters"][0];
    Stats fresh;
    fresh.ok = true;
    fresh.name = character["name"] | "";
    fresh.faction = factionName(character["faction_id"] | 0);
    fresh.className = character["class_name"] | "";
    fresh.level = character["level"] | 0;
    fresh.currency = character["currency"] | 0;
    fresh.expToNext = character["exp_to_next_level"] | 0;
    fresh.networks = doc["wardrive"]["lifetime"]["access_points_discovered"] | 0;
    fresh.routes = doc["wardrive"]["lifetime"]["routes_logged"] | 0;
    fresh.threats = doc["wardrive"]["lifetime"]["threats_flagged"] | 0;
    fresh.streakDays = doc["wardrive"]["streak_days"] | 0;
    lastGood = fresh;
    error = "";
    epoch++;
    Serial.printf("[wardrift] ok: level %ld faction %d networks %ld routes %ld threats %ld streak %ld\n",
                  static_cast<long>(fresh.level), character["faction_id"] | 0,
                  static_cast<long>(fresh.networks), static_cast<long>(fresh.routes),
                  static_cast<long>(fresh.threats), static_cast<long>(fresh.streakDays));
    return;
  }
  if (status != 200) {
    Serial.printf("[wardrift] HTTP %d\n", status);
    http.end();
    fail("Couldn't load Wardrift");
    return;
  }

  JsonDocument filter;
  filter["characters"][0]["name"] = true;
  filter["characters"][0]["level"] = true;
  filter["characters"][0]["currency"] = true;
  filter["characters"][0]["exp_to_next_level"] = true;
  filter["characters"][0]["faction_id"] = true;
  filter["characters"][0]["class_name"] = true;
  filter["wardrive"]["lifetime"]["routes_logged"] = true;
  filter["wardrive"]["lifetime"]["access_points_discovered"] = true;
  filter["wardrive"]["lifetime"]["threats_flagged"] = true;
  filter["wardrive"]["streak_days"] = true;

  JsonDocument doc;
  const DeserializationError parseError =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter),
                      DeserializationOption::NestingLimit(50));
  http.end();

  if (parseError || !doc.is<JsonObject>()) {
    Serial.printf("[wardrift] unreadable reply (%s)\n", parseError.c_str());
    fail("Couldn't load Wardrift");
    return;
  }
  JsonObject character = doc["characters"][0];
  if (character.isNull()) {
    fail("Couldn't load Wardrift");
    return;
  }

  Stats fresh;
  fresh.ok = true;
  fresh.name = character["name"] | "";
  fresh.faction = factionName(character["faction_id"] | 0);
  fresh.className = character["class_name"] | "";
  fresh.level = character["level"] | 0;
  fresh.currency = character["currency"] | 0;
  fresh.expToNext = character["exp_to_next_level"] | 0;
  fresh.networks = doc["wardrive"]["lifetime"]["access_points_discovered"] | 0;
  fresh.routes = doc["wardrive"]["lifetime"]["routes_logged"] | 0;
  fresh.threats = doc["wardrive"]["lifetime"]["threats_flagged"] | 0;
  fresh.streakDays = doc["wardrive"]["streak_days"] | 0;

  lastGood = fresh;
  error = "";
  epoch++;
  Serial.printf("[wardrift] ok: level %ld faction %d networks %ld routes %ld threats %ld streak %ld\n",
                static_cast<long>(fresh.level), character["faction_id"] | 0,
                static_cast<long>(fresh.networks), static_cast<long>(fresh.routes),
                static_cast<long>(fresh.threats), static_cast<long>(fresh.streakDays));
}

}  // namespace

void prepare(const Settings& settings) { applyLogin(settings); }

void update(const Settings& settings) {
  applyLogin(settings);
  loadToken();
  if (!activeUser.length() || !activePass.length() || WiFi.status() != WL_CONNECTED) return;

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

}  // namespace wardrift
