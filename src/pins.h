// Pin assignments. The display pins live in platformio.ini because TFT_eSPI
// reads them from there. Which board is selected with the build env:
//   e32r28t    2.8" resistive   XPT2046 on its own SPI wires
//   nicemcu28  2.8" capacitive  CST816D on I2C (NiceMCU-32S-DEV_2.8IPS)
//   cyd35      3.5" resistive   XPT2046 shares the display SPI wires
//   cyd35c     3.5" capacitive  GT911 on I2C (ESP32-3248S035C)
//   cyd40      4.0" resistive   XPT2046 shares the display SPI wires
//   cyd40c     4.0" capacitive  GT911 on I2C (classic-ESP32 ST7796 clones)
#pragma once

#if defined(BOARD_CYD35) || defined(BOARD_CYD35C)
// RGB LED, common anode: HIGH = off, LOW = on. Red is GPIO 4 on the 3.5".
constexpr int PIN_LED_R = 4;
constexpr int PIN_LED_G = 16;
constexpr int PIN_LED_B = 17;
#elif defined(BOARD_CYD40) || defined(BOARD_CYD40C)
// 4.0": red is GPIO 22 (same as the 2.8"), green 16, blue 17.
constexpr int PIN_LED_R = 22;
constexpr int PIN_LED_G = 16;
constexpr int PIN_LED_B = 17;
#endif

#if defined(BOARD_CYD35C) || defined(BOARD_CYD40C)
// GT911 capacitive touch on I2C (Sunton ESP32-3248S035C pinout). INT is often
// tied wrong on stock 3.5" C boards, so firmware polls over I2C.
constexpr int PIN_TOUCH_SDA = 33;
constexpr int PIN_TOUCH_SCL = 32;
constexpr int PIN_TOUCH_RST = 25;
constexpr int PIN_TOUCH_IRQ = 21;  // may be grounded; polling does not need it
constexpr uint8_t TOUCH_I2C_ADDR_PRIMARY = 0x5D;
constexpr uint8_t TOUCH_I2C_ADDR_ALT = 0x14;
#elif defined(BOARD_NICEMCU28)
// Capacitive touch (CST816D) on I2C. Reset is shared with the ST7789 (GPIO 2
// in platformio.ini); TFT_eSPI pulses it during tft.init().
constexpr int PIN_TOUCH_SDA = 33;
constexpr int PIN_TOUCH_SCL = 32;
constexpr int PIN_TOUCH_IRQ = 36;  // input; firmware polls I2C, not the IRQ edge
constexpr uint8_t TOUCH_I2C_ADDR = 0x15;

// Same RGB LED GPIOs as the classic 2.8" CYD when the pins are wired; writing
// HIGH (off) is harmless if this board leaves them unconnected.
constexpr int PIN_LED_R = 22;
constexpr int PIN_LED_G = 16;
constexpr int PIN_LED_B = 17;
#elif !defined(BOARD_CYD35) && !defined(BOARD_CYD40)
// Touch controller (XPT2046) on the 2.8" resistive board. Its own SPI wires.
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
