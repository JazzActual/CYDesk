#include "rgb_led.h"

#include <math.h>

#include "pins.h"

namespace rgb_led {
namespace {

// LEDC (the ESP32's PWM block), Arduino-ESP32 2.x API. Nothing else in this
// firmware uses LEDC; the high channels keep clear of any library that grabs
// channel 0. 5 kHz is well above flicker; 10 bits gives smooth dim colors.
constexpr uint8_t CHANNELS[3] = {13, 14, 15};
constexpr int PINS[3] = {PIN_LED_R, PIN_LED_G, PIN_LED_B};
constexpr uint32_t PWM_HZ = 5000;
constexpr uint8_t PWM_BITS = 10;
constexpr uint32_t PWM_MAX = (1u << PWM_BITS) - 1;

bool attached = false;
const Settings* saved = nullptr;
uint32_t previewStarted = 0;
bool previewing = false;

void allOff() {
  for (int i = 0; i < 3; i++) {
    if (attached) ledcDetachPin(PINS[i]);
    pinMode(PINS[i], OUTPUT);
    digitalWrite(PINS[i], HIGH);  // common anode: HIGH = dark
  }
  attached = false;
}

// 0-255 channel value and 0-100 brightness -> PWM "on" level. A 2.2 gamma
// curve so the brightness slider and mid colors look even to the eye
// instead of everything above ~30% looking the same.
uint32_t level(uint8_t channel, uint8_t brightnessPercent) {
  const float x = (channel / 255.0f) * (brightnessPercent / 100.0f);
  return static_cast<uint32_t>(lroundf(powf(x, 2.2f) * PWM_MAX));
}

void show(bool on, uint32_t rgb24, uint8_t brightnessPercent) {
  if (brightnessPercent > 100) brightnessPercent = 100;
  const uint8_t rgb[3] = {static_cast<uint8_t>((rgb24 >> 16) & 0xFF),
                          static_cast<uint8_t>((rgb24 >> 8) & 0xFF),
                          static_cast<uint8_t>(rgb24 & 0xFF)};
  uint32_t lit[3];
  for (int i = 0; i < 3; i++) lit[i] = on ? level(rgb[i], brightnessPercent) : 0;
  if (lit[0] == 0 && lit[1] == 0 && lit[2] == 0) {
    allOff();  // plain GPIO HIGH: no PWM glow at all
    return;
  }
  if (!attached) {
    for (int i = 0; i < 3; i++) {
      ledcSetup(CHANNELS[i], PWM_HZ, PWM_BITS);
      ledcAttachPin(PINS[i], CHANNELS[i]);
    }
    attached = true;
  }
  // Common anode: the LED is lit while the pin is LOW, so invert the duty.
  for (int i = 0; i < 3; i++) ledcWrite(CHANNELS[i], PWM_MAX - lit[i]);
}

}  // namespace

void begin() {
  attached = false;
  allOff();
}

void apply(const Settings& settings) {
  saved = &settings;
  previewing = false;
  show(settings.ledOn, settings.ledColor, settings.ledBrightness);
}

void preview(bool on, uint32_t rgb24, uint8_t brightnessPercent) {
  previewing = true;
  previewStarted = millis();
  show(on, rgb24, brightnessPercent);
}

void handle() {
  if (previewing && saved && millis() - previewStarted >= PREVIEW_MS) apply(*saved);
}

}  // namespace rgb_led
