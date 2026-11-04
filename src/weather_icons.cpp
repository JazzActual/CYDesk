#include "weather_icons.h"

#include <math.h>

#include "theme.h"

namespace {
using theme::rgb;

// Icon colours (the same ones used in the preview).
constexpr uint16_t SUN = rgb(255, 200, 61);
constexpr uint16_t MOON = rgb(236, 228, 205);
constexpr uint16_t MOON_DARK = rgb(34, 46, 60);  // unlit part, faintly visible
constexpr uint16_t CLOUD_LIGHT = rgb(205, 216, 228);
constexpr uint16_t CLOUD_MID = rgb(150, 166, 184);
constexpr uint16_t CLOUD_DARK = rgb(110, 126, 146);
constexpr uint16_t RAIN = rgb(79, 195, 247);
constexpr uint16_t SNOW = rgb(240, 246, 252);
constexpr uint16_t BOLT = rgb(255, 214, 64);

// ---- colour blending for the moon ------------------------------------------

uint16_t blend(uint16_t a, uint16_t b, float t) {  // t=0 -> a, t=1 -> b
  const int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  const int r = ar + (br - ar) * t + 0.5f, g = ag + (bg - ag) * t + 0.5f, bl = ab + (bb - ab) * t + 0.5f;
  return (r << 11) | (g << 5) | bl;
}

// ---- the layers -------------------------------------------------------------

void drawSun(TFT_eSprite& s, float cx, float cy, float r) {
  for (int i = 0; i < 8; i++) {
    const float a = i * PI / 4;
    s.drawWideLine(cx + cosf(a) * (r + 5), cy + sinf(a) * (r + 5),
                   cx + cosf(a) * (r + 11), cy + sinf(a) * (r + 11), 3.2f, SUN, theme::BG);
  }
  s.fillSmoothCircle(cx, cy, r, SUN, theme::BG);
}

// The moon is calculated pixel by pixel: each pixel is split into 4x4 sample
// points, and we count how many fall on the disc and how many on its lit part.
// That gives soft edges on both the outline and the shadow line.
void drawMoon(TFT_eSprite& s, float cx, float cy, float r, float phase) {
  // Fraction of the disc that's lit: 0 at new moon, 1 at full moon.
  const float illum = (1 - cosf(2 * PI * phase)) / 2;
  const bool waxing = phase < 0.5f;  // waxing = lit on the right (northern hemisphere)

  const int x0 = floorf(cx - r - 1), x1 = ceilf(cx + r + 1);
  const int y0 = floorf(cy - r - 1), y1 = ceilf(cy + r + 1);
  for (int py = y0; py <= y1; py++) {
    for (int px = x0; px <= x1; px++) {
      int onDisc = 0, onLit = 0;
      for (int sy = 0; sy < 4; sy++) {
        for (int sx = 0; sx < 4; sx++) {
          const float x = px + (sx + 0.5f) / 4 - cx;
          const float y = py + (sy + 0.5f) / 4 - cy;
          if (x * x + y * y > r * r) continue;
          onDisc++;
          // Half-width of the disc on this row, and where the shadow line is.
          const float half = sqrtf(r * r - y * y);
          const float terminator = half * (1 - 2 * illum);
          const bool lit = waxing ? (x >= terminator) : (x <= -terminator);
          if (lit) onLit++;
        }
      }
      if (onDisc == 0) continue;
      // Mix: background -> dark disc -> lit moon.
      const uint16_t under = s.readPixel(px, py);
      uint16_t c = blend(under, MOON_DARK, onDisc / 16.0f);
      if (onLit) c = blend(c, MOON, onLit / (float)onDisc);
      s.drawPixel(px, py, c);
    }
  }
}

// Sky position: big and centred for clear skies, top-left when weather is
// drawn on top, in between for partly cloudy / fog.
enum class SkyPos { Big, Normal, Corner };

void drawSky(TFT_eSprite& s, bool night, float moonPhase, SkyPos pos) {
  float cx = 38, cy = 36;
  if (pos == SkyPos::Big) { cx = 48; cy = 44; }
  if (pos == SkyPos::Corner) { cx = 30; cy = 27; }
  const bool big = pos == SkyPos::Big;
  if (night) {
    drawMoon(s, cx, cy, big ? 26 : 20, moonPhase);
  } else {
    drawSun(s, cx, cy, big ? 19 : 15);
  }
}

// A cloud: three circles on a rounded base. (ox, oy) = top-left of its
// design box, scale = size. A background-coloured rim is drawn first so the
// cloud stands out against the sun or moon behind it.
void drawCloud(TFT_eSprite& s, float ox, float oy, float scale, uint16_t colour) {
  struct Circle { float x, y, r; };
  const Circle parts[] = {{14, 10, 13}, {32, 2, 17}, {47, 12, 11}};
  const float bx0 = 2, by0 = 10, bx1 = 58, by1 = 32, br = 11;

  for (int pass = 0; pass < 2; pass++) {
    const float grow = pass == 0 ? 3 : 0;
    const uint16_t c = pass == 0 ? theme::BG : colour;
    for (const Circle& p : parts) {
      s.fillSmoothCircle(ox + p.x * scale, oy + p.y * scale, p.r * scale + grow, c, theme::BG);
    }
    const int x = lroundf(ox + bx0 * scale - grow), y = lroundf(oy + by0 * scale - grow);
    const int w = lroundf((bx1 - bx0) * scale + 2 * grow), h = lroundf((by1 - by0) * scale + 2 * grow);
    s.fillSmoothRoundRect(x, y, w, h, lroundf(br * scale + grow), c, theme::BG);
  }
}

void drawDrops(TFT_eSprite& s, const float* xs, int count, float y0, float length) {
  for (int i = 0; i < count; i++) {
    s.drawWideLine(xs[i], y0, xs[i] - length * 0.35f, y0 + length, 3, RAIN, theme::BG);
  }
}

void drawFlake(TFT_eSprite& s, float x, float y) {
  constexpr float r = 3.2f;
  for (int i = 0; i < 3; i++) {
    const float a = i * PI / 3;
    s.drawWideLine(x - cosf(a) * r, y - sinf(a) * r, x + cosf(a) * r, y + sinf(a) * r, 1.6f, SNOW,
                   theme::BG);
  }
}

void drawBolt(TFT_eSprite& s, int ox, int oy) {
  // The lightning bolt outline, split into triangles.
  const int px[] = {8, 0, 6, 2, 14, 8, 12};
  const int py[] = {0, 14, 14, 26, 9, 9, 0};
  auto tri = [&](int a, int b, int c) {
    s.fillTriangle(ox + px[a], oy + py[a], ox + px[b], oy + py[b], ox + px[c], oy + py[c], BOLT);
  };
  tri(0, 1, 5);
  tri(0, 5, 6);
  tri(1, 2, 5);
  tri(2, 3, 4);
  tri(2, 4, 5);
}

void drawFog(TFT_eSprite& s) {
  s.drawWideLine(14, 62, 70, 62, 5, CLOUD_MID, theme::BG);
  s.drawWideLine(24, 72, 82, 72, 5, CLOUD_MID, theme::BG);
  s.drawWideLine(10, 82, 62, 82, 5, CLOUD_MID, theme::BG);
}

// The sun is only drawn for clear and partly cloudy. A sun peeking past a
// cloud or some rain looks like a break in the clouds. The moon is drawn for
// every icon at night, so the phase stays visible behind the weather.
void drawSkyLayer(TFT_eSprite& s, IconKind kind, bool night, float phase, SkyPos pos) {
  if (night || kind == IconKind::Clear || kind == IconKind::PartlyCloudy) {
    drawSky(s, night, phase, pos);
  }
}

void drawIcon(TFT_eSprite& s, IconKind kind, bool night, float phase) {
  switch (kind) {
    case IconKind::Clear:
      drawSkyLayer(s, kind, night, phase, SkyPos::Big);
      break;
    case IconKind::PartlyCloudy:
      drawSkyLayer(s, kind, night, phase, SkyPos::Normal);
      drawCloud(s, 34, 46, 0.95f, CLOUD_LIGHT);
      break;
    case IconKind::Cloudy:
      drawSkyLayer(s, kind, night, phase, SkyPos::Normal);
      drawCloud(s, 22, 34, 1.15f, CLOUD_LIGHT);
      break;
    case IconKind::Overcast:
      drawSkyLayer(s, kind, night, phase, SkyPos::Corner);
      drawCloud(s, 40, 26, 0.8f, CLOUD_DARK);
      drawCloud(s, 24, 42, 1.05f, CLOUD_MID);
      break;
    case IconKind::Fog:
      drawSkyLayer(s, kind, night, phase, SkyPos::Normal);
      drawFog(s);
      break;
    default: {
      // Everything that falls from a cloud.
      drawSkyLayer(s, kind, night, phase, SkyPos::Corner);
      const bool dark = kind == IconKind::HeavyRain || kind == IconKind::Thunder;
      drawCloud(s, 30, 34, 0.98f, dark ? CLOUD_DARK : CLOUD_MID);

      if (kind == IconKind::Drizzle) {
        for (float x : {36.0f, 52.0f, 68.0f}) {
          s.fillSmoothCircle(x, 78, 2.4f, RAIN, theme::BG);
          s.fillSmoothCircle(x - 5, 88, 2.4f, RAIN, theme::BG);
        }
      } else if (kind == IconKind::Rain) {
        const float xs[] = {38, 54, 70};
        drawDrops(s, xs, 3, 72, 13);
      } else if (kind == IconKind::HeavyRain) {
        const float xs[] = {30, 42, 54, 66, 78};
        drawDrops(s, xs, 5, 72, 16);
      } else if (kind == IconKind::Snow) {
        drawFlake(s, 36, 78); drawFlake(s, 54, 76); drawFlake(s, 72, 78);
        drawFlake(s, 45, 90); drawFlake(s, 63, 90);
      } else if (kind == IconKind::Sleet) {
        const float xs[] = {36, 64};
        drawDrops(s, xs, 2, 72, 12);
        drawFlake(s, 50, 80); drawFlake(s, 78, 80); drawFlake(s, 42, 92);
      } else if (kind == IconKind::Thunder) {
        const float xs[] = {34, 74};
        drawDrops(s, xs, 2, 72, 12);
        drawBolt(s, 48, 66);
      }
      break;
    }
  }
}
}  // namespace

