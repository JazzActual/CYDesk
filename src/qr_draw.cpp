#include "qr_draw.h"

#include <qrcode.h>  // ESP-IDF's built-in QR generator

namespace {
// The generator hands the finished code to a plain callback function with no
// way to pass extra data, so the drawing target lives in these variables.
TFT_eSPI* target = nullptr;
int originX = 0, originY = 0, limit = 0, drawnSize = 0;

constexpr int QUIET_ZONE = 2;  // white border, in modules (scanners need some)

void drawModules(esp_qrcode_handle_t qr) {
  const int modules = esp_qrcode_get_size(qr);
  const int scale = limit / (modules + 2 * QUIET_ZONE);  // whole pixels per module
  if (scale < 1) return;
  drawnSize = (modules + 2 * QUIET_ZONE) * scale;

  target->fillRect(originX, originY, drawnSize, drawnSize, TFT_WHITE);
  const int left = originX + QUIET_ZONE * scale;
  const int top = originY + QUIET_ZONE * scale;
  for (int my = 0; my < modules; my++) {
    for (int mx = 0; mx < modules; mx++) {
      if (esp_qrcode_get_module(qr, mx, my)) {
        target->fillRect(left + mx * scale, top + my * scale, scale, scale, TFT_BLACK);
      }
    }
  }
}
}  // namespace

int drawQrCode(TFT_eSPI& tft, const char* text, int x, int y, int maxSize) {
  target = &tft;
  originX = x;
  originY = y;
  limit = maxSize;
  drawnSize = 0;

  esp_qrcode_config_t config = ESP_QRCODE_CONFIG_DEFAULT();
  config.display_func = drawModules;
  config.max_qrcode_version = 4;  // short URLs only; keeps memory use tiny
  config.qrcode_ecc_level = ESP_QRCODE_ECC_MED;
  esp_qrcode_generate(&config, text);
  return drawnSize;
}
