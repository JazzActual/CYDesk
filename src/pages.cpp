#include "pages.h"

#include <WiFi.h>

#include "fonts.h"
#include "layout.h"
#include "ota.h"
#include "qr_draw.h"
#include "settings.h"
#include "settings_web.h"
#include "theme.h"

// ---- PageManager -----------------------------------------------------------

void PageManager::add(Page* page) {
  if (count_ < MAX_PAGES) {
    enabled_[count_] = true;
    pages_[count_++] = page;
  }
}

void PageManager::setEnabled(size_t index, bool on) {
  if (index < count_) enabled_[index] = on;
}

void PageManager::settle() {
  if (count_ == 0 || enabled_[current_]) return;
  next();
}

void PageManager::next() {
  if (count_ == 0) return;
  for (size_t step = 1; step <= count_; step++) {
    const size_t index = (current_ + step) % count_;
    if (!enabled_[index]) continue;
    if (index == current_) return;
    show(index);
    return;
  }
}

void PageManager::tap() {
  if (count_ == 0) return;
  // Home is the first page. If it is switched off, the first page that is on.
  size_t home = 0;
  if (!enabled_[0]) {
    for (size_t i = 1; i < count_; i++) {
      if (!enabled_[i]) continue;
      home = i;
      break;
    }
  }
  const bool rested = static_cast<uint32_t>(millis() - shownAt_) >= 30 * 1000;
  if (rested && current_ != home && enabled_[home]) {
    show(home);
    return;
  }
  next();
}

void PageManager::show(size_t index) {
  if (index >= count_) return;
  current_ = index;
  Serial.printf("[pages] -> %s\n", pages_[current_]->name());
  redraw();
  shownAt_ = millis();
}

void PageManager::keepPage() { shownAt_ = millis(); }

void PageManager::redraw() {
  fonts::use(tft_, fonts::Id::BuiltIn);  // pages that want smooth fonts load their own
  if (count_ > 0) pages_[current_]->draw(tft_);
}

void PageManager::update() {
  if (count_ > 0) pages_[current_]->update(tft_);
}

void PageManager::dataChanged() {
  fonts::use(tft_, fonts::Id::BuiltIn);
  if (count_ > 0) pages_[current_]->dataChanged(tft_);
}

// ---- StatusPage ------------------------------------------------------------

namespace {
// Shortens text with "..." until it fits in maxWidth pixels.
String fitText(TFT_eSPI& tft, String text, int maxWidth, uint8_t font) {
  if (tft.textWidth(text, font) <= maxWidth) return text;
  while (text.length() > 0 && tft.textWidth(text + "...", font) > maxWidth) {
    text.remove(text.length() - 1);
  }
  return text + "...";
}
}  // namespace

// Shown when Wi-Fi works but name/ZIP/key are still missing: a QR code and
// the address of the settings page.
void drawFinishSetup(TFT_eSPI& tft) {
  // Home may still have the clock font loaded, and that font only contains
  // digits. Drawing this page with it turns every letter into an empty box
  // (the digits of the IP address still appear). Switch back first.
  fonts::use(tft, fonts::Id::BuiltIn);
  tft.fillScreen(theme::BG);

  // The QR code uses the IP address rather than cydesk.local: phones open it
  // instantly, while ".local" names can take a few seconds to look up.
  const String url = "http://" + WiFi.localIP().toString();
  const int qrMax = layout::setupQrSize(tft);
  const int qrSize = drawQrCode(tft, url.c_str(), 12, 44, qrMax);

  const int x = 12 + qrSize + 14;
  const int copyStep = layout::tall(tft) ? 22 : 18;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::accent(), theme::BG);
  tft.drawString("Finish setup", 12, 10, 4);

  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString("Scan, or open this", x, 46, 2);
  tft.drawString("on a phone or computer", x, 46 + copyStep, 2);
  tft.drawString("on the same Wi-Fi:", x, 46 + 2 * copyStep, 2);
  tft.setTextColor(theme::PINK, theme::BG);
  tft.drawString(String(settings_web::HOSTNAME) + ".local", x, 46 + 3 * copyStep + 8, 4);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("or " + WiFi.localIP().toString(), x, 46 + 4 * copyStep + 26, 2);

  tft.setTextDatum(BC_DATUM);
  tft.drawString("Add name, postal code and WeatherAPI key", tft.width() / 2, tft.height() - 10, 2);
}

