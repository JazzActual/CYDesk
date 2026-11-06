// Firmware updates: local ArduinoOTA (espota) and HTTPS pull from the
// published channel on GitHub Pages.
//
// Channel (per board folder next to the web installer):
//   https://jazzactual.github.io/CYDesk/<cyd28|cyd35|cyd40>/channel.json
// points at app.bin for that variant. The board compares CYDESK_FW_VERSION
// (UTC YYYYMMDDHHMM baked at build) to the channel's "version" field.
//
// Needs min_spiffs (two app slots). USB flasher and espota remain as backups.
#pragma once

#include <Arduino.h>

class TFT_eSPI;

namespace ota {

constexpr const char* PASSWORD = "cydesk";

// GitHub Pages root that hosts flasher/<board>/channel.json + app.bin.
constexpr const char* CHANNEL_BASE = "https://jazzactual.github.io/CYDesk";

enum class CheckResult : uint8_t {
  Idle = 0,
  Checking,
  UpToDate,
  Available,
  Failed,
};

enum class ApplyResult : uint8_t {
  Idle = 0,
  Applying,
  Failed,
};

// Call once Wi-Fi is up (safe to call again; only starts once).
void begin();

// Tear down before the captive portal takes the radio.
void stop();

// Call every loop(): ArduinoOTA + deferred channel apply.
void handle();

bool isReady();

// Comparable build id (CYDESK_FW_VERSION). Falls back to __DATE__ if unset.
const char* version();

// Human stamp for Status ("Nov  6 2026").
const char* buildStamp();

// Board folder on the channel: cyd28 / cyd35 / cyd40.
const char* boardId();

// Optional TFT for the full-screen progress UI during apply.
void setProgressDisplay(TFT_eSPI* tft);

// Fetch channel.json for this board. Blocking; call from the settings page.
CheckResult checkChannel();

CheckResult checkResult();
const char* checkMessage();   // short status for UI
const char* channelVersion(); // remote version when Available / UpToDate
const char* channelName();    // "CYDesk 2.8" etc., or ""

bool updateAvailable();

// Queue an apply after the HTTP response is sent. handle() runs it.
bool requestApply();
bool applyPending();

ApplyResult applyResult();
const char* applyMessage();
int applyPercent();  // 0–100 while Applying

}  // namespace ota
