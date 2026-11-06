// The Home page: greeting, big clock and date; weather in the bottom right;
// an optional three-line stats strip in the top right from one chosen source.
//
//   ┌──────────────────────────────────────┐
//   │ Morning, Jazz!          mo #   #128  │  ← WiGLE example (configurable)
//   │ Wednesday, November 4   rank  #1.2k  │
//   │ 10:42 AM                WiFi  45.3k  │
//   │                    ☀▲ 7:19    ☀▼ 5:40│  last / next sun (over weather)
//   │                    64°   │  icon  │  │  72 px weather icon
//   │                  ☂ 60%   │        │  │  chance of rain/snow
//   │                  90%RH   └────────┘  │  relative humidity
//   └──────────────────────────────────────┘
//
// To avoid flicker, only the parts that changed are redrawn: the time once a
// minute, the greeting when the time of day changes, the weather column when
// new weather arrives, the strip when a stats download finishes.
//
// A tap on the strip jumps to that source's page; a tap elsewhere advances.
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

  // Which page a tap at (x, y) should open, or -1 to keep the normal advance.
  int tapTarget(int16_t x, int16_t y) const;

 private:
  enum class Mode { FinishSetup, Clock };
  Mode currentMode() const;

  void drawGreeting(TFT_eSPI& tft, int hour);
  void drawTime(TFT_eSPI& tft);
  void drawWeather(TFT_eSPI& tft);
  void drawSunEvents(TFT_eSPI& tft);
  void drawStatsStrip(TFT_eSPI& tft);
  String sunSignature() const;
  String weatherSignature() const;
  String stripSignature() const;

  const Settings& settings_;
  Mode shownMode_ = Mode::Clock;
  bool shownReady_ = false;
  int shownMinute_ = -1;
  String shownFreshness_;
  String shownGreeting_;
  String shownWeather_;
  String shownSun_;
  String shownStrip_;

  // Last drawn strip bounds, for tap hit-testing (landscape pixels).
  int stripLeft_ = 0;
  int stripTop_ = 0;
  int stripW_ = 0;
  int stripRowH_ = 0;
  bool stripVisible_ = false;

  int demoStep_ = -1;
  uint32_t demoNextAt_ = 0;
};
