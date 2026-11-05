#include "wardrift_page.h"

#include <WiFi.h>

#include "count_text.h"
#include "fonts.h"
#include "layout.h"
#include "theme.h"
#include "wardrift_service.h"

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
  return s.wardriftUser.length() > 0 && s.wardriftPass.length() > 0;
}

// Same positions as local/previews/wardrift_preview.py on 320×240; mid band
// stretches on a taller screen (see layout::heroStats).
void drawStats(TFT_eSPI& tft) {
  const layout::HeroStats L = layout::heroStats(tft.height());
  const wardrift::Stats& stats = wardrift::current();
  if (!stats.ok) {
    if (wardrift::lastError().length()) {
      centerMessage(tft, wardrift::lastError().c_str(), nullptr, theme::PINK);
    } else if (WiFi.status() != WL_CONNECTED) {
      centerMessage(tft, "Waiting for Wi-Fi", nullptr, theme::MUTED);
    } else {
      centerMessage(tft, "Loading Wardrift...", nullptr, theme::MUTED);
    }
    return;
  }

  fonts::use(tft, fonts::Id::Small);
  const int factionW = stats.faction.length() ? tft.textWidth(stats.faction) : 0;

  fonts::use(tft, fonts::Id::Medium);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::accent(), theme::BG);
  const int nameMax = tft.width() - 2 * MARGIN - (factionW ? factionW + 8 : 0);
  tft.drawString(fitWidth(tft, stats.name, nameMax), MARGIN, L.nameY);

  if (factionW) {
    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(theme::PINK, theme::BG);
    tft.drawString(stats.faction, tft.width() - MARGIN, L.nameY + 6);
  }

  String sub = "level " + String(stats.level);
  if (stats.className.length()) sub += "  ·  " + stats.className;
  if (wardrift::stale()) sub += "  -  stale";
  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString(fitWidth(tft, sub, tft.width() - 2 * MARGIN), MARGIN, L.subY);

  fonts::use(tft, fonts::Id::Clock);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString(compactCount(stats.networks), tft.width() / 2, L.heroY);

  fonts::use(tft, fonts::Id::Tiny);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("networks", tft.width() / 2, L.heroLabelY);

  const int32_t values[] = {stats.routes, stats.threats};
  const char* labels[] = {"Routes", "Threats"};
  const int colW = tft.width() / 2;
  for (int i = 0; i < 2; i++) {
    const int cx = colW * i + colW / 2;
    fonts::use(tft, fonts::Id::Medium);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(compactCount(values[i]), cx, L.rowY);
    fonts::use(tft, fonts::Id::Tiny);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(labels[i], cx, L.rowLabelY);
  }

  String footer;
  if (stats.streakDays == 1) footer = "1 day streak";
  else if (stats.streakDays > 1) footer = String(stats.streakDays) + " day streak";
  if (stats.currency > 0) {
    if (footer.length()) footer += "  ·  ";
    footer += compactCount(stats.currency) + " coin";
  }
  if (!footer.length() && stats.expToNext > 0) {
    footer = compactCount(stats.expToNext) + " to next";
  }
  if (footer.length()) {
    fonts::use(tft, fonts::Id::Small);
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(footer, tft.width() / 2, tft.height() - 8);
  }
}
}  // namespace

uint32_t WardriftPage::signature() const {
  if (!loginSaved(settings_)) return 0;
  return 0x80000000u | wardrift::generation();
}

void WardriftPage::draw(TFT_eSPI& tft) {
  shown_ = signature();
  tft.fillScreen(theme::BG);
  if (!loginSaved(settings_)) {
    centerMessage(tft, "No Wardrift login yet", "Add it on the settings page", theme::TEXT);
    return;
  }
  drawStats(tft);
}

void WardriftPage::update(TFT_eSPI& tft) {
  if (signature() != shown_) draw(tft);
}
