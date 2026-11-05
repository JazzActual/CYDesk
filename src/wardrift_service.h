// Wardrift stats for the Wardrift page.
//
// One read-only request, GET https://wardrift.net/v1/dashboard. Wardrift's
// API key is only for uploads, so the desk signs in with the saved username
// and password, keeps the session token, and sends it as a bearer token.
// The password and token are never printed. The board never uploads
// anything. When a refresh fails, the last good numbers stay on screen.
#pragma once
#include "settings.h"

namespace wardrift {

constexpr uint32_t REFRESH_MS = 15 * 60 * 1000;
constexpr uint32_t RETRY_MS = 60 * 1000;

struct Stats {
  bool ok = false;
  String name;
  String faction;  // Arcanum, Crusade, or Technocracy
  String className;
  int32_t level = 0;
  int32_t currency = 0;
  int32_t expToNext = 0;
  int32_t networks = 0;  // lifetime access points
  int32_t routes = 0;
  int32_t threats = 0;
  int32_t streakDays = 0;
};

// Call when settings are saved, before the screen redraws.
void prepare(const Settings& settings);

// Call every loop(), and not while another HTTPS download is running.
void update(const Settings& settings);

uint32_t generation();
const Stats& current();
bool stale();
const String& lastError();

}  // namespace wardrift
