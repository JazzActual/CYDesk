#include "github_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "timekeeping.h"

#include <string.h>

namespace github {

bool slugChar(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || c == '-' ||
         c == '_' || c == '.';
}

bool slugOk(const String& slug) {
  const int slash = slug.indexOf('/');
  if (slash <= 0 || slash != slug.lastIndexOf('/') || slash >= slug.length() - 1) return false;
  if (slug.length() > 80) return false;
  for (unsigned i = 0; i < slug.length(); i++) {
    if (static_cast<int>(i) == slash) continue;
    if (!slugChar(slug[i])) return false;
  }
  return true;
}

namespace {

constexpr uint32_t HTTP_TIMEOUT_MS = 8000;
constexpr int MAX_DAYS = 21;
constexpr const char* NVS_NAMESPACE = "cydesk";
constexpr const char* HISTORY_KEY = "ghhist";

struct DayPoint {
  uint32_t day = 0;  // local yyyymmdd
  int32_t stars = 0;
};

Repo repos[MAX_REPOS];
DayPoint days[MAX_REPOS][MAX_DAYS];
uint8_t dayCount[MAX_REPOS] = {};
int repoCount = 0;
bool historyLoaded = false;
String rawList;
bool listKnown = false;
String error;
uint32_t epoch = 0;
int cursor = 0;
uint32_t holdUntil = 0;
bool busy = false;

void fail(const char* why) {
  Serial.printf("[github] failed: %s\n", why);
  if (error == why) return;
  error = why;
  epoch++;
}

int findSlug(const String& slug) {
  for (int i = 0; i < repoCount; i++) {
    if (repos[i].slug == slug) return i;
  }
  return -1;
}

void saveHistory() {
  uint8_t buf[1024];
  size_t used = 0;
  auto put = [&](const void* src, size_t n) -> bool {
    if (used + n > sizeof(buf)) return false;
    memcpy(buf + used, src, n);
    used += n;
    return true;
  };
  const uint8_t version = 1;
  const uint8_t count = static_cast<uint8_t>(repoCount);
  if (!put(&version, 1) || !put(&count, 1)) return;
  for (int i = 0; i < repoCount; i++) {
    const uint8_t slugLen = static_cast<uint8_t>(repos[i].slug.length());
    const uint8_t n = dayCount[i];
    if (!put(&slugLen, 1) || !put(repos[i].slug.c_str(), slugLen) || !put(&n, 1) ||
        !put(days[i], n * sizeof(DayPoint))) {
      return;
    }
  }
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBytes(HISTORY_KEY, buf, used);
  prefs.end();
}

void loadHistory() {
  if (historyLoaded) return;
  historyLoaded = true;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  if (!prefs.isKey(HISTORY_KEY)) {
    prefs.end();
    return;
  }
  uint8_t buf[1024];
  const size_t got = prefs.getBytes(HISTORY_KEY, buf, sizeof(buf));
  prefs.end();
  if (got < 2 || buf[0] != 1) return;
  size_t at = 1;
  const int count = buf[at++];
  if (count < 0 || count > MAX_REPOS) return;
  for (int i = 0; i < count; i++) {
    if (at >= got) return;
    const int slugLen = buf[at++];
    if (slugLen <= 0 || slugLen > 80 || at + slugLen + 1 > got) return;
    repos[i].slug = String(reinterpret_cast<char*>(buf + at), slugLen);
    at += slugLen;
    const int n = buf[at++];
    if (n < 0 || n > MAX_DAYS || at + n * sizeof(DayPoint) > got) return;
    memcpy(days[i], buf + at, n * sizeof(DayPoint));
    dayCount[i] = static_cast<uint8_t>(n);
    at += n * sizeof(DayPoint);
    repoCount = i + 1;
  }
}

uint32_t localDay() {
  struct tm now;
  if (!timekeeping::ready() || !timekeeping::localNow(now)) return 0;
  return (now.tm_year + 1900) * 10000u + (now.tm_mon + 1) * 100u + now.tm_mday;
}

// One point per local day. Later checks the same day update that point.
void noteStars(int index, int32_t stars) {
  const uint32_t day = localDay();
  if (day == 0 || index < 0 || index >= repoCount) return;
  DayPoint* row = days[index];
  uint8_t& n = dayCount[index];
  if (n > 0 && row[n - 1].day == day) {
    if (row[n - 1].stars == stars) return;
    row[n - 1].stars = stars;
    saveHistory();
    return;
  }
  if (n > 0 && row[n - 1].day > day) return;
  if (n == MAX_DAYS) {
    memmove(row, row + 1, (MAX_DAYS - 1) * sizeof(DayPoint));
    n = MAX_DAYS - 1;
  }
  row[n].day = day;
  row[n].stars = stars;
  n++;
  saveHistory();
  Serial.printf("[github] history %u days\n", n);
}

void applyList(const Settings& settings) {
  loadHistory();
  if (listKnown && settings.githubRepos == rawList) return;
  listKnown = true;
  rawList = settings.githubRepos;
  epoch++;

  Repo next[MAX_REPOS];
  DayPoint nextDays[MAX_REPOS][MAX_DAYS];
  uint8_t nextCount[MAX_REPOS] = {};
  int n = 0;
  String current;
  const String& raw = settings.githubRepos;
  for (unsigned i = 0; i <= raw.length() && n < MAX_REPOS; i++) {
    const char c = i == raw.length() ? '\n' : raw[i];
    if (c == '\n' || c == ',' || c == ';' || c == ' ') {
      current.trim();
      if (slugOk(current)) {
        const int old = findSlug(current);
        next[n] = old >= 0 ? repos[old] : Repo();
        next[n].slug = current;
        if (old >= 0 && dayCount[old] > 0) {
          nextCount[n] = dayCount[old];
          memcpy(nextDays[n], days[old], dayCount[old] * sizeof(DayPoint));
        }
        n++;
      }
      current = "";
    } else {
      current += c;
    }
  }
  for (int i = 0; i < n; i++) {
    repos[i] = next[i];
    dayCount[i] = nextCount[i];
    if (nextCount[i] > 0) memcpy(days[i], nextDays[i], nextCount[i] * sizeof(DayPoint));
  }
  for (int i = n; i < MAX_REPOS; i++) dayCount[i] = 0;
  repoCount = n;
  cursor = 0;
  error = "";
  saveHistory();
}

void fetchOne(int index) {
  Repo& repo = repos[index];
  Serial.printf("[github] fetching %s (heap %u)\n", repo.slug.c_str(), ESP.getFreeHeap());
  if (ESP.getMaxAllocHeap() < 40 * 1024) {
    fail("Not enough memory");
    repo.stale = repo.ok;
    return;
  }

  const int slash = repo.slug.indexOf('/');
  const String url = "https://api.github.com/repos/" + repo.slug.substring(0, slash) + "/" +
                     repo.slug.substring(slash + 1);

  WiFiClientSecure tls;
  tls.setInsecure();
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.useHTTP10(true);
  if (!http.begin(tls, url)) {
    fail("Couldn't load GitHub");
    repo.stale = repo.ok;
    return;
  }
  http.addHeader("User-Agent", "CYDesk");
  http.addHeader("Accept", "application/vnd.github+json");

  const int status = http.GET();
  if (status == 404) {
    http.end();
    repo.missing = true;
    repo.ok = false;
    repo.stale = false;
    error = "";
    epoch++;
    Serial.printf("[github] missing: %s\n", repo.slug.c_str());
    return;
  }
  if (status == 403 || status == 429) {
    http.end();
    holdUntil = millis() + REFRESH_MS;
    repo.stale = repo.ok;
    if (!repo.ok) fail("GitHub is limiting requests");
    else epoch++;
    Serial.printf("[github] HTTP %d\n", status);
    return;
  }
  if (status != 200) {
    Serial.printf("[github] HTTP %d\n", status);
    http.end();
    repo.stale = repo.ok;
    if (!repo.ok) fail("Couldn't load GitHub");
    else epoch++;
    return;
  }

  JsonDocument filter;
  filter["stargazers_count"] = true;
  JsonDocument doc;
  const DeserializationError parseError =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (parseError || !doc.is<JsonObject>() || doc["stargazers_count"].isNull()) {
    Serial.printf("[github] unreadable reply (%s)\n", parseError.c_str());
    repo.stale = repo.ok;
    if (!repo.ok) fail("Couldn't load GitHub");
    else epoch++;
    return;
  }

  repo.stars = doc["stargazers_count"] | 0;
  noteStars(index, repo.stars);
  repo.ok = true;
  repo.missing = false;
  repo.stale = false;
  error = "";
  epoch++;
  Serial.printf("[github] ok: %s stars %ld\n", repo.slug.c_str(), static_cast<long>(repo.stars));
}

}  // namespace

String normalizeRepos(const String& raw) {
  String out;
  int kept = 0;
  String current;
  for (unsigned i = 0; i <= raw.length() && kept < MAX_REPOS; i++) {
    const char c = i == raw.length() ? '\n' : raw[i];
    if (c == '\n' || c == ',' || c == ';' || c == ' ') {
      current.trim();
      if (slugOk(current)) {
        bool dup = false;
        int from = 0;
        while (from < out.length()) {
          const int nl = out.indexOf('\n', from);
          const String line = out.substring(from, nl < 0 ? out.length() : nl);
          if (line == current) dup = true;
          if (nl < 0) break;
          from = nl + 1;
        }
        if (!dup) {
          if (out.length()) out += '\n';
          out += current;
          kept++;
        }
      }
      current = "";
    } else {
      current += c;
    }
  }
  return out;
}

void prepare(const Settings& settings) { applyList(settings); }

void update(const Settings& settings) {
  applyList(settings);
  if (!repoCount || WiFi.status() != WL_CONNECTED || busy) return;
  if (holdUntil != 0 && static_cast<int32_t>(millis() - holdUntil) < 0) return;

  const uint32_t now = millis();
  int due = -1;
  for (int n = 0; n < repoCount; n++) {
    const int i = (cursor + n) % repoCount;
    const bool settled = (repos[i].ok || repos[i].missing) && !repos[i].stale;
    const uint32_t wait = settled ? REFRESH_MS : RETRY_MS;
    if (repos[i].lastAttempt == 0 || now - repos[i].lastAttempt >= wait) {
      due = i;
      break;
    }
  }
  if (due < 0) return;
  cursor = (due + 1) % repoCount;
  repos[due].lastAttempt = now;
  busy = true;
  fetchOne(due);
  busy = false;
}

uint32_t generation() { return epoch; }
int count() { return repoCount; }
const Repo& at(int index) { return repos[index]; }
bool loading() {
  if (!repoCount || error.length()) return false;
  for (int i = 0; i < repoCount; i++) {
    if (!repos[i].ok && !repos[i].missing) return true;
  }
  return false;
}
const String& lastError() { return error; }

int historyCount(int index) {
  if (index < 0 || index >= repoCount) return 0;
  return dayCount[index];
}

int32_t historyStars(int index, int point) {
  if (index < 0 || index >= repoCount || point < 0 || point >= dayCount[index]) return 0;
  return days[index][point].stars;
}

}  // namespace github
