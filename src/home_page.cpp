#include "home_page.h"

#include <math.h>
#include <WiFi.h>

#include "fonts.h"
#include "pet.h"
#include "theme.h"
#include "timekeeping.h"
#include "weather_icons.h"
#include "weather_service.h"

namespace {
// Layout (landscape, 320 x 240).
constexpr int MARGIN = 14;
constexpr int GREETING_Y = 12;      // top of the greeting (24 px font)
constexpr int DATE_Y = 44;          // top of the date (14 px font), under the greeting
constexpr int TIME_TOP = 62;        // top of the area cleared for the clock
constexpr int TIME_BASELINE = 110;  // clock digits and AM/PM sit on this line
// Sunrise/sunset above the weather block, inset from the right edge.
constexpr int SUN_COL_X[2] = {238, 288};  // column centres
// 3 px above the first try: the times were clipping the ° and the weather icon
// was chopping off the bottoms of the times.
constexpr int SUN_ICON_Y = 119;
constexpr int SUN_TIME_Y = 141;
constexpr int SUN_AREA_X = 210;           // left edge of the area cleared for them
constexpr uint16_t SUN_COLOUR = theme::rgb(255, 200, 61);

// Weather, bottom right: 72 px icon, temperature and humidity to its left.
constexpr int ICON_X = 240, ICON_Y = 158;
constexpr int WEATHER_TEXT_RIGHT = ICON_X - 8;  // right edge of temperature/humidity
constexpr int WEATHER_TEXT_LEFT = 100;          // left edge of the area they may use
constexpr int WEATHER_CLEAR_Y = 162;            // below the sun times; weather redraw starts here
constexpr int TEMP_BASELINE = 188;
constexpr int PRECIP_Y = 197;                   // top of the rain-chance line
constexpr int HUMIDITY_Y = 217;                 // top of the humidity line

// Phase for the icon: today's real moon, or a full moon if we don't have one yet.
float moonPhaseOf(const WeatherReport& w) {
  return (w.ok && !isnan(w.moonPhase)) ? w.moonPhase : 0.5f;
}

constexpr uint32_t DEMO_STEP_MS = 2000;
constexpr int DEMO_STEPS = static_cast<int>(IconKind::Count) * 2;  // every icon, day + night

const char* const DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                            "Thursday", "Friday", "Saturday"};
const char* const MONTHS[] = {"January", "February", "March", "April", "May", "June", "July",
                              "August", "September", "October", "November", "December"};

// "Morning, Jazz!" etc. Changes with the hour of the day.
String greetingFor(int hour, const String& name) {
  const bool named = name.length() > 0;
  if (hour >= 5 && hour < 12) return named ? "Morning, " + name + "!" : String("Good morning!");
  if (hour >= 12 && hour < 17) return named ? "Afternoon, " + name + "!" : String("Good afternoon!");
  if (hour >= 17 && hour < 22) return named ? "Evening, " + name + "!" : String("Good evening!");
  return named ? "Up late, " + name + "?" : String("Up late?");
}

// "7:19 AM" or "07:19", in local time.
String formatClock(time_t t, bool use24h) {
  struct tm lt;
  localtime_r(&t, &lt);
  char buf[12];
  if (use24h) {
    snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
  } else {
    int h = lt.tm_hour % 12;
    if (h == 0) h = 12;
    snprintf(buf, sizeof(buf), "%d:%02d %s", h, lt.tm_min, lt.tm_hour < 12 ? "AM" : "PM");
  }
  return buf;
}

// A half sun on a horizon line, with an up (sunrise) or down (sunset) arrow.
// (cx, top) = centre of the 26 x 18 icon's top edge.
void drawSunEventIcon(TFT_eSPI& tft, int cx, int top, bool rising) {
  const int x0 = cx - 13;
  const int sunX = x0 + 13, horizonY = top + 14;
  for (int a : {200, 240, 270, 300, 340}) {
    const float r = a * DEG_TO_RAD;
    tft.drawWideLine(sunX + cosf(r) * 8, horizonY - 1 + sinf(r) * 8, sunX + cosf(r) * 10.5f,
                     horizonY - 1 + sinf(r) * 10.5f, 1.6f, SUN_COLOUR, theme::BG);
  }
  tft.fillSmoothCircle(sunX, horizonY - 1, 6, SUN_COLOUR, theme::BG);
  tft.fillRect(x0, horizonY, 26, 4, theme::BG);  // cut the circle to a half sun
  tft.drawWideLine(x0 + 2, horizonY, x0 + 24, horizonY, 1.5f, theme::MUTED, theme::BG);
  const int ax = x0 + 23;
  if (rising) {
    tft.fillTriangle(ax, top + 2, ax - 3, top + 6, ax + 3, top + 6, theme::CYAN);
  } else {
    tft.fillTriangle(ax, top + 7, ax - 3, top + 3, ax + 3, top + 3, theme::PINK);
  }
}

