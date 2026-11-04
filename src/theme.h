// Shared colours, taken from the WDGWars card so every page matches it.
// The display uses 16-bit "RGB565" colours; color565() converts from normal RGB.
#pragma once
#include <TFT_eSPI.h>

namespace theme {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t BG     = rgb(0x0a, 0x10, 0x18);  // dark navy background
constexpr uint16_t PANEL  = rgb(0x0f, 0x25, 0x31);  // slightly lighter panels/tracks
constexpr uint16_t TEXT   = rgb(0xe6, 0xed, 0xf3);  // main text (off-white)
constexpr uint16_t MUTED  = rgb(0x5d, 0x7b, 0x8c);  // small labels (grey-blue)
constexpr uint16_t CYAN   = rgb(0x00, 0xe5, 0xff);  // accent
constexpr uint16_t PINK   = rgb(0xff, 0x2d, 0x95);  // second accent
}  // namespace theme
