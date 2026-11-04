#include "timekeeping.h"

#include <Preferences.h>

namespace timekeeping {
namespace {
bool ntpStarted = false;
bool offsetKnown = false;
int32_t currentOffset = 0;

// Any time before this means NTP hasn't answered yet (the ESP32 starts at 1970).
constexpr time_t VALID_AFTER = 1700000000;  // Nov 2023

// The C library understands timezones as "POSIX TZ" strings. A fixed offset
// is written with the sign flipped: UTC-5 is "LOC+5", UTC+5:30 is "LOC-5:30".
// ("LOC" is just a placeholder name; it's never shown.)
void applyOffset(int32_t seconds) {
  const int32_t flipped = -seconds;
  const char sign = flipped < 0 ? '-' : '+';
  const int32_t absolute = abs(flipped);
  char tz[24];
  snprintf(tz, sizeof(tz), "LOC%c%d:%02d", sign, absolute / 3600, (absolute % 3600) / 60);
  setenv("TZ", tz, 1);
  tzset();
}

void saveOffset(int32_t seconds) {
  Preferences prefs;
  prefs.begin("cydesk", false);
  prefs.putInt("utcoff", seconds);
  prefs.end();
}

bool loadOffset(int32_t& seconds) {
  Preferences prefs;
  prefs.begin("cydesk", true);
  const bool found = prefs.isKey("utcoff");
  if (found) seconds = prefs.getInt("utcoff", 0);
  prefs.end();
  return found;
}
}  // namespace

void begin() {
  if (ntpStarted) return;
  ntpStarted = true;

  // Ask for plain UTC; our own offset is applied on top.
  configTzTime("UTC0", "pool.ntp.org", "time.google.com", "time.nist.gov");

  int32_t saved;
  if (!offsetKnown && loadOffset(saved)) {
    currentOffset = saved;
    offsetKnown = true;
  }
  if (offsetKnown) applyOffset(currentOffset);
}

void setUtcOffset(int32_t seconds) {
  const bool changed = !offsetKnown || seconds != currentOffset;
  currentOffset = seconds;
  offsetKnown = true;
  applyOffset(seconds);
  if (changed) {
    saveOffset(seconds);  // only write flash when it actually changes
    Serial.printf("[time] UTC offset now %+.2f h\n", seconds / 3600.0f);
  }
}

bool ready() { return offsetKnown && time(nullptr) > VALID_AFTER; }

bool localNow(struct tm& out) {
  const time_t now = time(nullptr);
  if (now <= VALID_AFTER) return false;
  localtime_r(&now, &out);
  return true;
}

}  // namespace timekeeping