// A small umbrella (12 x 12, top-left at x, y) for the chance-of-rain line.
// Stem is centred under the canopy; the hook curves left from the stem tip.
void drawUmbrella(TFT_eSPI& tft, int x, int y, uint16_t colour) {
  const int cx = x + 6;
  tft.fillSmoothCircle(cx, y + 5, 6, colour, theme::BG);
  tft.fillRect(x - 1, y + 6, 14, 7, theme::BG);  // keep only the top half (the canopy)
  tft.drawFastVLine(cx, y + 5, 7, colour);       // handle, centred on the canopy
  tft.drawFastHLine(cx - 2, y + 11, 3, colour);  // hook, attached to the stem
  tft.drawPixel(cx - 2, y + 10, colour);         // tip of the J
}

// The previous and next sun events around `now`, from today's and tomorrow's
// sunrise/sunset. Returns false if the forecast has none.
bool pickSunEvents(const WeatherReport& w, time_t now, time_t times[2], bool rising[2]) {
  // In time order: sunrise, sunset (today), sunrise, sunset (tomorrow).
  const time_t ev[4] = {w.sunrise[0], w.sunset[0], w.sunrise[1], w.sunset[1]};
  const bool up[4] = {true, false, true, false};
  int next = -1;
  for (int i = 0; i < 4; i++) {
    if (ev[i] && ev[i] > now) { next = i; break; }
  }
  if (next < 0) return false;
  times[1] = ev[next];
  rising[1] = up[next];
  if (next > 0 && ev[next - 1]) {
    times[0] = ev[next - 1];
    rising[0] = up[next - 1];
  } else {
    // Before today's sunrise: the previous event was yesterday's sunset, which
    // isn't in the forecast; today's sunset minus a day is within a minute or two.
    if (!w.sunset[0]) return false;
    times[0] = w.sunset[0] - 86400;
    rising[0] = false;
  }
  return true;
}

// Shortens text with "..." until it fits in maxWidth (using the loaded font).
String fitWidth(TFT_eSPI& tft, const String& text, int maxWidth) {
  if (tft.textWidth(text) <= maxWidth) return text;
  String fitted = text;
  while (fitted.length() > 1) {
    fitted.remove(fitted.length() - 1);
    if (tft.textWidth(fitted + "...") <= maxWidth) return fitted + "...";
  }
  return fitted;
}
}  // namespace

HomePage::Mode HomePage::currentMode() const {
  // Wi-Fi works but name/ZIP/key are missing: show the "Finish setup" QR code.
  if (WiFi.status() == WL_CONNECTED && !settings_.isComplete()) return Mode::FinishSetup;
  return Mode::Clock;
}

void HomePage::draw(TFT_eSPI& tft) {
  shownMode_ = currentMode();
  if (shownMode_ == Mode::FinishSetup) {
    drawFinishSetup(tft);
    return;
  }

  tft.fillScreen(theme::BG);

  // Force every part to be drawn fresh.
  shownGreeting_ = "";
  shownMinute_ = -1;
  shownWeather_ = "";
  shownSun_ = "";
  shownReady_ = timekeeping::ready();

  struct tm now;
  const int hour = (shownReady_ && timekeeping::localNow(now)) ? now.tm_hour : 12;
  drawGreeting(tft, hour);
  drawTime(tft);
  drawWeather(tft);
  drawSunEvents(tft);
  pet::draw(tft, settings_);
}

void HomePage::startIconDemo() {
  demoStep_ = 0;
  demoNextAt_ = millis() + DEMO_STEP_MS;
  shownWeather_ = "";  // force the weather to redraw
  shownMinute_ = -1;   // and the date row, which shows the icon name during the test
  Serial.println("[home] icon test started");
}

