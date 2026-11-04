// Pin assignments for the E32R28T board (from the LCDwiki/Elecrow spec sheet).
// The display pins live in platformio.ini because TFT_eSPI reads them from there.
#pragma once

// Touch controller (XPT2046). It has its own SPI wires, separate from the display.
constexpr int PIN_TOUCH_SCK  = 25;
constexpr int PIN_TOUCH_MOSI = 32;
constexpr int PIN_TOUCH_MISO = 39;
constexpr int PIN_TOUCH_CS   = 33;
constexpr int PIN_TOUCH_IRQ  = 36;  // goes LOW while the screen is pressed

// RGB LED, common anode: HIGH = off, LOW = on.
constexpr int PIN_LED_R = 22;
constexpr int PIN_LED_G = 16;
constexpr int PIN_LED_B = 17;
