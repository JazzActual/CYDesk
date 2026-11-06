#include "home_page.h"

#include <math.h>
#include <WiFi.h>

#include "fonts.h"
#include "home_strip.h"
#include "layout.h"
#include "pet.h"
#include "theme.h"
#include "timekeeping.h"
#include "wardrift_service.h"
#include "wdgwars_service.h"
#include "weather_icons.h"
#include "weather_service.h"
#include "wigle_service.h"

namespace {
// Top block is the same on every screen. The weather cluster is measured from
// the bottom-right, so a 320×240 screen keeps the approved positions and a
// 480×320 screen (the 3.5" board, landscape) uses the extra room.
constexpr int MARGIN = 14;
constexpr int GREETING_Y = 12;      // top of the greeting (24 px font)
constexpr int DATE_Y = 44;          // top of the date (14 px font), under the greeting
constexpr int TIME_TOP = 62;        // top of the area cleared for the clock
constexpr int TIME_BASELINE = 110;  // clock digits and AM/PM sit on this line
// Top-right stats strip (Tiny font). Same size on 2.8 / 3.5 / 4.0 — the
// first layout the owner preferred. Hidden when the source is Off.
constexpr int STRIP_ROW_H = 16;
constexpr uint16_t SUN_COLOUR = theme::rgb(255, 200, 61);

int statsStripWidth(TFT_eSPI& tft) { return layout::wide(tft) ? 118 : 100; }
int statsStripLeft(TFT_eSPI& tft) { return tft.width() - MARGIN - statsStripWidth(tft); }
int contentRight(TFT_eSPI& tft, HomeStripSource source) {
  if (source == HomeStripSource::Off) return tft.width() - MARGIN;
  return statsStripLeft(tft) - 8;
}

struct Place {
  int icon;
  int sunX[2];
  int sunIconY, sunTimeY, sunAreaX;
  int iconX, iconY;
  int textRight, textLeft, clearY;
  int tempBaseline, precipY, humidityY;
};

// On 320×240 these are the positions that were checked on the board:
// icon (240, 158), sun columns 238 and 288, times at y=141.
// On 480×320 the icon grows a little and stays bottom-right.
const Place& place(TFT_eSPI& tft) {
  static Place p;
  static int w = -1, h = -1;
  if (w == tft.width() && h == tft.height()) return p;
  w = tft.width();
  h = tft.height();
  p.icon = layout::homeWeatherIcon(tft);
  p.iconX = w - p.icon - 8;
  p.iconY = h - p.icon - 10;
  p.textRight = p.iconX - 8;
  p.textLeft = layout::wide(tft) ? 140 : 100;
  p.clearY = p.iconY + 4;
  p.tempBaseline = p.iconY + p.icon * 30 / 72;
  p.precipY = p.iconY + p.icon * 39 / 72;
  p.humidityY = p.iconY + p.icon * 59 / 72;
  p.sunX[1] = w - 32;
  p.sunX[0] = p.sunX[1] - 50;
  p.sunAreaX = w - 110;
  p.sunIconY = p.iconY - 39;
  p.sunTimeY = p.sunIconY + 22;
  return p;
}

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

// "" while the weather on screen is current. "offline" when Wi-Fi is down,
// "stale" when the last download failed and the previous numbers are still up.
const char* freshnessMark() {
  if (!weather::current().ok) return "";
  if (WiFi.status() != WL_CONNECTED) return "offline";
  if (weather::stale()) return "stale";
  return "";
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
    tft.fillTriangle(ax, top + 2, ax - 3, top + 6, ax + 3, top + 6, theme::accent());
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
  shownStrip_ = "";
  shownReady_ = timekeeping::ready();

  struct tm now;
  const int hour = (shownReady_ && timekeeping::localNow(now)) ? now.tm_hour : 12;
  drawGreeting(tft, hour);
  drawTime(tft);
  drawStatsStrip(tft);
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
  if (!w.ok) return String("none:") + weather::unavailableReason();
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
  const Place& p = place(tft);
  tft.fillRect(p.sunAreaX, p.sunIconY - 2, tft.width() - p.sunAreaX, p.sunTimeY + 20 - p.sunIconY,
               theme::BG);
  time_t t[2];
  bool up[2];
  if (shownSun_ == "none" || !pickSunEvents(weather::current(), time(nullptr), t, up)) return;

  fonts::use(tft, fonts::Id::Small);
  tft.setTextDatum(TC_DATUM);
  for (int i = 0; i < 2; i++) {
    drawSunEventIcon(tft, p.sunX[i], p.sunIconY, up[i]);
    // The one that's already happened is greyed out; the next one is bright.
    tft.setTextColor(i == 0 ? theme::MUTED : theme::TEXT, theme::BG);
    tft.drawString(formatClock(t[i], settings_.clock24h), p.sunX[i], p.sunTimeY);
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
  if (stripSignature() != shownStrip_) drawStatsStrip(tft);
  if (shownFreshness_ != (demoStep_ < 0 ? freshnessMark() : "")) drawTime(tft);

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

  // Leave the top-right strip alone; it redraws on its own signature.
  const int clearW = contentRight(tft, settings_.stripSource());
  tft.fillRect(0, GREETING_Y - 2, clearW, 32, theme::BG);
  fonts::use(tft, fonts::Id::Medium);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::accent(), theme::BG);
  tft.drawString(fitWidth(tft, text, clearW - MARGIN), MARGIN, GREETING_Y);
}

void HomePage::drawTime(TFT_eSPI& tft) {
  // Clear the date + clock area. Happens once a minute. Leave the stats
  // strip's column alone so a clock tick doesn't wipe the ranks.
  const int clearW = contentRight(tft, settings_.stripSource());
  tft.fillRect(0, DATE_Y - 2, clearW, TIME_TOP - DATE_Y + 2, theme::BG);
  tft.fillRect(0, TIME_TOP, clearW, TIME_BASELINE - TIME_TOP + 8, theme::BG);

  struct tm now;
  const bool haveTime = timekeeping::ready() && timekeeping::localNow(now);

  // Date sits above the clock (or the icon name during the icon test).
  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TL_DATUM);
  if (demoStep_ >= 0) {
    const IconKind kind = static_cast<IconKind>(demoStep_ / 2);
    tft.setTextColor(theme::PINK, theme::BG);
    tft.drawString(fitWidth(tft,
                            String("Icon test ") + (demoStep_ + 1) + "/" + DEMO_STEPS + ": " +
                                iconName(kind) + (demoStep_ % 2 ? ", night" : ", day"),
                            clearW - MARGIN),
                   MARGIN, DATE_Y);
  } else if (haveTime) {
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(fitWidth(tft,
                            String(DAYS[now.tm_wday]) + ", " + MONTHS[now.tm_mon] + " " + now.tm_mday,
                            clearW - MARGIN),
                   MARGIN, DATE_Y);
  } else {
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(WiFi.status() == WL_CONNECTED ? "Syncing time..." : "Waiting for Wi-Fi...",
                   MARGIN, DATE_Y);
  }

