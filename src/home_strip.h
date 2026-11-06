// Home top-right strip: three lines from one stats source (WDGWars, WiGLE, or
// Wardrift). The source and each line's field are saved in Settings.
#pragma once
#include <Arduino.h>

#include "settings.h"

namespace home_strip {

constexpr int LINES = 3;

// How many fields the source offers (0 when Off).
uint8_t fieldCount(HomeStripSource source);

// Short label drawn on Home (fits the strip column).
const char* fieldLabel(HomeStripSource source, uint8_t field);

// Longer name for the settings dropdown.
const char* fieldTitle(HomeStripSource source, uint8_t field);

// Formatted value for Home, or "—" when that account isn't loaded yet.
String formatValue(HomeStripSource source, uint8_t field);

// Colour hint: pink for ranks, text otherwise. MUTED when empty.
uint16_t valueColour(HomeStripSource source, uint8_t field, bool haveValue);

// Page index to open on a strip tap, or -1 when the strip is off.
int pageIndex(HomeStripSource source);

// Keep source/fields inside the valid ranges after load or a form save.
void clamp(Settings& settings);

}  // namespace home_strip
