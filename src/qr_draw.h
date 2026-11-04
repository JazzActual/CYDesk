// Draws a QR code on the screen, using the QR generator that comes built into
// the ESP32 software (ESP-IDF "qrcode" component), so no extra library needed.
#pragma once
#include <TFT_eSPI.h>

// Draws `text` as a QR code with its top-left corner at (x, y), as large as
// fits in `maxSize` x `maxSize` pixels (dark modules on a white background,
// including the white border scanners need). Returns the size drawn, or 0 on failure.
int drawQrCode(TFT_eSPI& tft, const char* text, int x, int y, int maxSize);
