#include "touch.h"

#include <TFT_eSPI.h>
#if defined(BOARD_NICEMCU28) || defined(BOARD_CYD35C) || defined(BOARD_CYD40C)
#include <Wire.h>
#include "pins.h"
#elif !defined(BOARD_CYD35) && !defined(BOARD_CYD40)
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
#elif defined(BOARD_NICEMCU28)
// CST816D gesture/point register block (NiceMCU nicemcu_display.cpp).
constexpr uint8_t CST816_REG_DATA = 0x02;
bool touchReady_ = false;
uint16_t lastRawX_ = 0;
uint16_t lastRawY_ = 0;
bool lastPressed_ = false;

bool readCst816(uint8_t* packet, size_t length) {
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(CST816_REG_DATA);
  if (Wire.endTransmission(false) != 0) return false;
  const size_t got = Wire.requestFrom(TOUCH_I2C_ADDR, static_cast<uint8_t>(length));
  if (got != length) {
    while (Wire.available() > 0) Wire.read();
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    packet[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

// Returns true while a finger is down. Fills raw portrait coordinates when
// pressed (panel native 240×320).
bool cst816Pressed(uint16_t* rawX, uint16_t* rawY) {
  if (!touchReady_) return false;
  uint8_t packet[5] = {};
  if (!readCst816(packet, sizeof(packet))) return false;
  if (packet[0] == 0) return false;
  *rawX = static_cast<uint16_t>(((packet[1] & 0x0F) << 8) | packet[2]);
  *rawY = static_cast<uint16_t>(((packet[3] & 0x0F) << 8) | packet[4]);
  return true;
}
#elif defined(BOARD_CYD35C) || defined(BOARD_CYD40C)
// GT911 status + first touch point (Goodix). Polled; IRQ often unusable on
// stock ESP32-3248S035C boards.
constexpr uint16_t GT911_REG_PRODUCT_ID = 0x8140;
constexpr uint16_t GT911_REG_STATUS = 0x814E;
bool touchReady_ = false;
uint8_t touchAddr_ = TOUCH_I2C_ADDR_PRIMARY;
uint16_t lastRawX_ = 0;
uint16_t lastRawY_ = 0;
bool lastPressed_ = false;

bool gt911Write(uint16_t reg, const uint8_t* data, size_t length) {
  Wire.beginTransmission(touchAddr_);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  for (size_t i = 0; i < length; i++) Wire.write(data[i]);
  return Wire.endTransmission() == 0;
}

bool gt911Read(uint16_t reg, uint8_t* data, size_t length) {
  Wire.beginTransmission(touchAddr_);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  const size_t got = Wire.requestFrom(touchAddr_, static_cast<uint8_t>(length));
  if (got != length) {
    while (Wire.available() > 0) Wire.read();
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool gt911ProbeAddr(uint8_t addr) {
  touchAddr_ = addr;
  uint8_t id[4] = {};
  if (!gt911Read(GT911_REG_PRODUCT_ID, id, sizeof(id))) return false;
  // Product id ASCII is typically "911".
  return id[0] == '9' && id[1] == '1' && id[2] == '1';
}

bool gt911Pressed(uint16_t* rawX, uint16_t* rawY) {
  if (!touchReady_) return false;
  uint8_t packet[7] = {};
  if (!gt911Read(GT911_REG_STATUS, packet, sizeof(packet))) return false;
  const uint8_t status = packet[0];
  const uint8_t points = status & 0x0F;
  if (points == 0 || (status & 0x80) == 0) {
    if (points == 0 && (status & 0x80)) {
      const uint8_t zero = 0;
      gt911Write(GT911_REG_STATUS, &zero, 1);
    }
    return false;
  }
  *rawX = static_cast<uint16_t>(packet[2] | (static_cast<uint16_t>(packet[3]) << 8));
  *rawY = static_cast<uint16_t>(packet[4] | (static_cast<uint16_t>(packet[5]) << 8));
  const uint8_t zero = 0;
  gt911Write(GT911_REG_STATUS, &zero, 1);
  return true;
}
#else
// The 2.8" resistive touch chip has its own SPI bus (VSPI), separate from the
// display.
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

int16_t clampAxis(int32_t v, int16_t outMax) {
  if (v < 0) v = 0;
  if (v > outMax) v = outMax;
  return static_cast<int16_t>(v);
}
}  // namespace

void TouchInput::begin(TFT_eSPI& display) {
  display_ = &display;
#if defined(BOARD_CYD35C) || defined(BOARD_CYD40C)
  pinMode(PIN_TOUCH_IRQ, INPUT);
  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, LOW);
  delay(10);
  digitalWrite(PIN_TOUCH_RST, HIGH);
  delay(50);
  touchReady_ = Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, 400000);
  if (!touchReady_) {
    Serial.println("[touch] GT911 I2C bus failed to start");
  } else if (gt911ProbeAddr(TOUCH_I2C_ADDR_PRIMARY) || gt911ProbeAddr(TOUCH_I2C_ADDR_ALT)) {
    Serial.printf("[touch] GT911 present at 0x%02X\n", touchAddr_);
  } else {
    Serial.println("[touch] GT911 no ACK at 0x5D/0x14 (bus up, chip silent)");
    touchReady_ = false;
  }
#elif defined(BOARD_NICEMCU28)
  // Shared panel/touch reset already pulsed by tft.init() (TFT_RST=2).
  pinMode(PIN_TOUCH_IRQ, INPUT);
  touchReady_ = Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, 400000);
  if (!touchReady_) {
    Serial.println("[touch] CST816D I2C bus failed to start");
  } else {
    // Register 0xA7 is the CST816 family chip id.
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(0xA7);
    const uint8_t addrOk = Wire.endTransmission(false);
    const size_t got = (addrOk == 0) ? Wire.requestFrom(TOUCH_I2C_ADDR, (uint8_t)1) : 0;
    if (got == 1) {
      const uint8_t chipId = static_cast<uint8_t>(Wire.read());
      Serial.printf("[touch] CST816D present id=0x%02X (NiceMCU 2.8 C)\n", chipId);
    } else {
      Serial.printf("[touch] CST816D no ACK at 0x%02X (bus up, chip silent)\n",
                    TOUCH_I2C_ADDR);
      touchReady_ = false;
    }
  }
#elif !defined(BOARD_CYD35) && !defined(BOARD_CYD40)
  touchSpi.begin(PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  touchChip.begin(touchSpi);
  touchChip.setRotation(1);  // match the display's landscape rotation
#else
  // The 3.5" / 4.0" resistive touch chip shares the display's SPI wires.
  // TFT_eSPI already started that bus; TOUCH_CS in platformio.ini is the
  // touch chip's select.
  (void)display_;
#endif
}

bool TouchInput::screenPressed() {
#if defined(BOARD_CYD35C) || defined(BOARD_CYD40C)
  uint16_t x = 0, y = 0;
  const bool down = gt911Pressed(&x, &y);
  if (down) {
    lastRawX_ = x;
    lastRawY_ = y;
  }
  lastPressed_ = down;
  return down;
#elif defined(BOARD_NICEMCU28)
  uint16_t x = 0, y = 0;
  const bool down = cst816Pressed(&x, &y);
  if (down) {
    lastRawX_ = x;
    lastRawY_ = y;
  }
  lastPressed_ = down;
  return down;
#elif defined(BOARD_CYD35) || defined(BOARD_CYD40)
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
#if defined(BOARD_CYD35C) || defined(BOARD_CYD40C)
  // GT911 reports portrait panel pixels (320×480). Rotation 1 landscape is
  // 480×320: x = raw_y, y = (panel_w - 1) - raw_x.
  uint16_t rawX = lastRawX_;
  uint16_t rawY = lastRawY_;
  if (!lastPressed_) {
    if (!gt911Pressed(&rawX, &rawY)) {
      havePoint_ = false;
      return;
    }
  }
  pointX_ = clampAxis(rawY, w - 1);
  pointY_ = clampAxis(static_cast<int32_t>(TFT_WIDTH) - 1 - rawX, h - 1);
  Serial.printf("[touch] press raw=%u,%u -> %d,%d (gt911)\n", rawX, rawY, pointX_, pointY_);
#elif defined(BOARD_NICEMCU28)
  // CST816D reports portrait panel pixels (240×320). Display rotation 1 is
  // landscape 320×240: x = raw_y, y = (panel_w - 1) - raw_x.
  uint16_t rawX = lastRawX_;
  uint16_t rawY = lastRawY_;
  if (!lastPressed_) {
    if (!cst816Pressed(&rawX, &rawY)) {
      havePoint_ = false;
      return;
    }
  }
  // Panel native width is TFT_WIDTH (240); landscape height is that axis.
  pointX_ = clampAxis(rawY, w - 1);
  pointY_ = clampAxis(static_cast<int32_t>(TFT_WIDTH) - 1 - rawX, h - 1);
  Serial.printf("[touch] press raw=%u,%u -> %d,%d (cap)\n", rawX, rawY, pointX_, pointY_);
#elif defined(BOARD_CYD35) || defined(BOARD_CYD40)
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
