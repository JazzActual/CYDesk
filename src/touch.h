// Touch input: turns raw "is the screen pressed?" readings into events.
//
//   Tap        = press and release within MAX_TAP_MS       -> next page
//   LongPress  = keep pressing for LONG_PRESS_MS           -> setup portal
//
// Resistive screens are noisy: a press can "flicker" off for a few ms.
// We only count a release once the screen has read "not pressed" for
// RELEASE_DEBOUNCE_MS in a row.
//
// The press point is mapped into landscape screen pixels so Home can treat
// the top-right stats strip as three small buttons.
#pragma once
#include <Arduino.h>

class TFT_eSPI;

enum class TouchEvent { None, Tap, LongPress };

class TouchInput {
 public:
  static constexpr uint32_t LONG_PRESS_MS = 5000;
  static constexpr uint32_t MAX_TAP_MS = 800;

  // `display` is the screen already started with tft.init(). The 3.5" / 4.0"
  // resistive boards read the touch chip through that same connection. The
  // NiceMCU 2.8" capacitive board uses CST816D over I2C instead.
  void begin(TFT_eSPI& display);

  // Call every loop(). Returns at most one event per press.
  TouchEvent poll();

  // True while a finger is on the screen (after debouncing).
  bool isDown() const { return down_; }

  // How long the current press has lasted (0 if not pressed).
  uint32_t heldMs() const { return down_ ? millis() - downAt_ : 0; }

  // True once this press has already fired a LongPress.
  bool longPressFired() const { return longFired_; }

  // Screen coordinates of the current / just-finished press (landscape
  // pixels). Valid after a Tap. Returns false if the press had no point.
  bool tapPoint(int16_t& x, int16_t& y) const;

 private:
  bool down_ = false;
  bool longFired_ = false;
  bool havePoint_ = false;
  uint32_t downAt_ = 0;     // when the current press started
  uint32_t lastTouch_ = 0;  // last time we saw the screen pressed
  int16_t pointX_ = 0;
  int16_t pointY_ = 0;
  TFT_eSPI* display_ = nullptr;

  bool screenPressed();
  void capturePoint();
};