  // Freshness sits just left of the strip. Hidden during the icon test.
  const char* mark = demoStep_ < 0 ? freshnessMark() : "";
  shownFreshness_ = mark;
  if (mark[0] != '\0' && haveTime) {
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(mark, clearW, DATE_Y);
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
    tft.setTextColor(theme::accent(), theme::BG);
    tft.drawString(now.tm_hour < 12 ? "AM" : "PM", MARGIN + digitsWidth + 6, TIME_BASELINE);
  }
}

void HomePage::drawWeather(TFT_eSPI& tft) {
  shownWeather_ = weatherSignature();
  const Place& p = place(tft);
  // Clear the text area left of the icon (below the clock) and the icon itself.
  tft.fillRect(p.textLeft, p.clearY, p.iconX - p.textLeft, tft.height() - p.clearY, theme::BG);
  tft.fillRect(p.iconX, p.iconY - 4, tft.width() - p.iconX, tft.height() - p.iconY + 4, theme::BG);

  const WeatherReport& w = weather::current();
  if (demoStep_ >= 0) {
    drawWeatherIcon(tft, p.iconX, p.iconY, static_cast<IconKind>(demoStep_ / 2),
                    demoStep_ % 2 == 1, moonPhaseOf(w), p.icon);
  } else if (w.ok) {
    drawWeatherIcon(tft, p.iconX, p.iconY, iconForCode(w.conditionCode), !w.isDay,
                    moonPhaseOf(w), p.icon);
  }

  fonts::use(tft, fonts::Id::Temp);
  tft.setTextDatum(R_BASELINE);
  if (!w.ok) {
    // No weather yet: say why, in the spot the temperature would use.
    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(R_BASELINE);
    tft.setTextColor(weather::unavailableIsError() ? theme::PINK : theme::MUTED, theme::BG);
    tft.drawString(fitWidth(tft, weather::unavailableReason(), p.textRight - p.textLeft),
                   p.textRight, p.tempBaseline);
    return;
  }

  // Temperature, rounded, e.g. "64°".
  const float temp = settings_.fahrenheit ? w.tempF : w.tempC;
  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString(String(lroundf(temp)) + "°", p.textRight, p.tempBaseline);

  // Chance of rain/snow this hour: a small pink umbrella, then e.g. "60%".
  const int precip = w.precipChanceAt(time(nullptr));
  if (precip >= 0) {
    fonts::use(tft, fonts::Id::Small);
    const String text = String(precip) + "%";
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(text, p.textRight, p.precipY);
    drawUmbrella(tft, p.textRight - tft.textWidth(text) - 17, p.precipY + 2, theme::PINK);
  }

  // Relative humidity, smaller and muted: e.g. "90%RH".
  if (w.humidity >= 0) {
    fonts::use(tft, fonts::Id::Tiny);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(String(w.humidity) + "%RH", p.textRight, p.humidityY);
  }
}

