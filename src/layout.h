// Screen-size helpers. The desk was designed on the 2.8" board (320×240
// landscape). The 3.5" board is 480×320 landscape: same pages, more room.
//
// Home anchors weather to the bottom-right so the 2.8" positions stay as
// approved and the extra pixels fall into the middle. The hero-stat pages
// (Badge, WiGLE, Wardrift, Stars) keep the header at the top and the footer
// at the bottom, and stretch the mid band on a taller screen instead of
// sliding a fixed 240 px block down.
#pragma once
#include <TFT_eSPI.h>

namespace layout {

// Native landscape height of the 2.8" board. Anything taller is the 3.5".
constexpr int BASE_H = 240;
constexpr int BASE_W = 320;

// TFT_eSPI::width/height are not const, so these take a non-const ref.
inline bool tall(TFT_eSPI& tft) { return tft.height() > BASE_H; }
inline bool wide(TFT_eSPI& tft) { return tft.width() > BASE_W; }

// Weather icon on Home: a touch larger when there is room under the clock.
inline int homeWeatherIcon(TFT_eSPI& tft) { return tall(tft) ? 88 : 72; }

// Hourly tile icon: same idea, keeps the 6×2 grid readable on 480×320.
inline int hourlyIcon(TFT_eSPI& tft) { return tall(tft) ? 48 : 40; }

// Finish-setup QR: grow with the shorter edge so it stays scannable.
inline int setupQrSize(TFT_eSPI& tft) {
  const int side = tft.height() < tft.width() ? tft.height() : tft.width();
  // 120 on 240-tall; ~160 on 320-tall. Leave room for the side copy.
  return side <= BASE_H ? 120 : 160;
}

// Vertical positions for the hero-count pages. Exact 2.8" numbers when the
// screen is 240 tall; proportional mid-band on a taller screen.
struct HeroStats {
  int nameY;       // account / repo name
  int subY;        // muted line under the name
  int heroY;       // big count (MC_DATUM)
  int heroLabelY;  // "total" / "Wi-Fi" / "stars" under the count (TC_DATUM)
  int rowY;        // secondary counts
  int rowLabelY;   // labels under those
  int trendY;      // Stars daily line (top of the thin chart)
};

inline HeroStats heroStats(int height) {
  HeroStats s;
  s.nameY = 12;
  s.subY = 40;
  if (height <= BASE_H) {
    s.heroY = 104;
    s.heroLabelY = 134;
    s.rowY = 158;
    s.rowLabelY = 184;
    s.trendY = 160;
    return s;
  }
  // Mid band between the header (ends ~52) and the footer (starts ~height-28).
  // Fractions match the 2.8" mid (52..212 = 160 px).
  const int midTop = 52;
  const int midBot = height - 28;
  const int mid = midBot - midTop;
  s.heroY = midTop + mid * 52 / 160;
  s.heroLabelY = midTop + mid * 82 / 160;
  s.rowY = midTop + mid * 106 / 160;
  s.rowLabelY = midTop + mid * 132 / 160;
  s.trendY = midTop + mid * 108 / 160;
  return s;
}

// Status page row pitch: a little more air on a taller screen.
inline int statusRowPitch(TFT_eSPI& tft) { return tall(tft) ? 36 : 28; }

}  // namespace layout
