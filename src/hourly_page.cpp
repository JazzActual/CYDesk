#include "hourly_page.h"

#include <math.h>

#include "fonts.h"
#include "theme.h"
#include "timekeeping.h"
#include "weather_icons.h"
#include "weather_service.h"

namespace {
constexpr int COLS = 6;
constexpr int TILES = 12;
constexpr int CREDIT_H = 18;  // room for the WeatherAPI credit along the bottom
constexpr int ICON = 40;

// First forecast hour that hasn't ended. The report starts at the current
// hour, but a fetch can be up to 15 minutes old, so skip any hour that is over.
int firstHour(const WeatherReport& w) {
  const time_t now = time(nullptr);
  int i = 0;
  if (now > 1700000000) {
    while (i < w.hourCount && w.hours[i].epoch + 3600 <= now) i++;
  }
  return i;
}

// "3 PM" or "15".
void formatHour(time_t epoch, bool use24h, char* buf, size_t n) {
  struct tm lt;
  localtime_r(&epoch, &lt);
  if (use24h) {
    snprintf(buf, n, "%02d", lt.tm_hour);
  } else {
    int h = lt.tm_hour % 12;
    if (h == 0) h = 12;
    snprintf(buf, n, "%d %s", h, lt.tm_hour < 12 ? "AM" : "PM");
  }
}
}  // namespace

uint32_t HourlyPage::signature() const {
  const WeatherReport& w = weather::current();
  if (!w.ok) return 1;
  uint32_t hash = 2166136261u;
  auto mix = [&](uint32_t v) {
    hash ^= v;
    hash *= 16777619u;
  };
  mix(settings_.fahrenheit);
  mix(settings_.clock24h);
  mix(isnan(w.moonPhase) ? 0 : (uint32_t)lroundf(w.moonPhase * 100.0f));
  const int start = firstHour(w);
  const int count = max(0, min(TILES, w.hourCount - start));
  mix(count);
  mix(count > 0 ? (uint32_t)w.hours[start].epoch : 0);
  for (int i = 0; i < count; i++) {
    const HourForecast& hour = w.hours[start + i];
    const float temp = settings_.fahrenheit ? hour.tempF : hour.tempC;
    mix(hour.conditionCode);
    mix(hour.isDay);
    mix(hour.precipChance);
    mix((uint32_t)lroundf(temp * 10.0f));
  }
  return hash == 0 ? 2 : hash;  // 0 means "nothing drawn yet"
}

void HourlyPage::update(TFT_eSPI& tft) {
  if (signature() != shown_) draw(tft);
}

void HourlyPage::draw(TFT_eSPI& tft) {
  const uint32_t started = millis();
  shown_ = signature();
  tft.fillScreen(theme::BG);

  const WeatherReport& w = weather::current();
  const int start = w.ok ? firstHour(w) : 0;
  const int count = w.ok ? max(0, min(TILES, w.hourCount - start)) : 0;
  if (count == 0) {
    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString("Waiting for weather", tft.width() / 2, tft.height() / 2);
    return;
  }

  const int colW = tft.width() / COLS;
  const int gridX = (tft.width() - colW * COLS) / 2;
  const int rowH = (tft.height() - CREDIT_H) / 2;
  const time_t now = time(nullptr);
  const float phase = (w.ok && !isnan(w.moonPhase)) ? w.moonPhase : 0.5f;

  for (int i = 0; i < count; i++) {
    const HourForecast& hour = w.hours[start + i];
    const int col = i % COLS;
    const int row = i / COLS;
    const int cx = gridX + col * colW + colW / 2;
    const int top = row * rowH;
    const bool current = timekeeping::ready() && now >= hour.epoch && now < hour.epoch + 3600;

    char label[8];
    formatHour(hour.epoch, settings_.clock24h, label, sizeof(label));
    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(current ? theme::CYAN : theme::TEXT, theme::BG);
    tft.drawString(label, cx, top + 4);

    drawWeatherIcon(tft, cx - ICON / 2, top + 22, iconForCode(hour.conditionCode), !hour.isDay,
                    phase, ICON);

    const float temp = settings_.fahrenheit ? hour.tempF : hour.tempC;
    char tempText[8];
    snprintf(tempText, sizeof(tempText), "%d°", (int)lroundf(temp));
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(tempText, cx, top + 64);

    char precip[8];
    snprintf(precip, sizeof(precip), "%d%%", hour.precipChance);
    fonts::use(tft, fonts::Id::Tiny);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(precip, cx, top + 82);
  }

  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("Weather by WeatherAPI.com", tft.width() / 2, tft.height() - 4);
  Serial.printf("[hourly] drew %d hours in %lu ms\n", count, millis() - started);
}