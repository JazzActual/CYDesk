#include "badge_page.h"

#include <WiFi.h>

#include "badge_service.h"
#include "fonts.h"
#include "theme.h"
#include "wdgwars_service.h"

namespace {
constexpr int MARGIN = 14;

void centerMessage(TFT_eSPI& tft, const char* primary, const char* secondary, uint16_t color) {
  fonts::use(tft, fonts::Id::Small);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, theme::BG);
  if (secondary && secondary[0]) {
    tft.drawString(primary, tft.width() / 2, tft.height() / 2 - 14);
    fonts::use(tft, fonts::Id::Tiny);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(secondary, tft.width() / 2, tft.height() / 2 + 12);
  } else {
    tft.drawString(primary, tft.width() / 2, tft.height() / 2);
  }
}

String fitWidth(TFT_eSPI& tft, const String& text, int maxWidth) {
  if (tft.textWidth(text) <= maxWidth) return text;
  String fitted = text;
  while (fitted.length() > 1) {
    fitted.remove(fitted.length() - 1);
    if (tft.textWidth(fitted + "...") <= maxWidth) return fitted + "...";
  }
  return fitted;
}

// 39619 -> "39,619". The 56 px font has the comma; the smaller fonts do too.
String withCommas(int32_t n) {
  if (n < 0) n = 0;
  const String digits = String(static_cast<long>(n));
  String out;
  const int len = digits.length();
  for (int i = 0; i < len; i++) {
    if (i > 0 && (len - i) % 3 == 0) out += ',';
    out += digits[i];
  }
  return out;
}

// "stale" sits in the navy margin. A picture that fills the screen gets a
// small label in the corner instead of covering the middle of the badge.
void drawStale(TFT_eSPI& tft, int imgY, int imgH) {
  const int topGap = imgY;
  const int bottomGap = tft.height() - (imgY + imgH);
  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextColor(theme::MUTED, theme::BG);
  if (bottomGap >= 16) {
    tft.setTextDatum(MC_DATUM);
    tft.drawString("stale", tft.width() / 2, imgY + imgH + bottomGap / 2);
  } else if (topGap >= 16) {
    tft.setTextDatum(MC_DATUM);
    tft.drawString("stale", tft.width() / 2, topGap / 2);
  } else {
    tft.fillRect(tft.width() - 52, tft.height() - 18, 52, 18, theme::BG);
    tft.setTextDatum(BR_DATUM);
    tft.drawString("stale", tft.width() - 4, tft.height() - 2);
  }
}

void drawImage(TFT_eSPI& tft, const Settings& settings) {
  if (settings.badgeUrl.isEmpty()) {
    centerMessage(tft, "No badge yet", "Add one on the settings page", theme::TEXT);
    return;
  }

  int x = 0, y = 0, w = 0, h = 0;
  if (badge::hasImage()) {
    if (!badge::paint(tft, x, y, w, h)) {
      centerMessage(tft, "Couldn't read the badge image", nullptr, theme::PINK);
      return;
    }
    if (badge::stale()) drawStale(tft, y, h);
    return;
  }

  if (badge::lastError().length()) {
    centerMessage(tft, badge::lastError().c_str(), nullptr, theme::PINK);
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    centerMessage(tft, "Waiting for Wi-Fi", nullptr, theme::MUTED);
    return;
  }
  centerMessage(tft, "Loading badge...", nullptr, theme::MUTED);
}

// Same positions as local/previews/wdgwars_preview.py.
void drawStats(TFT_eSPI& tft) {
  const wdg::Stats& stats = wdg::current();
  if (!stats.ok) {
    if (wdg::lastError().length()) {
      centerMessage(tft, wdg::lastError().c_str(), nullptr, theme::PINK);
    } else if (WiFi.status() != WL_CONNECTED) {
      centerMessage(tft, "Waiting for Wi-Fi", nullptr, theme::MUTED);
    } else {
      centerMessage(tft, "Loading WDGWars...", nullptr, theme::MUTED);
    }
    return;
  }

  fonts::use(tft, fonts::Id::Small);
  const String gang = fitWidth(tft, stats.gang, 150);
  const int gangW = stats.gang.length() ? tft.textWidth(gang) : 0;

  fonts::use(tft, fonts::Id::Medium);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::CYAN, theme::BG);
  const int nameMax = tft.width() - 2 * MARGIN - (gangW ? gangW + 8 : 0);
  tft.drawString(fitWidth(tft, stats.username, nameMax), MARGIN, 12);

  if (gangW) {
    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(theme::PINK, theme::BG);
    tft.drawString(gang, tft.width() - MARGIN, 18);
  }

  String badges = String(stats.badges) + (stats.badges == 1 ? " badge" : " badges");
  if (wdg::stale()) badges += " - stale";
  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString(badges, MARGIN, 40);

  fonts::use(tft, fonts::Id::Clock);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString(withCommas(stats.total), tft.width() / 2, 104);

  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("total", tft.width() / 2, 134);

  const int32_t values[] = {stats.wifi, stats.ble, stats.aircraft, stats.mesh};
  const char* labels[] = {"WiFi", "BLE", "Air", "Mesh"};
  const int colW = tft.width() / 4;
  for (int i = 0; i < 4; i++) {
    const int cx = colW * i + colW / 2;
    fonts::use(tft, fonts::Id::Medium);
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(withCommas(values[i]), cx, 158);
    fonts::use(tft, fonts::Id::Tiny);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(labels[i], cx, 184);
  }

  if (stats.haveRecent) {
    const String footer = "today " + withCommas(stats.today) + "  \u00b7  week " + withCommas(stats.week);
    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(footer, tft.width() / 2, tft.height() - 8);
  }
}
}  // namespace

uint32_t BadgePage::signature() const {
  // The top bit keeps a stats screen from matching a picture screen by chance.
  if (settings_.wdgKey.length()) return 0x80000000u | wdg::generation();
  return badge::generation();
}

void BadgePage::draw(TFT_eSPI& tft) {
  shown_ = signature();
  tft.fillScreen(theme::BG);
  if (settings_.wdgKey.length()) drawStats(tft);
  else drawImage(tft, settings_);
}

void BadgePage::update(TFT_eSPI& tft) {
  if (signature() != shown_) draw(tft);
}
