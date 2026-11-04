// Pages: each screen (Home, Hourly, Badge) is a Page. The PageManager knows
// which one is showing and moves to the next on a tap.
//
// A page only DRAWS. Fetching data (weather, badge) happens in separate
// services, so a page is already up to date when you tap to it.
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

  // Called when new data arrived (e.g. a weather update). Default: redraw
  // everything. Pages that redraw only what changed can override this.
  virtual void dataChanged(TFT_eSPI& tft) { draw(tft); }
};

class PageManager {
 public:
  static constexpr size_t MAX_PAGES = 8;

  explicit PageManager(TFT_eSPI& tft) : tft_(tft) {}

  void add(Page* page);
  void next();     // go to the following page (wraps around)
  void show(size_t index);  // jump straight to a page
  void redraw();   // redraw the current page from scratch
  void update();   // pass loop() on to the current page
  void dataChanged();  // tell the current page new data arrived

  size_t count() const { return count_; }
  size_t currentIndex() const { return current_; }

 private:
  TFT_eSPI& tft_;
  Page* pages_[MAX_PAGES] = {};
  size_t count_ = 0;
  size_t current_ = 0;
};

// Diagnostics page: shows the saved settings, Wi-Fi status and the weather
// check result. Kept as the last page for now; may be removed in polish.
struct Settings;
class StatusPage : public Page {
 public:
  // placeError: why the weather check failed ("" while checking / when OK).
  StatusPage(const Settings& settings, const String& placeError)
      : settings_(settings), placeError_(placeError) {}
  const char* name() const override { return "Status"; }
  void draw(TFT_eSPI& tft) override;
  void update(TFT_eSPI& tft) override;  // redraws when Wi-Fi connects/drops

 private:
  const Settings& settings_;
  const String& placeError_;
  bool shownConnected_ = false;
};

// Temporary stand-in page for step 2: shows its name and page number.
// "Finish setup" screen with a QR code for the settings page (pages.cpp).
void drawFinishSetup(TFT_eSPI& tft);

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
