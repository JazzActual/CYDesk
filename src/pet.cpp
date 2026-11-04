#include "pet.h"

#include <time.h>

#include "theme.h"
#include "weather_icons.h"
#include "weather_service.h"

namespace pet {
namespace {

constexpr int CANVAS = 26;
constexpr int SCALE = 3;
constexpr int SIZE = CANVAS * SCALE;  // 78 px on screen
constexpr int PET_X = 8;
constexpr int PET_Y = 240 - SIZE - 4;
constexpr int ANIMAL_X = 1, ANIMAL_Y = 6;  // 20×20 animal on the canvas

constexpr uint16_t SHADES = theme::rgb(16, 16, 20);
constexpr uint16_t SHADES_SHINE = theme::rgb(120, 200, 255);
constexpr uint16_t SHADES_RIM = theme::rgb(200, 210, 220);
constexpr uint16_t UMBRELLA = theme::rgb(255, 45, 149);
constexpr uint16_t UMBRELLA_DARK = theme::rgb(190, 30, 110);
constexpr uint16_t HANDLE = theme::rgb(200, 200, 210);
constexpr uint16_t ZZZ = theme::rgb(140, 170, 200);
constexpr uint16_t EYE_CLOSED = theme::rgb(20, 14, 14);

enum class Look { Idle, Sunny, Umbrella, Asleep };

struct Animal {
  const char* const* rows;  // 20 strings, 20 chars, '.' = empty
  const char* letters;      // palette keys, same order as colours
  const uint16_t* colours;
  int colourCount;
  uint8_t eyes[16];  // eight x,y pairs in animal coordinates
  char closedFur;   // colour painted over the eyes when they shut
};

// Palettes. Letters match the preview script.
constexpr char FOX_LETTERS[] = "kosdwne";
constexpr uint16_t FOX_COLOURS[] = {
    theme::rgb(52, 26, 18), theme::rgb(238, 124, 46), theme::rgb(196, 92, 34),
    theme::rgb(74, 40, 30), theme::rgb(247, 239, 226), theme::rgb(20, 14, 14),
    theme::rgb(20, 14, 14),
};
constexpr const char* FOX_ROWS[] = {
    "....................", ".k................k.", ".kk..............kk.",
    ".kdk............kdk.", ".kddk..........kddk.", ".kodsk........ksdok.",
    ".koosokkkkkkkkosook.", ".kooooooooooooooook.", "kooooooooooooooooook",
    "kooeeooooooooooeeook", "kooeeooooooooooeeook", "kwwoooooooooooooowwk",
    ".kwwwwwwwwwwwwwwwwk.", "..kwwwwwwnnwwwwwwk..", "...kkwwwwwwwwwwkk.k.",
    "...koowwwwwwwwookkok", "..kooowwwwwwwwookoow", "..kooowwwwwwwwookosw",
    "..kdddkwwwwwwkdkooww", "...kkk.kkkkkk.kkkkkk",
};

constexpr char RACCOON_LETTERS[] = "kghlmwenr";
constexpr uint16_t RACCOON_COLOURS[] = {
    theme::rgb(30, 30, 38),  theme::rgb(148, 150, 162), theme::rgb(112, 114, 128),
    theme::rgb(214, 216, 222), theme::rgb(46, 46, 58), theme::rgb(246, 246, 248),
    theme::rgb(12, 12, 14),  theme::rgb(20, 20, 24), theme::rgb(70, 70, 84),
};
constexpr const char* RACCOON_ROWS[] = {
    "....................", "....................", "..kkk..........kkk..",
    ".khhgk........kghhk.", ".khggkkkkkkkkkkgghk.", ".kggggggggggggggggk.",
    "kgwwwggggggggggwwwgk", "kwwwwwwwggggwwwwwwwk", "kmmmmmmwlgglwmmmmmmk",
    "kmmeemmwllllwmmeemmk", "kmmeemmwllllwmmeemmk", "kmmmmmwwllllwwmmmmmk",
    ".kwwwwwwwwwwwwwwwwk.", "..kwwwwwwnnwwwwwwk..", "...kkwwwwwwwwwwkk.k.",
    "...kgggllllllgggkkgk", "..kggggllllllgggkrrk", "..kggggllllllgggkggk",
    "..khhhkllllllkhkrrk.", "...kkk.kkkkkk.kkkk..",
};

constexpr char OWL_LETTERS[] = "kbctyewav";
constexpr uint16_t OWL_COLOURS[] = {
    theme::rgb(44, 30, 22), theme::rgb(146, 100, 62), theme::rgb(112, 74, 44),
    theme::rgb(230, 206, 164), theme::rgb(250, 214, 92), theme::rgb(16, 12, 10),
    theme::rgb(255, 255, 255), theme::rgb(240, 150, 50), theme::rgb(196, 160, 116),
};
constexpr const char* OWL_ROWS[] = {
    "....................", "....................", ".kk..............kk.",
    ".kbk............kbk.", ".kbbkkkkkkkkkkkkbbk.", ".kbbbbbbbbbbbbbbbbk.",
    "kbbbbbbbbbbbbbbbbbbk", "kbkkkkbbbbbbbbkkkkbk", "kbkyyykbbbbbbkyyykbk",
    "kkyyweykbbbbkyewyykk", "kkyyeeykbbbbkyeeyykk", "kbkyyykbbaabbkyyykbk",
    "kbbkkkbbbaabbbkkkbbk", "kcbbbbbttttttbbbbbck", "kcbbbttvttttvttbbbck",
    "kcbbttttvttvttttbbck", "kcbbtvttttttttvtbbck", ".kcbbttttttttttbbck.",
    "..kkakkakkkkakkakk..", "....................",
};

const Animal ANIMALS[] = {
    {FOX_ROWS, FOX_LETTERS, FOX_COLOURS, 7, {3, 9, 4, 9, 3, 10, 4, 10, 15, 9, 16, 9, 15, 10, 16, 10}, 'o'},
    {RACCOON_ROWS, RACCOON_LETTERS, RACCOON_COLOURS, 9,
     {3, 9, 4, 9, 3, 10, 4, 10, 15, 9, 16, 9, 15, 10, 16, 10}, 'm'},
    {OWL_ROWS, OWL_LETTERS, OWL_COLOURS, 9, {4, 9, 5, 9, 4, 10, 5, 10, 14, 9, 15, 9, 14, 10, 15, 10}, 'y'},
};

bool visible(const Settings& s) {
  return s.petMode != static_cast<uint8_t>(PetMode::Off) &&
         s.petAnimal != static_cast<uint8_t>(PetAnimal::None) &&
         s.petAnimal <= static_cast<uint8_t>(PetAnimal::Owl);
}

const Animal& animalFor(const Settings& s) {
  const int i = s.petAnimal - 1;  // Fox = 1
  return ANIMALS[i];
}

uint16_t colourFor(const Animal& a, char ch) {
  for (int i = 0; i < a.colourCount; i++) {
    if (a.letters[i] == ch) return a.colours[i];
  }
  return theme::BG;
}

Look lookFor(const Settings& s) {
  if (s.petMode == static_cast<uint8_t>(PetMode::Idle)) return Look::Idle;
  const WeatherReport& w = weather::current();
  if (!w.ok) return Look::Idle;
  if (!w.isDay) return Look::Asleep;
  if (w.precipChanceAt(time(nullptr)) > 50) return Look::Umbrella;
  if (iconForCode(w.conditionCode) == IconKind::Clear && w.isDay) return Look::Sunny;
  return Look::Idle;
}

// Blink: eyes shut for a moment, then open again. Only the idle look blinks;
// sunglasses, the umbrella and sleep already cover the eyes.
bool eyesShut(Look look) {
  if (look != Look::Idle) return false;
  static uint32_t shutUntil = 0;
  static uint32_t nextShut = 0;
  const uint32_t now = millis();
  if (shutUntil == 0 && nextShut == 0) nextShut = now + 1800;
  if ((int32_t)(now - shutUntil) < 0) return true;
  if ((int32_t)(now - nextShut) >= 0) {
    shutUntil = now + 160;
    nextShut = now + 1800;
    return true;
  }
  return false;
}

void stamp(TFT_eSprite& sprite, int x, int y, uint16_t colour) {
  if (x < 0 || y < 0 || x >= CANVAS || y >= CANVAS) return;
  sprite.fillRect(x * SCALE, y * SCALE, SCALE, SCALE, colour);
}

void drawUmbrella(TFT_eSprite& sprite) {
  // Handle on the animal's right; canopy centred on that handle (clips the edge).
  constexpr int hx = 22, top = 1, r = 12;
  for (int y = top; y < top + 7; y++) {
    const int dy = y - top - 6;
    for (int x = 0; x < CANVAS; x++) {
      if ((x - hx) * (x - hx) * 36 + dy * dy * (r * r) <= (r * r) * 36) {
        const uint16_t c = ((x - hx + 24) / 4) % 2 == 0 ? UMBRELLA : UMBRELLA_DARK;
        stamp(sprite, x, y, c);
      }
    }
  }
  stamp(sprite, hx, 0, HANDLE);
  for (int y = top + 7; y < 22; y++) stamp(sprite, hx, y, HANDLE);
  stamp(sprite, hx - 1, 22, HANDLE);
  stamp(sprite, hx - 2, 22, HANDLE);
  stamp(sprite, hx - 2, 21, HANDLE);
}

void drawShades(TFT_eSprite& sprite, const Animal& a) {
  // Same geometry as the preview: eyes ±2 wide, two rows above the eyes,
  // light rim on the top edge so they read on the raccoon's mask.
  int minY = 99, maxY = -1, left0 = 99, left1 = -1, right0 = 99, right1 = -1;
  for (int i = 0; i < 8; i++) {
    const int x = a.eyes[i * 2], y = a.eyes[i * 2 + 1];
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
    if (x < 10) {
      if (x < left0) left0 = x;
      if (x > left1) left1 = x;
    } else {
      if (x < right0) right0 = x;
      if (x > right1) right1 = x;
    }
  }
  const int y0 = minY, y1 = maxY;
  const int lx0 = left0 - 2, lx1 = left1 + 2, rx0 = right0 - 2, rx1 = right1 + 2;
  const int bridgeY = y0 - 1;
  for (int y = y0 - 2; y < y1 + 2; y++) {
    for (int x = lx0; x <= lx1; x++) stamp(sprite, ANIMAL_X + x, ANIMAL_Y + y, SHADES);
    for (int x = rx0; x <= rx1; x++) stamp(sprite, ANIMAL_X + x, ANIMAL_Y + y, SHADES);
  }
  for (int x = lx1 + 1; x < rx0; x++) {
    stamp(sprite, ANIMAL_X + x, ANIMAL_Y + bridgeY, SHADES);
    stamp(sprite, ANIMAL_X + x, ANIMAL_Y + bridgeY + 1, SHADES);
  }
  for (int x = lx0; x <= lx1; x++) stamp(sprite, ANIMAL_X + x, ANIMAL_Y + (y0 - 2), SHADES_RIM);
  for (int x = rx0; x <= rx1; x++) stamp(sprite, ANIMAL_X + x, ANIMAL_Y + (y0 - 2), SHADES_RIM);
  for (int x = lx1 + 1; x < rx0; x++) stamp(sprite, ANIMAL_X + x, ANIMAL_Y + bridgeY, SHADES_RIM);
  stamp(sprite, ANIMAL_X + lx0, ANIMAL_Y + (y0 - 2), SHADES_SHINE);
  stamp(sprite, ANIMAL_X + rx0, ANIMAL_Y + (y0 - 2), SHADES_SHINE);
}

void drawZ(TFT_eSprite& sprite, int x0, int y0, int s) {
  for (int i = 0; i < s; i++) {
    stamp(sprite, x0 + i, y0, ZZZ);
    stamp(sprite, x0 + i, y0 + s - 1, ZZZ);
    stamp(sprite, x0 + s - 1 - i, y0 + i, ZZZ);
  }
}

void paint(TFT_eSPI& tft, const Settings& s, Look look, bool shut) {
  TFT_eSprite sprite(&tft);
  if (!sprite.createSprite(SIZE, SIZE)) {
    Serial.println("[pet] not enough memory for the pet sprite");
    return;
  }
  sprite.fillSprite(theme::BG);

  const Animal& a = animalFor(s);
  if (look == Look::Umbrella) drawUmbrella(sprite);

  for (int y = 0; y < 20; y++) {
    for (int x = 0; x < 20; x++) {
      const char ch = a.rows[y][x];
      if (ch != '.') stamp(sprite, ANIMAL_X + x, ANIMAL_Y + y, colourFor(a, ch));
    }
  }

  if (look == Look::Sunny) drawShades(sprite, a);

  if (look == Look::Asleep || shut) {
    const uint16_t fur = colourFor(a, a.closedFur);
    int lower = -1;
    for (int i = 0; i < 8; i++) {
      const int y = a.eyes[i * 2 + 1];
      if (y > lower) lower = y;
    }
    for (int i = 0; i < 8; i++) {
      const int x = a.eyes[i * 2], y = a.eyes[i * 2 + 1];
      stamp(sprite, ANIMAL_X + x, ANIMAL_Y + y, y == lower ? EYE_CLOSED : fur);
    }
  }
  if (look == Look::Asleep) {
    drawZ(sprite, 19, 6, 3);
    drawZ(sprite, 21, 2, 4);
  }

  sprite.pushSprite(PET_X, PET_Y);
  sprite.deleteSprite();
}

char shown[8] = "";

void makeSignature(char out[8], const Settings& s, Look look, bool shut) {
  // animal, look (i/s/u/a), blink (b = eyes shut, o = open). "off" = hidden.
  static const char LOOK[] = "isua";
  snprintf(out, 8, "%u%c%c", s.petAnimal, LOOK[static_cast<int>(look)], shut ? 'b' : 'o');
}

}  // namespace

void draw(TFT_eSPI& tft, const Settings& settings) {
  shown[0] = '\0';  // force a fresh paint
  update(tft, settings);
}

void update(TFT_eSPI& tft, const Settings& settings) {
  if (!visible(settings)) {
    if (strcmp(shown, "off") != 0) {
      tft.fillRect(PET_X, PET_Y, SIZE, SIZE, theme::BG);
      strncpy(shown, "off", sizeof(shown));
    }
    return;
  }
  const Look look = lookFor(settings);
  const bool shut = eyesShut(look);
  char sig[8];
  makeSignature(sig, settings, look, shut);
  if (strcmp(sig, shown) == 0) return;
  strncpy(shown, sig, sizeof(shown));
  paint(tft, settings, look, shut);
}

}  // namespace pet
