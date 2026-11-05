// The WiGLE page. It draws the saved account's rank and discovery counts.
//
//   ┌──────────────────────────────────────┐
//   │ Jazz                             #1.8k│
//   │ month #96                            │
//   │               24.9k                  │
//   │               Wi-Fi                  │
//   │       1.3k              860          │
//   │       Cell           Bluetooth       │
//   │            this month 42             │
//   └──────────────────────────────────────┘
//
// Fetching happens in wigle_service. This page only draws.
#pragma once
#include "pages.h"
#include "settings.h"

class WiglePage : public Page {
 public:
  explicit WiglePage(const Settings& settings) : settings_(settings) {}
  const char* name() const override { return "WiGLE"; }
  void draw(TFT_eSPI& tft) override;
  void update(TFT_eSPI& tft) override;

 private:
  uint32_t signature() const;

  const Settings& settings_;
  uint32_t shown_ = 0;
};