String HomePage::stripSignature() const {
  const HomeStripSource source = settings_.stripSource();
  if (source == HomeStripSource::Off) return "off";
  String s = String(static_cast<unsigned>(source));
  for (int i = 0; i < home_strip::LINES; i++) {
    s += "/";
    s += settings_.homeStripField[i];
  }
  s += ":";
  switch (source) {
    case HomeStripSource::Wdg:
      s += wdg::generation();
      break;
    case HomeStripSource::Wigle:
      s += wigle::generation();
      break;
    case HomeStripSource::Wardrift:
      s += wardrift::generation();
      break;
    default:
      break;
  }
  for (int i = 0; i < home_strip::LINES; i++) {
    s += "|";
    s += home_strip::formatValue(source, settings_.homeStripField[i]);
  }
  return s;
}

void HomePage::drawStatsStrip(TFT_eSPI& tft) {
  shownStrip_ = stripSignature();
  const HomeStripSource source = settings_.stripSource();
  if (source == HomeStripSource::Off) {
    stripVisible_ = false;
    stripW_ = 0;
    return;
  }

  stripVisible_ = true;
  stripW_ = statsStripWidth(tft);
  stripLeft_ = statsStripLeft(tft);
  stripTop_ = GREETING_Y;
  stripRowH_ = STRIP_ROW_H;

  tft.fillRect(stripLeft_ - 2, stripTop_ - 2, stripW_ + MARGIN + 2,
               home_strip::LINES * STRIP_ROW_H + 2, theme::BG);

  fonts::use(tft, fonts::Id::Tiny);
  for (int i = 0; i < home_strip::LINES; i++) {
    const uint8_t field = settings_.homeStripField[i];
    const String value = home_strip::formatValue(source, field);
    const bool haveValue = value != "—";
    const int y = stripTop_ + i * stripRowH_;
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(home_strip::fieldLabel(source, field), stripLeft_, y);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(home_strip::valueColour(source, field, haveValue), theme::BG);
    tft.drawString(value, tft.width() - MARGIN, y);
  }
}

int HomePage::tapTarget(int16_t x, int16_t y) const {
  if (shownMode_ != Mode::Clock || !stripVisible_) return -1;
  if (x < stripLeft_ - 4 || x > stripLeft_ + stripW_ + MARGIN) return -1;
  if (y < stripTop_ - 4 || y >= stripTop_ + home_strip::LINES * stripRowH_ + 4) return -1;
  return home_strip::pageIndex(settings_.stripSource());
}
