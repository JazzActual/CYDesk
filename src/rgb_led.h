// The RGB LED on the back of the board. Common anode on every CYD (HIGH =
// off), so the PWM duty is inverted. Pins come from pins.h per board.
//
// Color and brightness are set on the settings page. While the color picker
// moves, the page sends previews; an unsaved preview goes back to the saved
// color after PREVIEW_MS.
#pragma once
#include <Arduino.h>

#include "settings.h"

namespace rgb_led {

constexpr uint32_t PREVIEW_MS = 2 * 60 * 1000;

// Drives all three pins high (LED dark). Safe to call before anything else.
void begin();

// Shows the saved setting (and ends any preview).
void apply(const Settings& settings);

// Shows a color without saving it. Call handle() so it can time out.
void preview(bool on, uint32_t rgb24, uint8_t brightnessPercent);

// Call every loop(): puts the saved color back after an unsaved preview.
void handle();

}  // namespace rgb_led
