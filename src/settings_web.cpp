#include "settings_web.h"

#include <math.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "github_service.h"
#include "theme.h"

namespace settings_web {
namespace {

WebServer server(80);
Settings* settings = nullptr;
const String* placeError = nullptr;
bool running = false;
bool changed = false;
bool iconTestRequested = false;

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

// "#00e5ff" from the 24-bit color stored in flash. The color input needs
// exactly this form.
String hexColor(uint32_t rgb24) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%06x", rgb24 & 0xFFFFFFu);
  return String(buf);
}

// Relative luminance, 0 = black, 1 = white. Used so the Save button's text
// stays readable on a dark accent as well as on cyan.
float channel(uint8_t c) {
  const float x = c / 255.f;
  return x <= 0.04045f ? x / 12.92f : powf((x + 0.055f) / 1.055f, 2.4f);
}

float relLuma(uint32_t rgb24) {
  return 0.2126f * channel((rgb24 >> 16) & 255) + 0.7152f * channel((rgb24 >> 8) & 255) +
         0.0722f * channel(rgb24 & 255);
}

float contrast(float a, float b) {
  const float hi = a > b ? a : b;
  const float lo = a > b ? b : a;
  return (hi + 0.05f) / (lo + 0.05f);
}

// Navy when that reads more clearly on the accent, otherwise the off-white.
const char* onAccent(uint32_t rgb24) {
  const float L = relLuma(rgb24);
  const float navy = relLuma(0x0A1018);
  const float paper = relLuma(0xE6EDF3);
  return contrast(L, navy) >= contrast(L, paper) ? "#0a1018" : "#e6edf3";
}

// Accepts the value an <input type="color"> submits: "#rrggbb".
// "04:00" or "4:00" -> minutes after midnight.
bool parseClockTime(const String& in, uint16_t& out) {
  int hour = -1;
  int minute = -1;
  if (sscanf(in.c_str(), "%d:%d", &hour, &minute) != 2) return false;
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
  out = static_cast<uint16_t>(hour * 60 + minute);
  return true;
}

String clockTime(uint16_t minuteOfDay) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", minuteOfDay / 60, minuteOfDay % 60);
  return String(buf);
}

bool parseHexColor(const String& in, uint32_t& out) {
  if (in.length() != 7 || in.charAt(0) != '#') return false;
  for (int i = 1; i < 7; i++) {
    if (!isxdigit(static_cast<unsigned char>(in.charAt(i)))) return false;
  }
  out = strtoul(in.c_str() + 1, nullptr, 16) & 0xFFFFFFu;
  return true;
}