IconKind iconForCode(int code) {
  switch (code) {
    case 1000: return IconKind::Clear;
    case 1003: return IconKind::PartlyCloudy;
    case 1006: return IconKind::Cloudy;
    case 1009: return IconKind::Overcast;
    case 1030: case 1135: case 1147: return IconKind::Fog;
    case 1072: case 1150: case 1153: case 1168: case 1171: return IconKind::Drizzle;
    case 1063: case 1180: case 1183: case 1186: case 1189: case 1240: return IconKind::Rain;
    case 1192: case 1195: case 1243: case 1246: return IconKind::HeavyRain;
    case 1066: case 1114: case 1117: case 1210: case 1213: case 1216: case 1219: case 1222:
    case 1225: case 1255: case 1258: return IconKind::Snow;
    case 1069: case 1198: case 1201: case 1204: case 1207: case 1237: case 1249: case 1252:
    case 1261: case 1264: return IconKind::Sleet;
    case 1087: case 1273: case 1276: case 1279: case 1282: return IconKind::Thunder;
    default: return IconKind::Cloudy;  // unknown code: a neutral guess
  }
}

const char* iconName(IconKind kind) {
  static const char* const names[] = {"Clear", "Partly cloudy", "Cloudy", "Overcast", "Fog",
                                      "Drizzle", "Rain", "Heavy rain", "Snow", "Sleet", "Thunder"};
  const int i = static_cast<int>(kind);
  return (i >= 0 && i < static_cast<int>(IconKind::Count)) ? names[i] : "?";
}

