// The Wardrift page. It draws one character's level and wardrive counts.
//
//   ┌──────────────────────────────────────┐
//   │ Mara                        Crusade  │
//   │ level 12 · Warden                    │
//   │               18.4k                  │
//   │              networks                │
//   │        37                 6          │
//   │      Routes            Threats       │
//   │         4 day streak · 860 coin      │
//   └──────────────────────────────────────┘
//
// Fetching happens in wardrift_service. This page only draws.
#pragma once
#include "pages.h"
#include "settings.h"

class WardriftPage : public Page {
 public:
  explicit WardriftPage(const Settings& settings) : settings_(settings) {}
  const char* name() const override { return "Wardrift"; }
  void draw(TFT_eSPI& tft) override;
  void update(TFT_eSPI& tft) override;

 private:
  uint32_t signature() const;

  const Settings& settings_;
  uint32_t shown_ = 0;
};
