// The Badge page. It draws the saved WDGWars account's stats.
//
//   ┌──────────────────────────────────────┐
//   │ handle                         gang  │
//   │ 14 badges                            │
//   │              39,619                  │
//   │               total                  │
//   │  37,469     2,109      22       93   │
//   │   WiFi       BLE      Air     Mesh   │
//   │        today 100 · week 900          │
//   └──────────────────────────────────────┘
//
// Fetching happens in wdgwars_service. This page only draws.
#pragma once
#include "pages.h"
#include "settings.h"

class BadgePage : public Page {
 public:
  explicit BadgePage(const Settings& settings) : settings_(settings) {}
  const char* name() const override { return "Badge"; }
  void draw(TFT_eSPI& tft) override;
  void update(TFT_eSPI& tft) override;

 private:
  uint32_t signature() const;

  const Settings& settings_;
  uint32_t shown_ = 0;
};
