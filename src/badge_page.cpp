#include "badge_page.h"

#include <WiFi.h>

#include "count_text.h"
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

// Same positions as local/previews/wdgwars_preview.py.
void drawStats(TFT_eSPI& tft) {
  // The 2.8" screen is 240 px tall. A taller screen drops this block toward
  // the middle so it doesn't sit in a band under the name.
  const int dy = tft.height() <= 240 ? 0 : (tft.height() - 240) / 2;
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
  tft.setTextColor(theme::accent(), theme::BG);
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
  tft.drawString(compactCount(stats.total), tft.width() / 2, 104 + dy);

  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("total", tft.width() / 2, 134 + dy);

  const int32_t values[] = {stats.wifi, stats.ble, stats.aircraft, stats.mesh};
  const char* labels[] = {"WiFi", "BLE", "Air", "Mesh"};
  const int colW = tft.width() / 4;
  for (int i = 0; i < 4; i++) {
    const int cx = colW * i + colW / 2;
    fonts::use(tft, fonts::Id::Medium);
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(compactCount(values[i]), cx, 158 + dy);
    fonts::use(tft, fonts::Id::Tiny);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(labels[i], cx, 184 + dy);
  }

  if (stats.haveRecent) {
    const String footer = "today " + compactCount(stats.today) + "  \u00b7  week " + compactCount(stats.week);
    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(footer, tft.width() / 2, tft.height() - 8);
  }
}
}  // namespace

uint32_t BadgePage::signature() const {
  if (!settings_.wdgKey.length()) return 0;
  return 0x80000000u | wdg::generation();
}

void BadgePage::draw(TFT_eSPI& tft) {
  shown_ = signature();
  tft.fillScreen(theme::BG);
  if (!settings_.wdgKey.length()) {
    centerMessage(tft, "No WDGWars key yet", "Add one on the settings page", theme::TEXT);
    return;
  }
  drawStats(tft);
}

void BadgePage::update(TFT_eSPI& tft) {
  if (signature() != shown_) draw(tft);
}
