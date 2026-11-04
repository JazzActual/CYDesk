// Keeps the clock right.
//
//  - The ESP32 has no battery-backed clock, so after every power-up it asks
//    internet time servers (NTP) for the exact UTC time.
//  - The local offset (e.g. UTC-5) comes from WeatherAPI with every weather
//    update, so daylight saving changes are picked up automatically.
//  - The last offset is remembered in flash, so the clock can show the right
//    time right after a reboot, before the first weather update.
#pragma once
#include <Arduino.h>
#include <time.h>

namespace timekeeping {

// Call once Wi-Fi is connected (safe to call again; only starts NTP once).
void begin();

// Set the local offset from UTC, in seconds (e.g. -18000 for UTC-5).
void setUtcOffset(int32_t seconds);

// True once we have both the real time (from NTP) and a local offset.
bool ready();

// Current local time. Only meaningful when ready() is true.
bool localNow(struct tm& out);

}  // namespace timekeeping
