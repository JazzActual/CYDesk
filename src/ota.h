// Local-network firmware updates (ArduinoOTA).
//
// Once the board is on Wi-Fi, PlatformIO / espota can push a new app image
// without USB. The password is fixed and short on purpose: this sits on a
// home network next to the settings page, which is already open to the LAN.
//
// Needs an OTA-capable partition table (min_spiffs). USB flasher installs
// still rewrite the whole image; OTA only writes the inactive app slot.
#pragma once

namespace ota {

// Default password for `pio run -t upload --upload-protocol espota`.
constexpr const char* PASSWORD = "cydesk";

// Call once Wi-Fi is up (safe to call again; only starts once).
void begin();

// Tear down before the captive portal takes the radio (or any long Wi-Fi pause).
void stop();

// Call every loop() while Wi-Fi may be connected.
void handle();

// True after begin() succeeded.
bool isReady();

// Build stamp shown on Status / settings ("Nov  6 2026" style from the compiler).
const char* buildStamp();

}  // namespace ota
