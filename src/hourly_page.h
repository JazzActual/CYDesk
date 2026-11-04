// The Hourly page: the next 12 hours as a 6×2 grid.
//
//   ┌──────────────────────────────────────┐
//   │ 2 PM   3 PM   4 PM   5 PM   6 PM   7 PM│  hour (cyan = this hour)
//   │ icon   icon   icon   icon   icon   icon│  40 px, sun or moon
//   │ 64°    63°    61°    59°    57°    55° │
//   │ 10%    20%    40%    70%    80%    60% │  chance of rain/snow
//   │ 8 PM   ...                             │
//   │         Weather by WeatherAPI.com      │
//   └──────────────────────────────────────┘
//
// The hours are already in the weather report. This page only draws them.
#pragma once
#include "pages.h"
#include "settings.h"

class HourlyPage : public Page {
 public:
  explicit HourlyPage(const Settings& settings) : settings_(settings) {}
  const char* name() const override { return "Hourly"; }
  void draw(TFT_eSPI& tft) override;
  void update(TFT_eSPI& tft) override;
  void dataChanged(TFT_eSPI&) override {}  // update() redraws when the numbers change

 private:
  // Changes when the grid would look different (new hour, new forecast, °F/°C).
  uint32_t signature() const;

  const Settings& settings_;
  uint32_t shown_ = 0;
};
