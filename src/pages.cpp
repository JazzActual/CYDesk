#include "pages.h"

#include <WiFi.h>

#include "fonts.h"
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
    current_ = index;
    Serial.printf("[pages] -> %s\n", pages_[current_]->name());
    redraw();
    return;
  }
}

void PageManager::show(size_t index) {
  if (index >= count_) return;
  current_ = index;
  redraw();
}

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
  const int qrSize = drawQrCode(tft, url.c_str(), 12, 44, 120);

  const int x = 12 + qrSize + 14;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::accent(), theme::BG);
  tft.drawString("Finish setup", 12, 10, 4);

  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString("Scan, or open this", x, 46, 2);
  tft.drawString("on a phone or computer", x, 64, 2);
  tft.drawString("on the same Wi-Fi:", x, 82, 2);
  tft.setTextColor(theme::PINK, theme::BG);
  tft.drawString(String(settings_web::HOSTNAME) + ".local", x, 108, 4);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("or " + WiFi.localIP().toString(), x, 140, 2);

  tft.setTextDatum(BC_DATUM);
  tft.drawString("Add your name, ZIP code and WeatherAPI key", tft.width() / 2, tft.height() - 10, 2);
}

void StatusPage::draw(TFT_eSPI& tft) {
  // update() calls draw() directly, skipping the page manager's font reset.
  fonts::use(tft, fonts::Id::BuiltIn);
  shownConnected_ = WiFi.status() == WL_CONNECTED;
  if (shownConnected_ && !settings_.isComplete()) {
    drawFinishSetup(tft);
    return;
  }

  tft.fillScreen(theme::BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::accent(), theme::BG);
  tft.drawString("Settings check", 12, 12, 4);

  const int labelX = 12, valueX = 104, valueWidth = tft.width() - valueX - 8;
  int y = 44;
  auto row = [&](const char* label, const String& value, uint16_t colour) {
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(label, labelX, y, 2);
    tft.setTextColor(colour, theme::BG);
    tft.drawString(fitText(tft, value.length() ? value : String("(not set)"), valueWidth, 2),
                   valueX, y, 2);
    y += 20;
  };

  if (shownConnected_) {
    row("Wi-Fi", WiFi.SSID() + "  " + WiFi.localIP().toString(), theme::accent());
  } else {
    row("Wi-Fi", "connecting...", theme::PINK);
  }
  row("Name", settings_.name, theme::TEXT);
  row("ZIP", settings_.zip, theme::TEXT);
  row("API key", settings_.weatherKey.length() ? "saved" : "", theme::TEXT);
  if (settings_.placeLabel.length()) {
    row("Weather", "Found: " + settings_.placeLabel, theme::accent());
  } else if (placeError_.length()) {
    row("Weather", placeError_, theme::PINK);
  } else {
    row("Weather", "checking...", theme::MUTED);
  }
  row("Units", String(settings_.fahrenheit ? "Fahrenheit" : "Celsius") +
                   (settings_.clock24h ? ", 24-hour clock" : ", 12-hour clock"), theme::TEXT);
  row("Badge", settings_.wdgKey.length() ? "WDGWars stats" : "",
      settings_.wdgKey.length() ? theme::accent() : theme::TEXT);
  const bool wigleSaved = settings_.wigleName.length() && settings_.wigleToken.length();
  row("WiGLE", wigleSaved ? "stats" : "", wigleSaved ? theme::accent() : theme::TEXT);
  const bool wardriftSaved = settings_.wardriftUser.length() && settings_.wardriftPass.length();
  row("Wardrift", wardriftSaved ? "stats" : "", wardriftSaved ? theme::accent() : theme::TEXT);
  row("GitHub", settings_.githubRepos.length() ? "stars" : "",
      settings_.githubRepos.length() ? theme::accent() : theme::TEXT);
  const char* animal = "None";
  if (settings_.petAnimal == static_cast<uint8_t>(PetAnimal::Fox)) animal = "Fox";
  else if (settings_.petAnimal == static_cast<uint8_t>(PetAnimal::Raccoon)) animal = "Raccoon";
  else if (settings_.petAnimal == static_cast<uint8_t>(PetAnimal::Owl)) animal = "Owl";
  else if (settings_.petAnimal == static_cast<uint8_t>(PetAnimal::Bear)) animal = "Bear";
  const char* mode = "Off";
  if (settings_.petMode == static_cast<uint8_t>(PetMode::Idle)) mode = "Idle";
  else if (settings_.petMode == static_cast<uint8_t>(PetMode::Reacts)) mode = "Reacts";
  row("Pet", String(animal) + ", " + mode, theme::TEXT);

  tft.setTextColor(theme::MUTED, theme::BG);
  tft.setTextDatum(BC_DATUM);
  const String hint = shownConnected_
      ? String("settings: ") + settings_web::HOSTNAME + ".local   hold 5 s: Wi-Fi"
      : String("hold 5 s: Wi-Fi setup");
  tft.drawString(hint, tft.width() / 2, tft.height() - 12, 2);
}

void StatusPage::update(TFT_eSPI& tft) {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != shownConnected_) draw(tft);
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
