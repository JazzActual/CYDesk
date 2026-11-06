// Pin assignments. The display pins live in platformio.ini because TFT_eSPI
// reads them from there. Which board is selected with the build env:
//   e32r28t  2.8"  separate touch wires
//   cyd35    3.5"  touch shares the display's SPI wires (see touch.cpp)
//   cyd40    4.0"  touch shares the display's SPI wires (see touch.cpp)
#pragma once

#if defined(BOARD_CYD35)
// RGB LED, common anode: HIGH = off, LOW = on. Red is GPIO 4 on this board.
constexpr int PIN_LED_R = 4;
constexpr int PIN_LED_G = 16;
constexpr int PIN_LED_B = 17;
#elif defined(BOARD_CYD40)
// E32R40T: red is GPIO 22 (same as the 2.8"), green 16, blue 17.
constexpr int PIN_LED_R = 22;
constexpr int PIN_LED_G = 16;
constexpr int PIN_LED_B = 17;
#else
// Touch controller (XPT2046) on the 2.8" board. Its own SPI wires.
constexpr int PIN_TOUCH_SCK  = 25;
constexpr int PIN_TOUCH_MOSI = 32;
constexpr int PIN_TOUCH_MISO = 39;
constexpr int PIN_TOUCH_CS   = 33;
constexpr int PIN_TOUCH_IRQ  = 36;  // goes LOW while the screen is pressed

// RGB LED, common anode: HIGH = off, LOW = on.
constexpr int PIN_LED_R = 22;
constexpr int PIN_LED_G = 16;
constexpr int PIN_LED_B = 17;
#endif