// Styling to match the desk buddy: navy, the saved accent, and pink for errors.
// --accent is filled in per request so the page opens in the saved color.
const char PAGE_HEAD[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYDesk settings</title>
<style>
 body{background:#0a1018;color:#e6edf3;font-family:-apple-system,system-ui,sans-serif;margin:0;padding:20px;max-width:520px;accent-color:var(--accent,#00e5ff)}
 h1{color:var(--accent,#00e5ff);font-size:22px;margin:0 0 4px}
 .sub{color:#5d7b8c;font-size:13px;margin-bottom:18px}
 label{display:block;color:#5d7b8c;font-size:12px;letter-spacing:.08em;text-transform:uppercase;margin:16px 0 6px}
 input[type=text],input[type=password],textarea{width:100%;box-sizing:border-box;background:#0f2531;color:#e6edf3;
   border:1px solid #1d3a4a;border-radius:8px;padding:10px;font-size:16px}
 input[type=text]:focus,input[type=password]:focus,input[type=time]:focus,textarea:focus{border-color:var(--accent,#00e5ff);outline:none}
 textarea{min-height:88px;resize:vertical}
 input[type=time]{width:160px;box-sizing:border-box;background:#0f2531;color:#e6edf3;
   border:1px solid #1d3a4a;border-radius:8px;padding:10px;font-size:16px;color-scheme:dark}
 .units label{display:inline;text-transform:none;letter-spacing:0;color:#e6edf3;font-size:16px;margin-right:18px}
 .screens label{display:block;text-transform:none;letter-spacing:0;color:#e6edf3;font-size:16px;margin:8px 0}
 .hint{color:#5d7b8c;font-size:12px;margin-top:4px}
 .color-row{display:flex;align-items:center;gap:12px}
 input[type=color]{width:72px;height:42px;padding:3px;background:#0f2531;border:1px solid #1d3a4a;border-radius:8px}
 #accent-hex{font-size:16px;letter-spacing:.04em}
 button{margin-top:22px;width:100%;background:var(--accent,#00e5ff);color:var(--on-accent,#0a1018);border:0;border-radius:8px;padding:12px;font-size:17px;font-weight:600}
 .status{background:#0f2531;border-radius:8px;padding:10px 12px;font-size:14px;line-height:1.6}
 .ok{color:var(--accent,#00e5ff)}.bad{color:#ff2d95}.wait{color:#5d7b8c}
 a{color:var(--accent,#00e5ff)}
</style></head><body>
)HTML";

// Updates the page as the picker moves, before Save is pressed.
const char PAGE_SCRIPT[] PROGMEM = R"HTML(<script>
(function(){
  var picker=document.getElementById('accent');
  var hex=document.getElementById('accent-hex');
  var root=document.documentElement;
  function channel(c){c=c/255;return c<=0.04045?c/12.92:Math.pow((c+0.055)/1.055,2.4);}
  function luma(h){
    var n=parseInt(h.slice(1),16);
    return 0.2126*channel((n>>16)&255)+0.7152*channel((n>>8)&255)+0.0722*channel(n&255);
  }
  function contrast(a,b){var hi=a>b?a:b, lo=a>b?b:a; return (hi+0.05)/(lo+0.05);}
  var navy=luma('#0a1018'), paper=luma('#e6edf3');
  function apply(){
    var c=picker.value;
    var L=luma(c);
    root.style.setProperty('--accent', c);
    root.style.setProperty('--on-accent', contrast(L,navy)>=contrast(L,paper)?'#0a1018':'#e6edf3');
    hex.textContent=c.toUpperCase();
  }
  picker.addEventListener('input', apply);
  apply();
})();
</script>)HTML";

void handleRoot() {
  const Settings& s = *settings;
  const bool justSaved = server.hasArg("saved");
  const bool checking = s.isComplete() && s.placeLabel.isEmpty() && placeError->isEmpty();

  String html;
  html.reserve(12000);
  html += FPSTR(PAGE_HEAD);
  // The saved color, so the first paint matches the desk buddy before the
  // script runs. Dragging the picker updates the same variables live.
  html += "<style>:root{--accent:" + hexColor(s.accent) + ";--on-accent:" + onAccent(s.accent) +
          "}</style>";
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

  html += "<label for='accent'>Accent color</label><div class='color-row'>"
          "<input type='color' id='accent' name='accent' value='" +
          hexColor(s.accent) + "'><span id='accent-hex'>" + hexColor(s.accent) +
          "</span></div>"
          "<div class='hint'>Colors the heading, links, choices, and Save button on this page, "
          "and the highlights on the desk buddy. Pink stays for errors, the umbrella, and sunset.</div>";

  html += "<label>Screens</label><div class='screens'>";
  const char* screenNames[] = {"Home", "Hourly", "Badge", "WiGLE", "Wardrift", "Stars", "Repos"};
  for (uint8_t i = 0; i < ScreenStatus; i++) {
    html += "<label><input type='checkbox' name='screen" + String(i) + "' value='1'";
    if (s.screenOn(i)) html += " checked";
    html += "> " + String(screenNames[i]) + "</label>";
  }
  html += "</div><div class='hint'>A tap skips any screen that is off. "
          "Status always stays on. Home stays on if every other box is cleared.</div>";

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

  // The key is never put back into the page. The checkbox clears a saved key.
  html += "<label for='wdgkey'>WDGWars key</label>"
          "<input type='password' id='wdgkey' name='wdgkey' maxlength='80' autocomplete='off' placeholder='";
  html += s.wdgKey.length() ? "saved &mdash; leave blank to keep it" : "paste a key for this desk";
  html += "'><div class='hint'>Shows this desk's WDGWars stats on the badge page. "
          "Make a key on your WDGWars profile just for this desk. Leave blank to keep the saved key.</div>";
  if (s.wdgKey.length()) {
    html += "<div class='hint'><input type='checkbox' name='wdgclear' value='1'> "
            "Remove the saved WDGWars key</div>";
  }

  // Neither value is put back into the page. The checkbox clears both.
  const bool wigleSaved = s.wigleName.length() && s.wigleToken.length();
  html += "<label for='wiglname'>WiGLE API name</label>"
          "<input type='password' id='wiglname' name='wiglname' maxlength='48' autocomplete='off' placeholder='";
  html += s.wigleName.length() ? "saved &mdash; leave blank to keep it" : "paste the API name";
  html += "'>";
  html += "<label for='wigltoken'>WiGLE API token</label>"
          "<input type='password' id='wigltoken' name='wigltoken' maxlength='80' autocomplete='off' placeholder='";
  html += s.wigleToken.length() ? "saved &mdash; leave blank to keep it" : "paste the API token";
  html += "'><div class='hint'>From your WiGLE account page. Shows rank and discoveries. "
          "Leave a box blank to keep what's saved.</div>";
  html += "<label for='wiglat'>Check WiGLE at</label>"
          "<input type='time' id='wiglat' name='wiglat' value='" +
          clockTime(s.wigleMinute) + "'>"
          "<div class='hint'>Once a day, at this time. If the desk is off then, it checks once "
          "the next time it is on. A failed check tries again every 15 minutes until one works. "
          "The last numbers stay on screen until the next check.</div>";
  if (wigleSaved) {
    html += "<div class='hint'><input type='checkbox' name='wigleclear' value='1'> "
            "Remove the saved WiGLE login</div>";
  }

  const bool wardriftSaved = s.wardriftUser.length() && s.wardriftPass.length();
  html += "<label for='warduser'>Wardrift username</label>"
          "<input type='text' id='warduser' name='warduser' maxlength='30' autocomplete='off' value='" +
          escapeHtml(s.wardriftUser) + "'>";
  html += "<label for='wardpass'>Wardrift password</label>"
          "<input type='password' id='wardpass' name='wardpass' maxlength='80' autocomplete='off' placeholder='";
  html += s.wardriftPass.length() ? "saved &mdash; leave blank to keep it" : "password for wardrift.net";
  html += "'><div class='hint'>The API key is only for uploads, so it cannot load this page. "
          "The desk signs in with this username and password instead. "
          "Leave the password blank to keep the saved one.</div>";
  if (wardriftSaved) {
    html += "<div class='hint'><input type='checkbox' name='wardclear' value='1'> "
            "Remove the saved Wardrift login</div>";
  }

  html += "<label for='ghrepos'>GitHub repos</label>"
          "<textarea id='ghrepos' name='ghrepos' maxlength='200'>" +
          escapeHtml(s.githubRepos) + "</textarea>"
          "<div class='hint'>Up to four public repos, one per line, as owner/repo. "
          "The desk reads the star count every 15 minutes. No token needed.</div>";

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

  html += "<label>Desk pet</label><div class='units'>";
  html += String("<label><input type='radio' name='petmode' value='off'") +
          (s.petMode == static_cast<uint8_t>(PetMode::Off) ? " checked" : "") + "> Off</label>";
  html += String("<label><input type='radio' name='petmode' value='idle'") +
          (s.petMode == static_cast<uint8_t>(PetMode::Idle) ? " checked" : "") + "> Idle</label>";
  html += String("<label><input type='radio' name='petmode' value='reacts'") +
          (s.petMode == static_cast<uint8_t>(PetMode::Reacts) ? " checked" : "") +
          "> Reacts to weather</label></div>";
  html += "<div class='hint'>Reacts: asleep at night, umbrella in the rain, sunglasses when it's clear.</div>";

  html += "<label>Animal</label><div class='units'>";
  html += String("<label><input type='radio' name='pet' value='none'") +
          (s.petAnimal == static_cast<uint8_t>(PetAnimal::None) ? " checked" : "") + "> None</label>";
  html += String("<label><input type='radio' name='pet' value='fox'") +
          (s.petAnimal == static_cast<uint8_t>(PetAnimal::Fox) ? " checked" : "") + "> Fox</label>";
  html += String("<label><input type='radio' name='pet' value='raccoon'") +
          (s.petAnimal == static_cast<uint8_t>(PetAnimal::Raccoon) ? " checked" : "") + "> Raccoon</label>";
  html += String("<label><input type='radio' name='pet' value='owl'") +
          (s.petAnimal == static_cast<uint8_t>(PetAnimal::Owl) ? " checked" : "") + "> Owl</label>";
  html += String("<label><input type='radio' name='pet' value='bear'") +
          (s.petAnimal == static_cast<uint8_t>(PetAnimal::Bear) ? " checked" : "") +
          "> Bear</label></div>";

  html += "<button type='submit'>Save to desk buddy</button></form>";
  html += "<p class='hint'>To change the Wi-Fi network, hold a finger on the screen for 5 seconds.</p>";
  html += FPSTR(PAGE_SCRIPT);
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
  String wdg = server.arg("wdgkey");
  wdg.trim();
  if (wdg.length()) s.wdgKey = wdg;
  else if (server.hasArg("wdgclear")) s.wdgKey = "";
  String wigleName = server.arg("wiglname");
  wigleName.trim();
  String wigleToken = server.arg("wigltoken");
  wigleToken.trim();
  if (server.hasArg("wigleclear")) {
    s.wigleName = "";
    s.wigleToken = "";
  } else {
    if (wigleName.length()) s.wigleName = wigleName;
    if (wigleToken.length()) s.wigleToken = wigleToken;
  }
  uint16_t wigleMinute = s.wigleMinute;
  if (parseClockTime(server.arg("wiglat"), wigleMinute)) s.wigleMinute = wigleMinute;
  s.wardriftUser = server.arg("warduser");
  s.wardriftUser.trim();
  String wardriftPass = server.arg("wardpass");
  wardriftPass.trim();
  if (server.hasArg("wardclear")) {
    s.wardriftUser = "";
    s.wardriftPass = "";
  } else if (wardriftPass.length()) {
    s.wardriftPass = wardriftPass;
  }
  s.githubRepos = github::normalizeRepos(server.arg("ghrepos"));
  s.fahrenheit = server.arg("units") != "C";
  s.clock24h = server.arg("clock") == "24";
  const String mode = server.arg("petmode");
  s.petMode = mode == "off" ? static_cast<uint8_t>(PetMode::Off)
            : mode == "idle" ? static_cast<uint8_t>(PetMode::Idle)
                             : static_cast<uint8_t>(PetMode::Reacts);
  const String animal = server.arg("pet");
  s.petAnimal = animal == "none" ? static_cast<uint8_t>(PetAnimal::None)
              : animal == "raccoon" ? static_cast<uint8_t>(PetAnimal::Raccoon)
              : animal == "owl" ? static_cast<uint8_t>(PetAnimal::Owl)
              : animal == "bear" ? static_cast<uint8_t>(PetAnimal::Bear)
                                 : static_cast<uint8_t>(PetAnimal::Fox);
  uint32_t accent = s.accent;
  if (parseHexColor(server.arg("accent"), accent)) s.accent = accent;
  uint8_t screens = 1u << ScreenStatus;
  for (uint8_t i = 0; i < ScreenStatus; i++) {
    if (server.hasArg("screen" + String(i))) screens |= 1u << i;
  }
  if ((screens & ~(1u << ScreenStatus)) == 0) screens |= 1u << ScreenHome;
  s.screens = screens;
  theme::setAccent(s.accent);
  s.save();
  changed = true;

  // Never print either key, only whether one is saved.
  Serial.printf("[web] saved: zip=%s key=%s units=%s clock=%s wdg=%s wigle=%s at %s ward=%s pet=%u mode=%u accent=%s\n",
                s.zip.length() ? "set" : "missing", s.weatherKey.length() ? "set" : "missing",
                s.fahrenheit ? "F" : "C", s.clock24h ? "24h" : "12h",
                s.wdgKey.length() ? "set" : "none",
                (s.wigleName.length() && s.wigleToken.length()) ? "set" : "none",
                clockTime(s.wigleMinute).c_str(),
                (s.wardriftUser.length() && s.wardriftPass.length()) ? "set" : "none",
                s.petAnimal, s.petMode, hexColor(s.accent).c_str());
  Serial.printf("[web] screens: %02x\n", s.screens);

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
    server.on("/icons", HTTP_GET, [] {
      iconTestRequested = true;
      server.send(200, "text/plain",
                  "Icon test started: the desk buddy shows every weather icon, 2 s each.");
    });
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

bool takeIconTestRequest() {
  const bool was = iconTestRequested;
  iconTestRequested = false;
  return was;
}

bool takeChanged() {
  const bool was = changed;
  changed = false;
  return was;
}

}  // namespace settings_web
