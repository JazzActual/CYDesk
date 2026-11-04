#include "pages.h"

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
