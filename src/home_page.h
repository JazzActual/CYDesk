// The Home page: greeting, big clock and date. (Weather joins in step 5, in
// the right-hand column.)
//
//   ┌──────────────────────────────────────┐
//   │ Morning, Jazz!                       │
//   │                                      │
//   │ 10:42 AM               (weather      │
//   │ Wednesday, November 4    column)     │
//   │                                      │
//   │ • Springfield, Illinois              │
//   └──────────────────────────────────────┘
//
// To avoid flicker, only the parts that changed are redrawn: the time once a
// minute, the greeting when the time of day changes.
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

 private:
  enum class Mode { FinishSetup, Clock };
  Mode currentMode() const;

  void drawGreeting(TFT_eSPI& tft, int hour);
  void drawTime(TFT_eSPI& tft);
  void drawPlace(TFT_eSPI& tft);

  const Settings& settings_;
  Mode shownMode_ = Mode::Clock;
  bool shownReady_ = false;
  int shownMinute_ = -1;   // minute-of-day currently on screen
  String shownGreeting_;
  String shownPlace_;
};
