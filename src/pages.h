// Pages: each screen (Home, Hourly, Badge) is a Page. The PageManager knows
// which one is showing and moves to the next on a tap.
//
// A page only DRAWS. Fetching data (weather, badge) will happen in separate
// background "services", so a page is already up to date when you tap to it.
#pragma once
#include <TFT_eSPI.h>

class Page {
 public:
  virtual ~Page() = default;
  virtual const char* name() const = 0;

  // Draw the whole page from scratch (called when the page appears).
  virtual void draw(TFT_eSPI& tft) = 0;

  // Called every loop() while the page is visible, for small redraws
  // (e.g. the clock digits). Default: nothing.
  virtual void update(TFT_eSPI& tft) {}
};

class PageManager {
 public:
  static constexpr size_t MAX_PAGES = 8;

  explicit PageManager(TFT_eSPI& tft) : tft_(tft) {}

  void add(Page* page);
  void next();     // go to the following page (wraps around)
  void redraw();   // redraw the current page from scratch
  void update();   // pass loop() on to the current page

  size_t count() const { return count_; }
  size_t currentIndex() const { return current_; }

 private:
  TFT_eSPI& tft_;
  Page* pages_[MAX_PAGES] = {};
  size_t count_ = 0;
  size_t current_ = 0;
};

// Temporary page for step 3: shows the saved settings and Wi-Fi status so we
// can check the setup portal worked. Replaced by the real Home page in step 4.
struct Settings;
class StatusPage : public Page {
 public:
  explicit StatusPage(const Settings& settings) : settings_(settings) {}
  const char* name() const override { return "Status"; }
  void draw(TFT_eSPI& tft) override;
  void update(TFT_eSPI& tft) override;  // redraws when Wi-Fi connects/drops

 private:
  const Settings& settings_;
  bool shownConnected_ = false;
};

// Temporary stand-in page for step 2: shows its name and page number.
class PlaceholderPage : public Page {
 public:
  PlaceholderPage(const char* name, uint16_t accent, size_t number, size_t total)
      : name_(name), accent_(accent), number_(number), total_(total) {}
  const char* name() const override { return name_; }
  void draw(TFT_eSPI& tft) override;

 private:
  const char* name_;
  uint16_t accent_;
  size_t number_, total_;
};
