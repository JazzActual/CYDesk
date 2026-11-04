// CYDesk — Step 1: hello screen / display test
//
// What this checks, just by looking at the screen:
//   1. Driver:   if anything shows up at all, the ILI9341 driver is right.
//   2. Colours:  each bar's label must match its colour (RED bar says "RED").
//                Swapped red/blue  -> colour order is wrong.
//                Black looks white  -> colour inversion is wrong.
//   3. Rotation: "TOP" text should be at the top edge, the right way up.
//
// It also turns off the RGB LED, which otherwise glows dimly at boot.

#include <Arduino.h>
#include <TFT_eSPI.h>

// RGB LED pins. The LED is "common anode": writing HIGH turns a colour OFF.
constexpr int LED_R = 22;
constexpr int LED_G = 16;
constexpr int LED_B = 17;

// Landscape. If everything comes out upside down, change this to 3.
constexpr uint8_t SCREEN_ROTATION = 1;

TFT_eSPI tft;  // display driver, configured entirely by platformio.ini

void ledsOff() {
  for (int pin : {LED_R, LED_G, LED_B}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
}

// One vertical colour bar with its name written in the middle.
void drawBar(int index, int count, uint16_t colour, const char* name, uint16_t textColour) {
  const int barWidth = tft.width() / count;
  const int x = index * barWidth;
  tft.fillRect(x, 40, barWidth, tft.height() - 80, colour);
  tft.setTextColor(textColour, colour);
  tft.setTextDatum(MC_DATUM);  // MC = "middle centre": x,y is the text's centre
  tft.drawString(name, x + barWidth / 2, tft.height() / 2, 2);
}

void drawTestScreen() {
  tft.fillScreen(TFT_BLACK);

  // Five bars: if a label doesn't match its colour, the settings need a tweak.
  drawBar(0, 5, TFT_RED,   "RED",   TFT_WHITE);
  drawBar(1, 5, TFT_GREEN, "GREEN", TFT_BLACK);
  drawBar(2, 5, TFT_BLUE,  "BLUE",  TFT_WHITE);
  drawBar(3, 5, TFT_WHITE, "WHITE", TFT_BLACK);
  drawBar(4, 5, TFT_BLACK, "BLACK", TFT_WHITE);

  // Top / bottom labels to check rotation.
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);  // top centre
  tft.drawString("TOP - CYDesk hello!", tft.width() / 2, 8, 4);
  tft.setTextDatum(BC_DATUM);  // bottom centre
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  char info[48];
  snprintf(info, sizeof(info), "BOTTOM  %dx%d  rotation %d", tft.width(), tft.height(), SCREEN_ROTATION);
  tft.drawString(info, tft.width() / 2, tft.height() - 8, 2);
}

void setup() {
  ledsOff();
  Serial.begin(115200);
  Serial.println("\nCYDesk step 1: display test");

  tft.init();  // also switches the backlight on (TFT_BL in platformio.ini)
  tft.setRotation(SCREEN_ROTATION);
  drawTestScreen();

  Serial.printf("Screen is %d x %d. Free heap: %u bytes\n",
                tft.width(), tft.height(), ESP.getFreeHeap());
}

void loop() {
  // Nothing to do yet. A short pause keeps the CPU from spinning flat out.
  delay(100);
}
