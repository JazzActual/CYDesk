#include "ota.h"

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "settings_web.h"

namespace ota {
namespace {

bool started = false;

}  // namespace

const char* buildStamp() {
  return __DATE__;
}

void begin() {
  if (started) return;
  if (WiFi.status() != WL_CONNECTED) return;

  ArduinoOTA.setHostname(settings_web::HOSTNAME);
  ArduinoOTA.setPassword(PASSWORD);

  ArduinoOTA.onStart([] {
    Serial.println("[ota] update starting");
  });
  ArduinoOTA.onEnd([] {
    Serial.println("\n[ota] update done; rebooting");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static int lastPct = -1;
    if (total == 0) return;
    const int pct = static_cast<int>((progress * 100u) / total);
    if (pct == lastPct) return;
    lastPct = pct;
    Serial.printf("[ota] %d%%\n", pct);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[ota] error %u\n", static_cast<unsigned>(error));
  });

  ArduinoOTA.begin();
  started = true;
  Serial.printf("[ota] ready as %s.local  password \"%s\"  build %s\n",
                settings_web::HOSTNAME, PASSWORD, buildStamp());
}

void stop() {
  if (!started) return;
  ArduinoOTA.end();
  started = false;
  Serial.println("[ota] stopped");
}

void handle() {
  if (!started) {
    begin();
    return;
  }
  ArduinoOTA.handle();
}

bool isReady() { return started; }

}  // namespace ota