String HomePage::weatherSignature() const {
  if (demoStep_ >= 0) return "demo" + String(demoStep_);
  const WeatherReport& w = weather::current();
  if (!w.ok) return "none";
  const float temp = settings_.fahrenheit ? w.tempF : w.tempC;
  // Moon hundredths so a new phase redraws the icon on the next weather check.
  const int moon = isnan(w.moonPhase) ? -1 : lroundf(w.moonPhase * 100);
  return String(w.conditionCode) + (w.isDay ? "d" : "n") + lroundf(temp) +
         (settings_.fahrenheit ? "F" : "C") + w.humidity + "/" + w.precipChanceAt(time(nullptr)) +
         "m" + moon;
}

String HomePage::sunSignature() const {
  time_t t[2];
  bool up[2];
  if (!timekeeping::ready() || !pickSunEvents(weather::current(), time(nullptr), t, up)) return "none";
  return String((long)t[0]) + "/" + (long)t[1] + (settings_.clock24h ? "/24" : "/12");
}

void HomePage::drawSunEvents(TFT_eSPI& tft) {
  shownSun_ = sunSignature();
  tft.fillRect(SUN_AREA_X, SUN_ICON_Y - 2, tft.width() - SUN_AREA_X, SUN_TIME_Y + 20 - SUN_ICON_Y,
               theme::BG);
  time_t t[2];
  bool up[2];
  if (shownSun_ == "none" || !pickSunEvents(weather::current(), time(nullptr), t, up)) return;

  fonts::use(tft, fonts::Id::Small);
  tft.setTextDatum(TC_DATUM);
  for (int i = 0; i < 2; i++) {
    drawSunEventIcon(tft, SUN_COL_X[i], SUN_ICON_Y, up[i]);
    // The one that's already happened is greyed out; the next one is bright.
    tft.setTextColor(i == 0 ? theme::MUTED : theme::TEXT, theme::BG);
    tft.drawString(formatClock(t[i], settings_.clock24h), SUN_COL_X[i], SUN_TIME_Y);
  }
}

void HomePage::update(TFT_eSPI& tft) {
  if (currentMode() != shownMode_ || timekeeping::ready() != shownReady_) {
    draw(tft);
    return;
  }
  if (shownMode_ != Mode::Clock) return;

  // Icon test: move on every DEMO_STEP_MS, then go back to the real weather.
  if (demoStep_ >= 0 && (int32_t)(millis() - demoNextAt_) >= 0) {
    demoStep_++;
    demoNextAt_ = millis() + DEMO_STEP_MS;
    if (demoStep_ >= DEMO_STEPS) {
      demoStep_ = -1;
      Serial.println("[home] icon test finished");
    }
    shownMinute_ = -1;  // redraw the date row (icon name, or the date again)
  }
  if (weatherSignature() != shownWeather_) drawWeather(tft);
  if (sunSignature() != shownSun_) drawSunEvents(tft);

  struct tm now;
  const bool haveTime = shownReady_ && timekeeping::localNow(now);
  if (haveTime && now.tm_hour * 60 + now.tm_min != shownMinute_) drawTime(tft);
  if (!haveTime && shownMinute_ == -1) drawTime(tft);  // e.g. icon test step, before time sync
  if (haveTime) drawGreeting(tft, now.tm_hour);  // only redraws if the text changed
  pet::update(tft, settings_);
}

void HomePage::drawGreeting(TFT_eSPI& tft, int hour) {
  const String text = greetingFor(hour, settings_.name);
  if (text == shownGreeting_) return;
  shownGreeting_ = text;

  tft.fillRect(0, GREETING_Y - 2, tft.width(), 32, theme::BG);
  fonts::use(tft, fonts::Id::Medium);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::CYAN, theme::BG);
  tft.drawString(fitWidth(tft, text, tft.width() - 2 * MARGIN), MARGIN, GREETING_Y);
}

