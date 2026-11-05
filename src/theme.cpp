#include "theme.h"

namespace theme {
namespace {
uint8_t accentR = 0x00;
uint8_t accentG = 0xE5;
uint8_t accentB = 0xFF;
}  // namespace

void setAccent(uint32_t rgb24) {
  accentR = (rgb24 >> 16) & 0xFF;
  accentG = (rgb24 >> 8) & 0xFF;
  accentB = rgb24 & 0xFF;
}

uint32_t accentRgb() {
  return (uint32_t(accentR) << 16) | (uint32_t(accentG) << 8) | accentB;
}

uint16_t accent() { return rgb(accentR, accentG, accentB); }
}  // namespace theme
