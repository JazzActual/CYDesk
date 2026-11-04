#include "wifi_link.h"

#include <WiFi.h>

#include "esp_wifi.h"

namespace {

// How long to stay offline before we give up on the radio's own retry and
// look for a different node. The driver's retry often keeps the old address.
constexpr uint32_t RETRY_AFTER_DOWN_MS = 10000;
// A full scan takes a couple of seconds. Don't start another one immediately.
constexpr uint32_t RETRY_GAP_MS = 20000;

bool eventsHooked = false;
// millis() of the last connect or drop. 0 means nothing new for the screen.
volatile uint32_t wifiChangedAt = 0;
// Let a burst of reconnect events finish before the screen is repainted.
constexpr uint32_t WIFI_SETTLE_MS = 400;

// The saved config can have a radio address and channel filled in after a
// successful join. Clear both so the next connect may use any node.
void clearNodeLock() {
  wifi_config_t conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) return;
  if (conf.sta.ssid[0] == 0) return;

  conf.sta.bssid_set = 0;
  memset(conf.sta.bssid, 0, sizeof(conf.sta.bssid));
  conf.sta.channel = 0;
  conf.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  conf.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  esp_wifi_set_config(WIFI_IF_STA, &conf);
}

void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    // Reason is a number from the Wi-Fi standard (8 = the access point told
    // us to leave, 200 = its signal disappeared). Logged so the next report
    // says why, without the network name.
    Serial.printf("[wifi] disconnected, reason %u\n", info.wifi_sta_disconnected.reason);
    wifiChangedAt = millis();
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.printf("[wifi] connected, rssi %d\n", WiFi.RSSI());
    wifiChangedAt = millis();
  }
}

}  // namespace

void holdWifiPolicy() {
  WiFi.setAutoReconnect(true);
  // A desk display is plugged in. Power-save makes some mesh systems decide
  // the board has left and refuse to hand it to another node.
  WiFi.setSleep(false);
  // Look at every channel and pick the strongest node with this name.
  // The default stops at the first node it hears.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
}

void prepareWifi() {
  WiFi.mode(WIFI_STA);
  holdWifiPolicy();

  if (!eventsHooked) {
    WiFi.onEvent(onWifiEvent);
    eventsHooked = true;
  }
}

void connectSavedWifi() {
  clearNodeLock();
  holdWifiPolicy();
  WiFi.begin();
}

void maintainWifi() {
  static uint32_t downSince = 0;
  static uint32_t lastTry = 0;

  if (WiFi.status() == WL_CONNECTED) {
    downSince = 0;
    return;
  }

  const uint32_t now = millis();
  if (downSince == 0) downSince = now;
  if (now - downSince < RETRY_AFTER_DOWN_MS) return;
  if (lastTry != 0 && now - lastTry < RETRY_GAP_MS) return;

  // No saved name: nothing to retry. Setup is still a 5-second hold.
  wifi_config_t conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK || conf.sta.ssid[0] == 0) return;

  lastTry = now;
  Serial.println("[wifi] still offline, searching for any node of the saved network");
  connectSavedWifi();
}

bool takeSettledWifiChange() {
  const uint32_t at = wifiChangedAt;
  if (at == 0) return false;
  if (millis() - at < WIFI_SETTLE_MS) return false;
  if (wifiChangedAt == at) wifiChangedAt = 0;
  return true;
}