void HomePage::drawTime(TFT_eSPI& tft) {
  // Clear the date + clock area. Happens once a minute. Full width — sun
  // events sit below the clock now, above the weather block.
  tft.fillRect(0, DATE_Y - 2, tft.width(), TIME_TOP - DATE_Y + 2, theme::BG);
  tft.fillRect(0, TIME_TOP, tft.width(), TIME_BASELINE - TIME_TOP + 8, theme::BG);

  struct tm now;
  const bool haveTime = timekeeping::ready() && timekeeping::localNow(now);

  // Date sits above the clock (or the icon name during the icon test).
  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TL_DATUM);
  if (demoStep_ >= 0) {
    const IconKind kind = static_cast<IconKind>(demoStep_ / 2);
    tft.setTextColor(theme::PINK, theme::BG);
    tft.drawString(String("Icon test ") + (demoStep_ + 1) + "/" + DEMO_STEPS + ": " +
                       iconName(kind) + (demoStep_ % 2 ? ", night" : ", day"),
                   MARGIN, DATE_Y);
  } else if (haveTime) {
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(String(DAYS[now.tm_wday]) + ", " + MONTHS[now.tm_mon] + " " + now.tm_mday,
                   MARGIN, DATE_Y);
  } else {
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(WiFi.status() == WL_CONNECTED ? "Syncing time..." : "Waiting for Wi-Fi...",
                   MARGIN, DATE_Y);
  }

  if (!haveTime) {
    fonts::use(tft, fonts::Id::Clock);
    tft.setTextDatum(L_BASELINE);
    tft.setTextColor(theme::PANEL, theme::BG);
    tft.drawString("--:--", MARGIN, TIME_BASELINE);
    shownMinute_ = -2;  // "placeholder shown"
    return;
  }

  shownMinute_ = now.tm_hour * 60 + now.tm_min;

  char digits[6];
  if (settings_.clock24h) {
    // 24-hour clock: "09:05", "13:07".
    snprintf(digits, sizeof(digits), "%02d:%02d", now.tm_hour, now.tm_min);
  } else {
    // 12-hour clock: 0 -> 12 AM, 13 -> 1 PM.
    int hour12 = now.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(digits, sizeof(digits), "%d:%02d", hour12, now.tm_min);
  }

  // Digits and AM/PM share a baseline, so their bottoms line up.
  fonts::use(tft, fonts::Id::Clock);
  tft.setTextDatum(L_BASELINE);
  tft.setTextColor(theme::TEXT, theme::BG);
  const int digitsWidth = tft.drawString(digits, MARGIN, TIME_BASELINE);

  if (!settings_.clock24h) {
    fonts::use(tft, fonts::Id::Medium);
    tft.setTextColor(theme::CYAN, theme::BG);
    tft.drawString(now.tm_hour < 12 ? "AM" : "PM", MARGIN + digitsWidth + 6, TIME_BASELINE);
  }
}

void HomePage::drawWeather(TFT_eSPI& tft) {
  shownWeather_ = weatherSignature();
  // Clear the text area left of the icon (below the clock) and the icon itself.
  tft.fillRect(WEATHER_TEXT_LEFT, WEATHER_CLEAR_Y, ICON_X - WEATHER_TEXT_LEFT,
               tft.height() - WEATHER_CLEAR_Y, theme::BG);
  tft.fillRect(ICON_X, ICON_Y - 4, tft.width() - ICON_X, tft.height() - ICON_Y + 4, theme::BG);

  const WeatherReport& w = weather::current();
  if (demoStep_ >= 0) {
    drawWeatherIcon(tft, ICON_X, ICON_Y, static_cast<IconKind>(demoStep_ / 2),
                    demoStep_ % 2 == 1, moonPhaseOf(w));
  } else if (w.ok) {
    drawWeatherIcon(tft, ICON_X, ICON_Y, iconForCode(w.conditionCode), !w.isDay,
                    moonPhaseOf(w));
  }

  fonts::use(tft, fonts::Id::Temp);
  tft.setTextDatum(R_BASELINE);
  if (!w.ok) {
    // No weather yet: just a placeholder temperature.
    tft.setTextColor(theme::PANEL, theme::BG);
    tft.drawString("--°", WEATHER_TEXT_RIGHT, TEMP_BASELINE);
    return;
  }

  // Temperature, rounded, e.g. "64°".
  const float temp = settings_.fahrenheit ? w.tempF : w.tempC;
  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString(String(lroundf(temp)) + "°", WEATHER_TEXT_RIGHT, TEMP_BASELINE);

  // Chance of rain/snow this hour: a small pink umbrella, then e.g. "60%".
  const int precip = w.precipChanceAt(time(nullptr));
  if (precip >= 0) {
    fonts::use(tft, fonts::Id::Small);
    const String text = String(precip) + "%";
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(text, WEATHER_TEXT_RIGHT, PRECIP_Y);
    drawUmbrella(tft, WEATHER_TEXT_RIGHT - tft.textWidth(text) - 17, PRECIP_Y + 2, theme::PINK);
  }

  // Relative humidity, smaller and muted: e.g. "90%RH".
  if (w.humidity >= 0) {
    fonts::use(tft, fonts::Id::Tiny);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(String(w.humidity) + "%RH", WEATHER_TEXT_RIGHT, HUMIDITY_Y);
  }
}
