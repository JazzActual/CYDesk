#include "portal.h"

#include <WiFiManager.h>

#include "theme.h"

namespace {
// Set by WiFiManager when the user presses Save on the form. Without this we
// couldn't tell "user unticked °F" from "portal timed out, nothing submitted".
bool formSubmitted = false;
void onFormSaved() { formSubmitted = true; }

void drawInstructions(TFT_eSPI& tft, const char* reason) {
  tft.fillScreen(theme::BG);
  tft.fillRect(0, 0, tft.width(), 3, theme::CYAN);

  tft.setTextDatum(TL_DATUM);  // top-left
  tft.setTextColor(theme::CYAN, theme::BG);
  tft.drawString(reason, 16, 14, 4);

  const int x = 16;
  int y = 56;
  auto line = [&](const char* label, const char* value) {
    tft.setTextColor(theme::MUTED, theme::BG);
    tft.drawString(label, x, y, 2);
    tft.setTextColor(theme::TEXT, theme::BG);
    tft.drawString(value, x + 24, y, 2);
    y += 26;
  };
  line("1.", "On your phone, join the Wi-Fi network:");
  tft.setTextColor(theme::PINK, theme::BG);
  tft.drawString(PORTAL_AP_NAME, x + 24, y - 6, 4);
  y += 26;
  line("2.", "A setup page should open by itself.");
  line("", "If not, browse to  192.168.4.1");
  line("3.", "Fill in the form and tap Save.");

  tft.setTextColor(theme::MUTED, theme::BG);
  tft.setTextDatum(BC_DATUM);
  char note[48];
  snprintf(note, sizeof(note), "Closes by itself after %d minutes", PORTAL_TIMEOUT_S / 60);
  tft.drawString(note, tft.width() / 2, tft.height() - 6, 2);
}
}  // namespace

bool hasSavedWifi() {
  WiFiManager wm;
  return wm.getWiFiIsSaved();
}

bool runSetupPortal(TFT_eSPI& tft, Settings& settings, const char* reason) {
  Serial.printf("[portal] opening (%s)\n", reason);
  drawInstructions(tft, reason);

  WiFiManager wm;
  wm.setTitle("CYDesk setup");
  wm.setClass("invert");                  // dark theme for the phone page
  std::vector<const char*> menu = {"wifi", "exit"};
  wm.setMenu(menu);                       // hide the advanced/debug buttons
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setBreakAfterConfig(true);           // return after Save even if Wi-Fi fails,
                                          // so the other settings still get stored
  wm.setSaveParamsCallback(onFormSaved);
  wm.setSaveConfigCallback(onFormSaved);

  // Our own fields, shown under the Wi-Fi fields on the same page.
  // (id, label, current value, max length[, extra HTML attributes, label position])
  WiFiManagerParameter nameField("name", "Your name (for the greeting)",
                                 settings.name.c_str(), 20);
  WiFiManagerParameter cityField("city", "City for weather (e.g. Chicago, IL)",
                                 settings.city.c_str(), 48);
  WiFiManagerParameter badgeField("badge", "Badge: paste the HTML embed code (or an image URL)",
                                  settings.badgeUrl.c_str(), 512);
  // A checkbox: sends "F" when ticked, nothing when unticked.
  WiFiManagerParameter unitsField("units", "Show temperatures in &deg;F (untick for &deg;C)", "F", 2,
                                  settings.fahrenheit ? "type='checkbox' checked" : "type='checkbox'",
                                  WFM_LABEL_AFTER);
  wm.addParameter(&nameField);
  wm.addParameter(&cityField);
  wm.addParameter(&badgeField);
  wm.addParameter(&unitsField);

  formSubmitted = false;
  // This blocks until Save, Exit or the timeout. An open network (no password)
  // keeps setup simple; the timeout stops it hanging around forever.
  const bool connected = wm.startConfigPortal(PORTAL_AP_NAME);

  if (formSubmitted) {
    String newCity = cityField.getValue();
    newCity.trim();
    if (newCity != settings.city) {
      // Different city: forget the old coordinates so they get looked up again.
      settings.latitude = NAN;
      settings.longitude = NAN;
      settings.placeLabel = "";
    }
    settings.city = newCity;
    settings.name = nameField.getValue();
    settings.name.trim();
    settings.badgeUrl = extractImageUrl(badgeField.getValue());
    settings.fahrenheit = strcmp(unitsField.getValue(), "F") == 0;
    settings.save();
    Serial.printf("[portal] saved: name='%s' city='%s' units=%s badge='%s'\n",
                  settings.name.c_str(), settings.city.c_str(),
                  settings.fahrenheit ? "F" : "C", settings.badgeUrl.c_str());
  } else {
    Serial.println("[portal] closed without saving");
  }

  Serial.printf("[portal] Wi-Fi %s\n", connected ? "connected" : "not connected");
  return connected;
}
