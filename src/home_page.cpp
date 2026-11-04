#include "home_page.h"

#include <WiFi.h>

#include "fonts.h"
#include "theme.h"
#include "timekeeping.h"

namespace {
// Layout (landscape, 320 x 240). The right-hand column (x >= RIGHT_COL_X) is
// kept free for the weather in step 5.
constexpr int MARGIN = 14;
constexpr int RIGHT_COL_X = 204;
constexpr int GREETING_Y = 12;      // top of the greeting (24 px font)
constexpr int TIME_TOP = 50;        // top of the area cleared for the clock
constexpr int TIME_BASELINE = 98;   // clock digits and AM/PM sit on this line
constexpr int DATE_Y = 128;         // top of the date (16 px font)
constexpr int PLACE_Y = 209;        // top of the place name (16 px font)

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
  shownPlace_ = "";
  shownReady_ = timekeeping::ready();

  struct tm now;
  const int hour = (shownReady_ && timekeeping::localNow(now)) ? now.tm_hour : 12;
  drawGreeting(tft, hour);
  drawTime(tft);
  drawPlace(tft);
}

void HomePage::update(TFT_eSPI& tft) {
  if (currentMode() != shownMode_ || timekeeping::ready() != shownReady_) {
    draw(tft);
    return;
  }
  if (shownMode_ != Mode::Clock || !shownReady_) return;

  struct tm now;
  if (!timekeeping::localNow(now)) return;
  if (now.tm_hour * 60 + now.tm_min != shownMinute_) drawTime(tft);
  drawGreeting(tft, now.tm_hour);  // only redraws if the text changed
  if (settings_.placeLabel != shownPlace_) drawPlace(tft);
}

void HomePage::drawGreeting(TFT_eSPI& tft, int hour) {
  const String text = greetingFor(hour, settings_.name);
  if (text == shownGreeting_) return;
  shownGreeting_ = text;

  tft.fillRect(0, GREETING_Y - 2, tft.width(), 32, theme::BG);  // wipe the old greeting
  fonts::use(tft, fonts::Id::Medium);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::CYAN, theme::BG);
  tft.drawString(text, MARGIN, GREETING_Y);
}

void HomePage::drawTime(TFT_eSPI& tft) {
  // Clear the time + date area (left column only). Happens once a minute.
  tft.fillRect(0, TIME_TOP, RIGHT_COL_X, DATE_Y + 22 - TIME_TOP, theme::BG);

  struct tm now;
  if (!timekeeping::ready() || !timekeeping::localNow(now)) {
    fonts::use(tft, fonts::Id::Clock);
    tft.setTextDatum(L_BASELINE);
    tft.setTextColor(theme::PANEL, theme::BG);
    tft.drawString("--:--", MARGIN, TIME_BASELINE);
    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(WiFi.status() == WL_CONNECTED ? "Syncing time..." : "Waiting for Wi-Fi...",
                   MARGIN, DATE_Y);
    return;
  }
  shownMinute_ = now.tm_hour * 60 + now.tm_min;

  // 12-hour clock: 0 -> 12 AM, 13 -> 1 PM.
  int hour12 = now.tm_hour % 12;
  if (hour12 == 0) hour12 = 12;
  char digits[6];
  snprintf(digits, sizeof(digits), "%d:%02d", hour12, now.tm_min);

  // Digits and AM/PM share a baseline, so their bottoms line up.
  fonts::use(tft, fonts::Id::Clock);
  tft.setTextDatum(L_BASELINE);
  tft.setTextColor(theme::TEXT, theme::BG);
  const int digitsWidth = tft.drawString(digits, MARGIN, TIME_BASELINE);

  fonts::use(tft, fonts::Id::Medium);
  tft.setTextColor(theme::CYAN, theme::BG);
  tft.drawString(now.tm_hour < 12 ? "AM" : "PM", MARGIN + digitsWidth + 6, TIME_BASELINE);

  // Date: "Wednesday, November 4"
  fonts::use(tft, fonts::Id::Small);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::TEXT, theme::BG);
  const String date = String(DAYS[now.tm_wday]) + ", " + MONTHS[now.tm_mon] + " " + now.tm_mday;
  tft.drawString(date, MARGIN, DATE_Y);
}

void HomePage::drawPlace(TFT_eSPI& tft) {
  shownPlace_ = settings_.placeLabel;
  tft.fillRect(0, PLACE_Y - 2, tft.width(), 20, theme::BG);
  if (shownPlace_.isEmpty()) return;

  // A small cyan dot as a location marker, then the place name.
  tft.fillCircle(MARGIN + 3, PLACE_Y + 8, 3, theme::CYAN);
  fonts::use(tft, fonts::Id::Small);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString(shownPlace_, MARGIN + 12, PLACE_Y);
}
