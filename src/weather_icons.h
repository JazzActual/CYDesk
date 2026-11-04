// Weather icons, drawn from simple shapes (no image files).
//
// Clear and partly cloudy show a sun by day. The other daytime icons are just
// the weather, so a sun in the corner does not read as a break in the clouds.
// Every icon shows the moon at night, with the weather drawn on top.
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

constexpr int ICON_SIZE = 72;   // on-screen size (width and height)
constexpr int ICON_DESIGN = 96; // internal draw size; scaled down when pushed

IconKind iconForCode(int weatherApiCode);
const char* iconName(IconKind kind);

// Draws the icon with its top-left corner at (x, y).
// moonPhase: 0 = new moon, 0.25 = first quarter, 0.5 = full, 0.75 = last quarter.
// size: on-screen width and height. Home uses ICON_SIZE (72); the hourly tiles use 40.
// Uses a temporary ICON_DESIGN×ICON_DESIGN sprite (~18 KB), then scales down.
void drawWeatherIcon(TFT_eSPI& tft, int x, int y, IconKind kind, bool night, float moonPhase,
                     int size = ICON_SIZE);