String StatusPage::weatherLine() const {
  if (settings_.placeLabel.length()) return "Found: " + settings_.placeLabel;
  if (placeError_.length()) return placeError_;
  return "checking...";
}

String StatusPage::updateLine() const {
  if (ota::updateAvailable()) return String(ota::channelVersion()) + " ready";
  if (ota::checkResult() == ota::CheckResult::Failed) return "check failed";
  if (ota::checkResult() == ota::CheckResult::UpToDate) return "up to date";
  if (ota::checkResult() == ota::CheckResult::Checking) return "checking...";
  return "";
}

void StatusPage::draw(TFT_eSPI& tft) {
  // update() calls draw() directly, skipping the page manager's font reset.
  fonts::use(tft, fonts::Id::BuiltIn);
  shownConnected_ = WiFi.status() == WL_CONNECTED;
  shownWeather_ = weatherLine();
  shownBlock_ = ESP.getMaxAllocHeap();
  if (shownConnected_ && !settings_.isComplete()) {
    drawFinishSetup(tft);
    return;
  }

  tft.fillScreen(theme::BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::accent(), theme::BG);
  tft.drawString("Settings check", 12, 12, 4);

  const int labelX = 12, valueX = 104, valueWidth = tft.width() - valueX - 8;
  const int pitch = layout::statusRowPitch(tft);
  int y = layout::tall(tft) ? 64 : 56;
  auto row = [&](const char* label, const String& value, uint16_t colour) {
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(label, labelX, y, 2);
    tft.setTextColor(colour, theme::BG);
    tft.drawString(fitText(tft, value, valueWidth, 2), valueX, y, 2);
    y += pitch;
  };

  if (shownConnected_) {
    row("Wi-Fi", WiFi.SSID() + "  " + WiFi.localIP().toString(), theme::accent());
  } else {
    row("Wi-Fi", "connecting...", theme::PINK);
  }
  const bool weatherOk = settings_.placeLabel.length() > 0;
  row("Weather", shownWeather_, weatherOk ? theme::accent() : placeError_.length() ? theme::PINK : theme::MUTED);

  // The largest free block. An HTTPS download needs about 40 KB of it.
  const bool low = shownBlock_ < 40 * 1024;
  row("Memory", String(shownBlock_ / 1024) + " KB", low ? theme::PINK : theme::accent());
  shownUpdate_ = updateLine();
  row("Build", ota::version(), theme::MUTED);
  if (shownUpdate_.length()) {
    const uint16_t colour = ota::updateAvailable() ? theme::accent()
                          : ota::checkResult() == ota::CheckResult::Failed ? theme::PINK
                                                                           : theme::MUTED;
    row("Update", shownUpdate_, colour);
  }

  tft.setTextColor(theme::MUTED, theme::BG);
  tft.setTextDatum(BC_DATUM);
  const String hint = shownConnected_
      ? String("settings: ") + settings_web::HOSTNAME + ".local   hold 5 s: Wi-Fi"
      : String("hold 5 s: Wi-Fi setup");
  tft.drawString(hint, tft.width() / 2, tft.height() - 10, 2);
}

void StatusPage::update(TFT_eSPI& tft) {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const uint32_t block = ESP.getMaxAllocHeap();
  const uint32_t gap = block > shownBlock_ ? block - shownBlock_ : shownBlock_ - block;
  const bool memoryMoved = gap >= 8 * 1024 || (block < 40 * 1024) != (shownBlock_ < 40 * 1024);
  if (connected != shownConnected_ || memoryMoved || weatherLine() != shownWeather_ ||
      updateLine() != shownUpdate_) {
    draw(tft);
  }
}

// ---- PlaceholderPage -------------------------------------------------------

void PlaceholderPage::draw(TFT_eSPI& tft) {
  tft.fillScreen(theme::BG);

  // Coloured stripe across the top, like the card's cyan top edge.

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(accent_, theme::BG);
  tft.drawString(name_, tft.width() / 2, tft.height() / 2 - 20, 4);

  char line[32];
  snprintf(line, sizeof(line), "page %u of %u", unsigned(number_), unsigned(total_));
  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString(line, tft.width() / 2, tft.height() / 2 + 14, 2);

  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("tap: next page   hold 5 s: setup", tft.width() / 2, tft.height() - 20, 2);
}
