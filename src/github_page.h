// Two GitHub pages share one fetch.
// Stars is the first repo: a big count, the daily line, and the address.
// Repos is the list, and it shows once a second repo is saved.
//
//   ┌──────────────────────────────────────┐
//   │ arduino-esp32                        │
//   │ espressif                            │
//   │               1.2k                   │
//   │               stars                  │
//   └──────────────────────────────────────┘
//
// Fetching happens in github_service. This page only draws.
#pragma once
#include "pages.h"
#include "settings.h"

class GithubPage : public Page {
 public:
  explicit GithubPage(const Settings& settings) : settings_(settings) {}
  const char* name() const override { return "Stars"; }
  void draw(TFT_eSPI& tft) override;
  void update(TFT_eSPI& tft) override;

 private:
  uint32_t signature() const;

  const Settings& settings_;
  uint32_t shown_ = 0;
};

// The list of every saved repo. With only one repo, this page asks for another.
class GithubListPage : public Page {
 public:
  explicit GithubListPage(const Settings& settings) : settings_(settings) {}
  const char* name() const override { return "Repos"; }
  void draw(TFT_eSPI& tft) override;
  void update(TFT_eSPI& tft) override;

 private:
  uint32_t signature() const;

  const Settings& settings_;
  uint32_t shown_ = 0;
};
