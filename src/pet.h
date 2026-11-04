// Desk pet, bottom left of the Home page.
//
// A 20×20 animal on a 26×26 canvas (room for the umbrella and "z z"), drawn
// at 3× (78 px). Pixel art matches the approved preview.
//
// Reactions, highest priority first:
//   asleep at night > umbrella (rain/snow chance over 50%) >
//   sunglasses (clear by day) > idle (a short blink).
// "Idle" mode always stays on idle. "Off", or animal "None", draws nothing.
#pragma once
#include <TFT_eSPI.h>

#include "settings.h"

namespace pet {
// Paints the pet (or clears its corner if it's turned off).
void draw(TFT_eSPI& tft, const Settings& settings);

// Redraws only the pet's corner when the reaction or blink frame changes.
void update(TFT_eSPI& tft, const Settings& settings);
}  // namespace pet
