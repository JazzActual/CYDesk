// Keeps the weather up to date in the background: fetches every 15 minutes
// (retrying after 1 minute if a fetch fails) and remembers the last good
// result, so pages can draw instantly from memory.
//
// Each successful fetch also updates the clock's UTC offset and the place
// name shown on the settings page.
#pragma once
#include "settings.h"
#include "weather_api.h"

namespace weather {

constexpr uint32_t REFRESH_MS = 15 * 60 * 1000;  // ~3,000 calls/month: well within the free plan
constexpr uint32_t RETRY_MS = 60 * 1000;

// Call every loop(). Fetches when one is due (blocks ~1–2 s while it does).
void update(Settings& settings);

// Fetch as soon as possible (e.g. after the settings were changed).
void refreshSoon();

// True once after each fetch attempt (good or bad), so pages can redraw.
bool takeChanged();

// The last good weather (check .ok; false until the first success).
const CurrentWeather& current();

// Why the most recent fetch failed ("" if it worked).
const String& lastError();

}  // namespace weather