void drawWeatherIcon(TFT_eSPI& tft, int x, int y, IconKind kind, bool night, float moonPhase,
                     int size) {
  if (size <= 0) size = ICON_SIZE;
  TFT_eSprite sprite(&tft);
  // Drawn at the design size, then nearest-neighbour scaled so a 40 px tile
  // icon is the same picture as the 72 px one on Home, just smaller.
  if (!sprite.createSprite(ICON_DESIGN, ICON_DESIGN)) {
    Serial.println("[icons] not enough memory for the icon sprite");
    return;
  }
  sprite.fillSprite(theme::BG);
  drawIcon(sprite, kind, night, moonPhase);

  TFT_eSprite scaled(&tft);
  TFT_eSprite* out = &sprite;
  if (size != ICON_DESIGN) {
    if (!scaled.createSprite(size, size)) {
      Serial.println("[icons] not enough memory to scale the icon");
      sprite.deleteSprite();
      return;
    }
    for (int dy = 0; dy < size; dy++) {
      const int sy = dy * ICON_DESIGN / size;
      for (int dx = 0; dx < size; dx++) {
        scaled.drawPixel(dx, dy, sprite.readPixel(dx * ICON_DESIGN / size, sy));
      }
    }
    sprite.deleteSprite();
    out = &scaled;
  }
  out->pushSprite(x, y);
  out->deleteSprite();
}
