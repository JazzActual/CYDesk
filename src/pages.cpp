#include "pages.h"

#include <WiFi.h>

#include "settings.h"
#include "theme.h"

// ---- PageManager -----------------------------------------------------------

void PageManager::add(Page* page) {
  if (count_ < MAX_PAGES) pages_[count_++] = page;
}

void PageManager::next() {
  if (count_ == 0) return;
  current_ = (current_ + 1) % count_;
  Serial.printf("[pages] -> %s\n", pages_[current_]->name());
  redraw();
}

void PageManager::redraw() {
  if (count_ > 0) pages_[current_]->draw(tft_);
}

void PageManager::update() {
  if (count_ > 0) pages_[current_]->update(tft_);
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

void StatusPage::draw(TFT_eSPI& tft) {
  shownConnected_ = WiFi.status() == WL_CONNECTED;

  tft.fillScreen(theme::BG);
  tft.fillRect(0, 0, tft.width(), 3, theme::CYAN);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::CYAN, theme::BG);
  tft.drawString("Settings check", 12, 12, 4);

  const int labelX = 12, valueX = 92, valueWidth = tft.width() - valueX - 8;
  int y = 50;
  auto row = [&](const char* label, const String& value, uint16_t colour) {
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(label, labelX, y, 2);
    tft.setTextColor(colour, theme::BG);
    tft.drawString(fitText(tft, value.length() ? value : String("(not set)"), valueWidth, 2),
                   valueX, y, 2);
    y += 22;
  };

  if (shownConnected_) {
    row("Wi-Fi", WiFi.SSID() + "  " + WiFi.localIP().toString(), theme::CYAN);
  } else {
    row("Wi-Fi", "connecting...", theme::PINK);
  }
  row("Name", settings_.name, theme::TEXT);
  row("City", settings_.city, theme::TEXT);
  row("Units", settings_.fahrenheit ? "Fahrenheit" : "Celsius", theme::TEXT);
  row("Badge", settings_.badgeUrl, theme::TEXT);

  tft.setTextColor(theme::MUTED, theme::BG);
  tft.setTextDatum(BC_DATUM);
  tft.drawString("tap: next page   hold 5 s: setup", tft.width() / 2, tft.height() - 12, 2);
}

void StatusPage::update(TFT_eSPI& tft) {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != shownConnected_) draw(tft);
}

// ---- PlaceholderPage -------------------------------------------------------

void PlaceholderPage::draw(TFT_eSPI& tft) {
  tft.fillScreen(theme::BG);

  // Coloured stripe across the top, like the card's cyan top edge.
  tft.fillRect(0, 0, tft.width(), 3, accent_);

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
