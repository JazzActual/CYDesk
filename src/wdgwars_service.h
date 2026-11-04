// WDGWars stats for the badge page.
//
// One read-only request, GET https://wdgwars.pl/api/me, with the key in a
// header. The board never uploads anything. The key stays in flash and is
// not printed. When a refresh fails, the last good numbers stay on screen.
#pragma once
#include "settings.h"

namespace wdg {

constexpr uint32_t REFRESH_MS = 15 * 60 * 1000;
constexpr uint32_t RETRY_MS = 60 * 1000;

struct Stats {
  bool ok = false;
  bool haveRecent = false;  // today / 7-day fields were in the reply
  String username;
  String gang;
  int32_t total = 0;
  int32_t wifi = 0;
  int32_t ble = 0;
  int32_t aircraft = 0;
  int32_t mesh = 0;
  int32_t today = 0;
  int32_t week = 0;
  int badges = 0;
};

// Call when settings are saved, before the screen redraws.
void prepare(const Settings& settings);

// Call every loop(), and not while another HTTPS download is running.
void update(const Settings& settings);

uint32_t generation();
const Stats& current();
bool stale();
const String& lastError();

}  // namespace wdg
