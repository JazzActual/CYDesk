#include "settings_web.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

namespace settings_web {
namespace {

WebServer server(80);
Settings* settings = nullptr;
const String* placeError = nullptr;
bool running = false;
bool changed = false;

// Makes user text safe to put inside HTML (so a name like <b> can't break the page).
String escapeHtml(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (char c : in) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c;
    }
  }
  return out;
}

// Styling to roughly match the WDGWars card: navy, cyan, pink.
const char PAGE_HEAD[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYDesk settings</title>
<style>
 body{background:#0a1018;color:#e6edf3;font-family:-apple-system,system-ui,sans-serif;margin:0;padding:20px;max-width:520px}
 h1{color:#00e5ff;font-size:22px;margin:0 0 4px}
 .sub{color:#5d7b8c;font-size:13px;margin-bottom:18px}
 label{display:block;color:#5d7b8c;font-size:12px;letter-spacing:.08em;text-transform:uppercase;margin:16px 0 6px}
 input[type=text],input[type=password],textarea{width:100%;box-sizing:border-box;background:#0f2531;color:#e6edf3;
   border:1px solid #1d3a4a;border-radius:8px;padding:10px;font-size:16px}
 textarea{height:90px;font-family:ui-monospace,Menlo,monospace;font-size:13px}
 .units label{display:inline;text-transform:none;letter-spacing:0;color:#e6edf3;font-size:16px;margin-right:18px}
 .hint{color:#5d7b8c;font-size:12px;margin-top:4px}
 button{margin-top:22px;width:100%;background:#00e5ff;color:#0a1018;border:0;border-radius:8px;padding:12px;font-size:17px;font-weight:600}
 .status{background:#0f2531;border-radius:8px;padding:10px 12px;font-size:14px;line-height:1.6}
 .ok{color:#00e5ff}.bad{color:#ff2d95}.wait{color:#5d7b8c}
 a{color:#00e5ff}
</style></head><body>
)HTML";

void handleRoot() {
  const Settings& s = *settings;
  const bool justSaved = server.hasArg("saved");
  const bool checking = s.isComplete() && s.placeLabel.isEmpty() && placeError->isEmpty();

  String html;
  html.reserve(4096);
  html += FPSTR(PAGE_HEAD);
  // While the board is still checking the ZIP/key, reload every 3 s to show the result.
  if (justSaved && checking) html += "<meta http-equiv='refresh' content='3;url=/?saved=1'>";

  html += "<h1>CYDesk settings</h1><div class='sub'>Saved on the desk buddy itself.</div>";

  // Status box
  html += "<div class='status'>";
  if (justSaved) html += "<div class='ok'>&#10003; Saved</div>";
  html += "Wi-Fi: <span class='ok'>" + escapeHtml(WiFi.SSID()) + "</span><br>Weather: ";
  if (s.placeLabel.length()) {
    html += "<span class='ok'>Found: " + escapeHtml(s.placeLabel) + "</span>";
  } else if (placeError->length()) {
    html += "<span class='bad'>" + escapeHtml(*placeError) + "</span>";
  } else if (checking) {
    html += "<span class='wait'>checking&hellip;</span>";
  } else {
    html += "<span class='wait'>needs a ZIP code and API key</span>";
  }
  html += "</div>";

  // The form
  html += "<form method='POST' action='/save'>";

  html += "<label for='name'>Your name (for the greeting)</label>"
          "<input type='text' id='name' name='name' maxlength='20' value='" +
          escapeHtml(s.name) + "'>";

  html += "<label for='zip'>ZIP code (for weather)</label>"
          "<input type='text' id='zip' name='zip' maxlength='10' inputmode='numeric' value='" +
          escapeHtml(s.zip) + "'>";

  // The key is never put back into the page: the box starts empty.
  html += "<label for='wkey'>WeatherAPI.com key</label>"
          "<input type='password' id='wkey' name='wkey' maxlength='40' autocomplete='off' placeholder='";
  html += s.weatherKey.length() ? "saved &mdash; leave blank to keep it" : "paste your key";
  html += "'><div class='hint'>Free at <a href='https://www.weatherapi.com/signup.aspx'>weatherapi.com</a>"
          " &rarr; dashboard &rarr; API key.</div>";

  html += "<label for='badge'>Badge (paste the HTML embed code or an image URL)</label>"
          "<textarea id='badge' name='badge'>" + escapeHtml(s.badgeUrl) + "</textarea>"
          "<div class='hint'>Only the image address is kept. Leave empty for no badge.</div>";

  html += "<label>Temperature units</label><div class='units'>";
  html += String("<label><input type='radio' name='units' value='F'") +
          (s.fahrenheit ? " checked" : "") + "> &deg;F</label>";
  html += String("<label><input type='radio' name='units' value='C'") +
          (s.fahrenheit ? "" : " checked") + "> &deg;C</label></div>";

  html += "<label>Clock</label><div class='units'>";
  html += String("<label><input type='radio' name='clock' value='12'") +
          (s.clock24h ? "" : " checked") + "> 12-hour (1:07 PM)</label>";
  html += String("<label><input type='radio' name='clock' value='24'") +
          (s.clock24h ? " checked" : "") + "> 24-hour (13:07)</label></div>";

  html += "<button type='submit'>Save to desk buddy</button></form>";
  html += "<p class='hint'>To change the Wi-Fi network, hold a finger on the screen for 5 seconds.</p>";
  html += "</body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

void handleSave() {
  Settings& s = *settings;

  String zip = server.arg("zip");
  zip.trim();
  String key = server.arg("wkey");
  key.trim();
  if (key.isEmpty()) key = s.weatherKey;  // blank = keep the saved key

  if (zip != s.zip || key != s.weatherKey) {
    s.placeLabel = "";  // location or key changed: check it again
  }
  s.zip = zip;
  s.weatherKey = key;
  s.name = server.arg("name");
  s.name.trim();
  s.badgeUrl = extractImageUrl(server.arg("badge"));
  s.fahrenheit = server.arg("units") != "C";
  s.clock24h = server.arg("clock") == "24";
  s.save();
  changed = true;

  // Never print the key itself, only whether one is saved.
  Serial.printf("[web] saved: zip=%s key=%s units=%s clock=%s badge=%s\n",
                s.zip.length() ? "set" : "missing", s.weatherKey.length() ? "set" : "missing",
                s.fahrenheit ? "F" : "C", s.clock24h ? "24h" : "12h",
                s.badgeUrl.length() ? "set" : "none");

  // "Post/Redirect/Get": send the browser back to the page, so a refresh
  // doesn't submit the form twice.
  server.sendHeader("Location", "/?saved=1");
  server.send(303);
}

}  // namespace

void begin(Settings& s, const String& error) {
  settings = &s;
  placeError = &error;
  if (running) return;

  static bool routesAdded = false;  // begin() can run again after stop()
  if (!routesAdded) {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound([] {
      server.sendHeader("Location", "/");
      server.send(302);
    });
    routesAdded = true;
  }
  server.begin();

  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[web] mDNS failed; use the IP address instead");
  }
  running = true;
  Serial.printf("[web] settings page at http://%s.local  (http://%s)\n", HOSTNAME,
                WiFi.localIP().toString().c_str());
}

void stop() {
  if (!running) return;
  server.stop();
  MDNS.end();
  running = false;
}

bool isRunning() { return running; }

void handle() {
  if (running) server.handleClient();
}

bool takeChanged() {
  const bool was = changed;
  changed = false;
  return was;
}

}  // namespace settings_web
