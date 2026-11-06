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

TouchEvent TouchInput::poll() {
  const uint32_t now = millis();

  if (screenPressed()) {
    lastTouch_ = now;
    if (!down_) {
      // A new press has started.
      down_ = true;
      longFired_ = false;
      downAt_ = now;
#if defined(BOARD_CYD35) || defined(BOARD_CYD40)
      Serial.printf("[touch] press, pressure %u\n", display_->getTouchRawZ());
#else
      // Raw coordinates (roughly 200..3900 on each axis). We don't need them
      // for "tap anywhere", but logging them now helps if we ever add buttons.
      TS_Point p = touchChip.getPoint();
      Serial.printf("[touch] press at raw x=%d y=%d pressure=%d\n", p.x, p.y, p.z);
#endif
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
