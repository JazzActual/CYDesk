// Weather icons, drawn from simple shapes (no image files).
//
// Each icon is a "sky" layer (sun by day, moon by night) with weather layers
// on top (clouds, rain, snow, fog, lightning), so the moon stays visible
// behind the weather at night.
//
// WeatherAPI has ~48 condition codes; iconForCode() groups them into the
// 11 kinds below.
#pragma once
#include <TFT_eSPI.h>

enum class IconKind {
  Clear, PartlyCloudy, Cloudy, Overcast, Fog,
  Drizzle, Rain, HeavyRain, Snow, Sleet, Thunder,
  Count  // number of kinds (for the icon test)
};

constexpr int ICON_SIZE = 96;  // icons are ICON_SIZE x ICON_SIZE pixels

IconKind iconForCode(int weatherApiCode);
const char* iconName(IconKind kind);

// Draws the icon with its top-left corner at (x, y).
// moonPhase: 0 = new moon, 0.25 = first quarter, 0.5 = full, 0.75 = last quarter.
// Uses a temporary 96x96 sprite (~18 KB) so the icon appears in one go.
void drawWeatherIcon(TFT_eSPI& tft, int x, int y, IconKind kind, bool night, float moonPhase);
