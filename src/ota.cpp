#include "ota.h"

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <TFT_eSPI.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "settings_web.h"
#include "theme.h"

namespace ota {
namespace {

bool started = false;
TFT_eSPI* progressTft = nullptr;

CheckResult checkState = CheckResult::Idle;
ApplyResult applyState = ApplyResult::Idle;
bool applyQueued = false;
int progressPct = 0;

String checkMsg;
String applyMsg;
String remoteVersion;
String remoteName;
String remoteAppUrl;
int remoteSize = 0;

#if defined(BOARD_CYD40)
constexpr const char* BOARD_ID = "cyd40";
#elif defined(BOARD_CYD35)
constexpr const char* BOARD_ID = "cyd35";
#elif defined(BOARD_NICEMCU28)
constexpr const char* BOARD_ID = "nicemcu28";
#else
constexpr const char* BOARD_ID = "cyd28";
#endif

#ifndef CYDESK_FW_VERSION
#define CYDESK_FW_VERSION "0"
#endif

String channelJsonUrl() {
  return String(CHANNEL_BASE) + "/" + BOARD_ID + "/channel.json";
}

String resolveAppUrl(const String& appField) {
  if (appField.startsWith("http://") || appField.startsWith("https://")) return appField;
  if (appField.startsWith("/")) return String(CHANNEL_BASE) + appField;
  return String(CHANNEL_BASE) + "/" + BOARD_ID + "/" + appField;
}

void drawProgress(int pct, const char* title) {
  progressPct = pct;
  if (!progressTft) return;
  TFT_eSPI& tft = *progressTft;
  tft.fillScreen(theme::BG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(theme::accent(), theme::BG);
  tft.drawString(title, tft.width() / 2, 36, 4);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString(version(), tft.width() / 2, 78, 2);

  const int barW = tft.width() - 48;
  const int barH = 18;
  const int barX = 24;
  const int barY = tft.height() / 2;
  tft.drawRect(barX, barY, barW, barH, theme::MUTED);
  const int fill = (barW - 4) * constrain(pct, 0, 100) / 100;
  if (fill > 0) tft.fillRect(barX + 2, barY + 2, fill, barH - 4, theme::accent());

  char line[24];
  snprintf(line, sizeof(line), "%d%%", constrain(pct, 0, 100));
  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString(line, tft.width() / 2, barY + barH + 16, 2);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("Keep power on", tft.width() / 2, tft.height() - 28, 2);
}

void drawFailure(const char* msg) {
  if (!progressTft) return;
  TFT_eSPI& tft = *progressTft;
  tft.fillScreen(theme::BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(theme::PINK, theme::BG);
  tft.drawString("Update failed", tft.width() / 2, tft.height() / 2 - 18, 4);
  tft.setTextColor(theme::TEXT, theme::BG);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2 + 18, 2);
  tft.setTextColor(theme::MUTED, theme::BG);
  tft.drawString("Try again from settings", tft.width() / 2, tft.height() - 28, 2);
}

void applyNow() {
  applyQueued = false;
  applyState = ApplyResult::Applying;
  applyMsg = "Downloading…";
  progressPct = 0;

  if (remoteAppUrl.isEmpty()) {
    applyState = ApplyResult::Failed;
    applyMsg = "No download URL. Check for updates first.";
    drawFailure(applyMsg.c_str());
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    applyState = ApplyResult::Failed;
    applyMsg = "Wi-Fi disconnected.";
    drawFailure(applyMsg.c_str());
    return;
  }

  Serial.printf("[ota] applying %s from %s\n", remoteVersion.c_str(), remoteAppUrl.c_str());
  drawProgress(0, "Updating…");

  // Pause local OTA listener so it does not fight the pull.
  if (started) {
    ArduinoOTA.end();
    started = false;
  }

  WiFiClientSecure tls;
  tls.setInsecure();  // same trust model as weather / GitHub fetches
  tls.setTimeout(20000);

  httpUpdate.rebootOnUpdate(true);
  httpUpdate.onStart([]() {
    applyMsg = "Writing flash…";
    Serial.println("[ota] http update start");
  });
  httpUpdate.onProgress([](int cur, int total) {
    int pct = 0;
    if (total > 0) pct = (int)((cur * 100LL) / total);
    drawProgress(pct, "Updating…");
    static int last = -1;
    if (pct != last && (pct % 5 == 0 || pct == 100)) {
      last = pct;
      Serial.printf("[ota] download %d%%\n", pct);
    }
  });
  httpUpdate.onEnd([]() {
    applyMsg = "Rebooting…";
    drawProgress(100, "Done");
    Serial.println("[ota] http update end");
  });
  httpUpdate.onError([](int err) {
    Serial.printf("[ota] http update error %d\n", err);
  });

  // Third arg is the current version string; static hosting ignores it, but
  // HTTPUpdate skips work if the server returns 304.
  const t_httpUpdate_return ret = httpUpdate.update(tls, remoteAppUrl, version());

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      applyState = ApplyResult::Failed;
      applyMsg = httpUpdate.getLastErrorString();
      if (applyMsg.isEmpty()) applyMsg = "Download or flash failed.";
      Serial.printf("[ota] failed (%d): %s\n", httpUpdate.getLastError(), applyMsg.c_str());
      drawFailure(applyMsg.c_str());
      begin();  // restore ArduinoOTA
      break;
    case HTTP_UPDATE_NO_UPDATES:
      applyState = ApplyResult::Failed;
      applyMsg = "Server said no update.";
      drawFailure(applyMsg.c_str());
      begin();
      break;
    case HTTP_UPDATE_OK:
      // rebootOnUpdate should already restart; belt and suspenders:
      applyMsg = "Rebooting…";
      delay(250);
      ESP.restart();
      break;
  }
}

}  // namespace

const char* version() { return CYDESK_FW_VERSION; }

const char* buildStamp() { return __DATE__; }

const char* boardId() { return BOARD_ID; }

void setProgressDisplay(TFT_eSPI* tft) { progressTft = tft; }

void begin() {
  if (started) return;
  if (WiFi.status() != WL_CONNECTED) return;

  ArduinoOTA.setHostname(settings_web::HOSTNAME);
  ArduinoOTA.setPassword(PASSWORD);

  ArduinoOTA.onStart([] { Serial.println("[ota] espota starting"); });
  ArduinoOTA.onEnd([] { Serial.println("\n[ota] espota done; rebooting"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static int lastPct = -1;
    if (total == 0) return;
    const int pct = static_cast<int>((progress * 100u) / total);
    if (pct == lastPct) return;
    lastPct = pct;
    Serial.printf("[ota] espota %d%%\n", pct);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[ota] espota error %u\n", static_cast<unsigned>(error));
  });

  ArduinoOTA.begin();
  started = true;
  Serial.printf("[ota] espota ready as %s.local  password \"%s\"  version %s (%s)\n",
                settings_web::HOSTNAME, PASSWORD, version(), BOARD_ID);
}

void stop() {
  if (!started) return;
  ArduinoOTA.end();
  started = false;
  Serial.println("[ota] espota stopped");
}

void handle() {
  if (applyQueued) {
    applyNow();
    return;
  }
  if (!started) {
    begin();
    return;
  }
  ArduinoOTA.handle();
}

bool isReady() { return started; }

CheckResult checkChannel() {
  checkState = CheckResult::Checking;
  checkMsg = "Checking…";
  remoteVersion = "";
  remoteName = "";
  remoteAppUrl = "";
  remoteSize = 0;

  if (WiFi.status() != WL_CONNECTED) {
    checkState = CheckResult::Failed;
    checkMsg = "Wi-Fi is not connected.";
    return checkState;
  }

  const String url = channelJsonUrl();
  Serial.printf("[ota] check %s (have %s)\n", url.c_str(), version());

  WiFiClientSecure tls;
  tls.setInsecure();
  HTTPClient http;
  http.setTimeout(12000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(tls, url)) {
    checkState = CheckResult::Failed;
    checkMsg = "Could not start the channel request.";
    return checkState;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    checkState = CheckResult::Failed;
    checkMsg = String("Channel HTTP ") + code;
    http.end();
    return checkState;
  }

  const String body = http.getString();
  http.end();

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    checkState = CheckResult::Failed;
    checkMsg = "Channel JSON was unreadable.";
    return checkState;
  }

  const char* ver = doc["version"] | "";
  const char* name = doc["name"] | "";
  const char* app = doc["app"] | "app.bin";
  const char* board = doc["board"] | "";
  remoteSize = doc["size"] | 0;

  if (!ver[0]) {
    checkState = CheckResult::Failed;
    checkMsg = "Channel has no version.";
    return checkState;
  }
  if (board[0] && String(board) != BOARD_ID) {
    checkState = CheckResult::Failed;
    checkMsg = String("Wrong board in channel (") + board + ").";
    return checkState;
  }

  remoteVersion = ver;
  remoteName = name;
  remoteAppUrl = resolveAppUrl(app);

  if (remoteVersion == version()) {
    checkState = CheckResult::UpToDate;
    checkMsg = String("Up to date (") + version() + ").";
  } else if (remoteVersion > version()) {
    checkState = CheckResult::Available;
    checkMsg = String("Update available: ") + remoteVersion;
  } else {
    // Channel older than this build (dev / unreleased). Treat as up to date.
    checkState = CheckResult::UpToDate;
    checkMsg = String("Running a newer build than the channel (") + version() + ").";
  }
  Serial.printf("[ota] check result: %s\n", checkMsg.c_str());
  return checkState;
}

CheckResult checkResult() { return checkState; }
const char* checkMessage() { return checkMsg.c_str(); }
const char* channelVersion() { return remoteVersion.c_str(); }
const char* channelName() { return remoteName.c_str(); }

bool updateAvailable() { return checkState == CheckResult::Available && remoteAppUrl.length() > 0; }

bool requestApply() {
  if (!updateAvailable()) {
    applyState = ApplyResult::Failed;
    applyMsg = "No update queued. Check first.";
    return false;
  }
  if (applyState == ApplyResult::Applying || applyQueued) return false;
  applyQueued = true;
  applyState = ApplyResult::Applying;
  applyMsg = "Starting…";
  progressPct = 0;
  return true;
}

bool applyPending() { return applyQueued || applyState == ApplyResult::Applying; }

ApplyResult applyResult() { return applyState; }
const char* applyMessage() { return applyMsg.c_str(); }
int applyPercent() { return progressPct; }

}  // namespace ota
