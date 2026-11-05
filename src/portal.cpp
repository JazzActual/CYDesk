#include "portal.h"

#include <WiFiManager.h>

#include "theme.h"

namespace {
void drawInstructions(TFT_eSPI& tft, const char* reason) {
  tft.fillScreen(theme::BG);

  tft.setTextDatum(TL_DATUM);  // top-left
  tft.setTextColor(theme::accent(), theme::BG);
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
  line("2.", "A page opens (or browse to 192.168.4.1).");
  line("3.", "Pick your home Wi-Fi, enter its password.");
  line("", "Everything else comes after that.");

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

bool runWifiPortal(TFT_eSPI& tft, const char* reason) {
  Serial.printf("[portal] opening (%s)\n", reason);
  drawInstructions(tft, reason);

  WiFiManager wm;
  wm.setTitle("CYDesk Wi-Fi");
  wm.setClass("invert");                  // dark theme for the phone page
  std::vector<const char*> menu = {"wifi", "exit"};
  wm.setMenu(menu);                       // hide the advanced/debug buttons
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);

  // A phone checks a handful of "is there internet?" addresses before it
  // shows the setup page. Those used to miss every handler, so the phone
  // retried for several seconds. Answer them with a short page that opens
  // the setup menu. The network list is scanned in the background so that
  // page is ready when the phone asks for it.
  wm._preloadwifiscan = true;
  wm._asyncScan = true;
  wm.setWebServerCallback([&wm]() {
    const auto answer = [&wm]() {
      wm.server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      wm.server->send(200, "text/html",
                      "<!DOCTYPE html><html><head>"
                      "<meta http-equiv=\"refresh\" content=\"0;url=http://192.168.4.1/\">"
                      "<title>CYDesk</title></head><body>"
                      "<a href=\"http://192.168.4.1/\">Set up CYDesk Wi-Fi</a>"
                      "</body></html>");
    };
    const char* probes[] = {
        "/generate_204",
        "/gen_204",
        "/hotspot-detect.html",
        "/canonical.html",
        "/connecttest.txt",
        "/ncsi.txt",
        "/success.txt",
        "/redirect",
        "/fwlink",
    };
    for (const char* path : probes) wm.server->on(path, answer);
  });

  // Blocks until the board joins a network, Exit is pressed, or the timeout.
  // An open network (no password) keeps setup simple; the timeout stops it
  // hanging around forever.
  const bool connected = wm.startConfigPortal(PORTAL_AP_NAME);
  Serial.printf("[portal] Wi-Fi %s\n", connected ? "connected" : "not connected");
  return connected;
}
