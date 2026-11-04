#include "touch.h"

#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "pins.h"

namespace {
constexpr uint32_t RELEASE_DEBOUNCE_MS = 60;  // must read "released" this long
constexpr uint32_t MIN_TAP_MS = 20;           // shorter than this = electrical noise

// The touch chip gets its own SPI bus (VSPI) on its own pins.
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touchChip(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
}  // namespace

void TouchInput::begin() {
  touchSpi.begin(PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  touchChip.begin(touchSpi);
  touchChip.setRotation(1);  // match the display's landscape rotation
}

TouchEvent TouchInput::poll() {
  const uint32_t now = millis();

  if (touchChip.touched()) {
    lastTouch_ = now;
    if (!down_) {
      // A new press has started.
      down_ = true;
      longFired_ = false;
      downAt_ = now;
      // Raw coordinates (roughly 200..3900 on each axis). We don't need them
      // for "tap anywhere", but logging them now helps if we ever add buttons.
      TS_Point p = touchChip.getPoint();
      Serial.printf("[touch] press at raw x=%d y=%d pressure=%d\n", p.x, p.y, p.z);
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
