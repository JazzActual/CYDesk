// The Home page: greeting, big clock and date; weather in the bottom right.
//
//   ┌──────────────────────────────────────┐
//   │ Morning, Jazz!                       │
//   │                       ☀▲       ☀▼    │
//   │ 10:42 AM            7:19 AM  5:40 PM │  last / next sun event
//   │ Wednesday, November 4                │
//   │                          ┌────────┐  │
//   │                    64°   │  icon  │  │
//   │                  ☂ 60%   │        │  │  chance of rain/snow
//   │                  90%RH   └────────┘  │  relative humidity
//   └──────────────────────────────────────┘
//
// To avoid flicker, only the parts that changed are redrawn: the time once a
// minute, the greeting when the time of day changes, the weather column when
// new weather arrives.
#pragma once
#include "pages.h"
#include "settings.h"

class HomePage : public Page {
 public:
  explicit HomePage(const Settings& settings) : settings_(settings) {}
  const char* name() const override { return "Home"; }
  void draw(TFT_eSPI& tft) override;
  void update(TFT_eSPI& tft) override;
  void dataChanged(TFT_eSPI&) override {}  // update() picks up changes itself

  // Steps through every weather icon (day and night), 2 s each, then returns
  // to the real weather. Started from http://cydesk.local/icons
  void startIconDemo();

 private:
  enum class Mode { FinishSetup, Clock };
  Mode currentMode() const;

  void drawGreeting(TFT_eSPI& tft, int hour);
  void drawTime(TFT_eSPI& tft);
  void drawWeather(TFT_eSPI& tft);
  void drawSunEvents(TFT_eSPI& tft);
  String sunSignature() const;  // which two sun events are showing, and the clock format
  String weatherSignature() const;  // changes whenever the weather column would look different

  const Settings& settings_;
  Mode shownMode_ = Mode::Clock;
  bool shownReady_ = false;
  int shownMinute_ = -1;   // minute-of-day currently on screen
  String shownGreeting_;
  String shownWeather_;
  String shownSun_;

  int demoStep_ = -1;  // -1 = not running; otherwise icon index * 2 + (night ? 1 : 0)
  uint32_t demoNextAt_ = 0;
};
