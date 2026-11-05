#include "wigle_page.h"

#include <WiFi.h>

#include "count_text.h"
#include "fonts.h"
#include "theme.h"
#include "wigle_service.h"

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

bool loginSaved(const Settings& s) {
  return s.wigleName.length() > 0 && s.wigleToken.length() > 0;
}

// Same positions as local/previews/wigle_preview.py.
void drawStats(TFT_eSPI& tft) {
  const int dy = tft.height() <= 240 ? 0 : (tft.height() - 240) / 2;
  const wigle::Stats& stats = wigle::current();
  if (!stats.ok) {
    if (wigle::lastError().length()) {
      centerMessage(tft, wigle::lastError().c_str(), nullptr, theme::PINK);
    } else if (WiFi.status() != WL_CONNECTED) {
      centerMessage(tft, "Waiting for Wi-Fi", nullptr, theme::MUTED);
    } else {
      centerMessage(tft, "Loading WiGLE...", nullptr, theme::MUTED);
    }
    return;
  }

  const String rank = "#" + compactCount(stats.rank);
  fonts::use(tft, fonts::Id::Small);
  const int rankW = tft.textWidth(rank);

  fonts::use(tft, fonts::Id::Medium);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::accent(), theme::BG);
  const int nameMax = tft.width() - 2 * MARGIN - rankW - 8;
  tft.drawString(fitWidth(tft, stats.username, nameMax), MARGIN, 12);

  fonts::use(tft, fonts::Id::Small);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(theme::PINK, theme::BG);
  tft.drawString(rank, tft.width() - MARGIN, 18);

  String sub = "month #" + compactCount(stats.monthRank);
  if (wigle::stale()) sub += "  -  stale";
  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString(sub, MARGIN, 40);

  const String hero = compactCount(stats.wifi);
  fonts::use(tft, fonts::Id::Clock);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString(hero, tft.width() / 2, 104 + dy);

  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("Wi-Fi", tft.width() / 2, 134 + dy);

  const int32_t values[] = {stats.cell, stats.bt};
  const char* labels[] = {"Cell", "Bluetooth"};
  const int colW = tft.width() / 2;
  for (int i = 0; i < 2; i++) {
    const int cx = colW * i + colW / 2;
    fonts::use(tft, fonts::Id::Medium);
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(compactCount(values[i]), cx, 158 + dy);
    fonts::use(tft, fonts::Id::Tiny);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(labels[i], cx, 184 + dy);
  }

  fonts::use(tft, fonts::Id::Small);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("this month " + compactCount(stats.monthCount), tft.width() / 2, tft.height() - 8);
}
}  // namespace

uint32_t WiglePage::signature() const {
  if (!loginSaved(settings_)) return 0;
  return 0x80000000u | wigle::generation();
}

void WiglePage::draw(TFT_eSPI& tft) {
  shown_ = signature();
  tft.fillScreen(theme::BG);
  if (!loginSaved(settings_)) {
    centerMessage(tft, "No WiGLE login yet", "Add it on the settings page", theme::TEXT);
    return;
  }
  drawStats(tft);
}

void WiglePage::update(TFT_eSPI& tft) {
  if (signature() != shown_) draw(tft);
}
