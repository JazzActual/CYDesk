// WiGLE stats for the WiGLE page.
//
// One read-only request, GET https://api.wigle.net/api/v2/stats/user, with
// the API name and token as HTTP Basic auth. The board never uploads
// anything. Neither value is printed. When a refresh fails, the last good
// numbers stay on screen.
#pragma once
#include "settings.h"

namespace wigle {

constexpr uint32_t REFRESH_MS = 15 * 60 * 1000;
constexpr uint32_t RETRY_MS = 60 * 1000;

struct Stats {
  bool ok = false;
  String username;
  int32_t rank = 0;
  int32_t monthRank = 0;
  int32_t wifi = 0;
  int32_t cell = 0;
  int32_t bt = 0;
  int32_t monthCount = 0;
};

// Call when settings are saved, before the screen redraws.
void prepare(const Settings& settings);

// Call every loop(), and not while another HTTPS download is running.
void update(const Settings& settings);

uint32_t generation();
const Stats& current();
bool stale();
const String& lastError();

}  // namespace wigle
