#include "touch.h"

#include <TFT_eSPI.h>
#if !defined(BOARD_CYD35) && !defined(BOARD_CYD40)
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "pins.h"
#endif

namespace {
constexpr uint32_t RELEASE_DEBOUNCE_MS = 60;  // must read "released" this long
constexpr uint32_t MIN_TAP_MS = 20;           // shorter than this = electrical noise
#if defined(BOARD_CYD35) || defined(BOARD_CYD40)
// Pressure above this counts as a finger. The display library uses the same
// number. A light tap on a resistive screen can sit lower; the log prints it.
constexpr uint16_t PRESS_Z = 600;
#else
// The 2.8" touch chip has its own SPI bus (VSPI), separate from the display.
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touchChip(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
#endif

// Rough map from the touch chip's raw ADC range into landscape pixels.
// Large hit targets (the Home stats strip) forgive a few dozen pixels of
// calibration drift across boards.
int16_t mapAxis(int32_t raw, int32_t lo, int32_t hi, int16_t outMax) {
  if (hi == lo) return 0;
  int32_t v = (raw - lo) * outMax / (hi - lo);
  if (v < 0) v = 0;
  if (v > outMax) v = outMax;
  return static_cast<int16_t>(v);
}
}  // namespace

void TouchInput::begin(TFT_eSPI& display) {
  display_ = &display;
#if !defined(BOARD_CYD35) && !defined(BOARD_CYD40)
  touchSpi.begin(PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  touchChip.begin(touchSpi);
  touchChip.setRotation(1);  // match the display's landscape rotation
#else
  // The 3.5" / 4.0" touch chip shares the display's SPI wires. TFT_eSPI already
  // started that bus; TOUCH_CS in platformio.ini is the touch chip's select.
  (void)display_;
#endif
}

bool TouchInput::screenPressed() {
#if defined(BOARD_CYD35) || defined(BOARD_CYD40)
  return display_->getTouchRawZ() > PRESS_Z;
#else
  return touchChip.touched();
#endif
}

void TouchInput::capturePoint() {
  if (!display_) {
    havePoint_ = false;
    return;
  }
  const int16_t w = display_->width();
  const int16_t h = display_->height();
#if defined(BOARD_CYD35) || defined(BOARD_CYD40)
  uint16_t rawX = 0, rawY = 0;
  display_->getTouchRaw(&rawX, &rawY);
  // Shared-SPI panels report portrait-oriented raw axes; swap into landscape.
  pointX_ = mapAxis(rawY, 200, 3800, w - 1);
  pointY_ = mapAxis(rawX, 200, 3800, h - 1);
  Serial.printf("[touch] press raw=%u,%u -> %d,%d pressure %u\n", rawX, rawY, pointX_,
                pointY_, display_->getTouchRawZ());
#else
  TS_Point p = touchChip.getPoint();
  // With setRotation(1) the chip's x/y already match landscape.
  pointX_ = mapAxis(p.x, 200, 3800, w - 1);
  pointY_ = mapAxis(p.y, 200, 3800, h - 1);
  Serial.printf("[touch] press raw x=%d y=%d -> %d,%d pressure=%d\n", p.x, p.y, pointX_,
                pointY_, p.z);
#endif
  havePoint_ = true;
}

bool TouchInput::tapPoint(int16_t& x, int16_t& y) const {
  if (!havePoint_) return false;
  x = pointX_;
  y = pointY_;
  return true;
}

TouchEvent TouchInput::poll() {
  const uint32_t now = millis();

  if (screenPressed()) {
    lastTouch_ = now;
    if (!down_) {
      // A new press has started.
      down_ = true;
      longFired_ = false;
      downAt_ = now;
      capturePoint();
    }
    if (!longFired_ && now - downAt_ >= LONG_PRESS_MS) {
      longFired_ = true;
      return TouchEvent::LongPress;
    }
    return TouchEvent::None;
  }

  // Screen reads "not pressed". Only treat it as a release after the debounce time.
  if (down_ && now - lastTouch_ >= RELEASE_DEBOUNCE_MS) {
    down_ = false;
    const uint32_t held = lastTouch_ - downAt_;
    if (!longFired_ && held >= MIN_TAP_MS && held <= MAX_TAP_MS) {
      return TouchEvent::Tap;
    }
  }
  return TouchEvent::None;
}
