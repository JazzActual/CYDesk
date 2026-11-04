#include "badge_service.h"

#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace badge {
namespace {

// The picture has to fit in RAM next to an HTTPS connection. A normal badge
// is well under this; a full-screen photo that isn't gets a plain error.
constexpr size_t MAX_JPEG = 48 * 1024;
constexpr int FIT_W = 320;
constexpr int FIT_H = 240;
constexpr uint32_t HTTP_TIMEOUT_MS = 8000;

uint8_t* jpeg = nullptr;
size_t jpegLen = 0;
uint16_t jpegW = 0;
uint16_t jpegH = 0;
String imageUrl;  // address the current fetch cycle is for (empty = no badge)
String error;
uint32_t epoch = 0;  // generation(); starts at 0
bool soon = true;
bool fetchedOnce = false;
uint32_t lastAttempt = 0;

TFT_eSPI* screen = nullptr;

String urlEncode(const String& in) {
  static const char* hex = "0123456789ABCDEF";
  String out;
  out.reserve(in.length() * 3);
  for (unsigned i = 0; i < in.length(); i++) {
    const uint8_t c = static_cast<uint8_t>(in[i]);
    const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
    if (plain) {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

void clearImage() {
  free(jpeg);
  jpeg = nullptr;
  jpegLen = 0;
  jpegW = 0;
  jpegH = 0;
}

// Records a failed fetch. The previous JPEG stays, so the page can keep
// showing it and mark it stale. Same error twice doesn't redraw the page.
void fail(const char* why) {
  Serial.printf("[badge] failed: %s\n", why);
  if (error == why) return;
  error = why;
  epoch++;
}

void applyUrl(const Settings& settings) {
  if (settings.badgeUrl == imageUrl) return;
  clearImage();
  error = "";
  imageUrl = settings.badgeUrl;
  soon = imageUrl.length() > 0;
  epoch++;
}

// wsrv.nl fetches the image for us and sends back one baseline JPEG that
// fits inside the screen. "il" (progressive) is left off: the decoder can't
// read progressive JPEGs. The source address is encoded so its own ? and &
// aren't read as wsrv's options.
String requestUrl() {
  String url = "https://wsrv.nl/?url=";
  url += urlEncode(imageUrl);
  url += "&w=" + String(FIT_W) + "&h=" + String(FIT_H);
  url += "&fit=inside&output=jpg&q=80";
  return url;
}

// Reads the reply into a buffer. Caller ends the HTTP client. On failure,
// *out is left null and fail() has been called.
uint8_t* download(HTTPClient& http, size_t& outLen) {
  outLen = 0;
  const int status = http.GET();
  if (status <= 0) {
    Serial.printf("[badge] request failed: %s\n", http.errorToString(status).c_str());
    fail("Couldn't load that badge");
    return nullptr;
  }
  if (status != 200) {
    Serial.printf("[badge] HTTP %d\n", status);
    fail("Couldn't load that badge");
    return nullptr;
  }

  const int reported = http.getSize();
  if (reported > (int)MAX_JPEG) {
    fail("That badge is too big to show");
    return nullptr;
  }
  const size_t cap = reported > 0 ? static_cast<size_t>(reported) : MAX_JPEG;
  uint8_t* buf = static_cast<uint8_t*>(malloc(cap));
  if (!buf) {
    fail("Not enough memory");
    return nullptr;
  }

  WiFiClient* stream = http.getStreamPtr();
  const uint32_t start = millis();
  size_t got = 0;
  while (got < cap && millis() - start < HTTP_TIMEOUT_MS) {
    const int avail = stream->available();
    if (avail <= 0) {
      if (!http.connected()) break;
      delay(1);
      continue;
    }
    size_t want = static_cast<size_t>(avail);
    if (want > cap - got) want = cap - got;
    const int n = stream->readBytes(buf + got, want);
    if (n <= 0) break;
    got += static_cast<size_t>(n);
  }

  // A size we didn't know ahead of time, and the buffer is full with more
  // still coming: the picture doesn't fit our cap.
  if (reported <= 0 && got == cap && stream->available()) {
    free(buf);
    fail("That badge is too big to show");
    return nullptr;
  }
  if (reported > 0 && got != static_cast<size_t>(reported)) {
    free(buf);
    fail("Couldn't load that badge");
    return nullptr;
  }
  if (got < 2 || buf[0] != 0xFF || buf[1] != 0xD8) {
    free(buf);
    fail("That address isn't a picture");
    return nullptr;
  }
  outLen = got;
  return buf;
}

void fetch() {
  Serial.printf("[badge] fetching (heap %u, max block %u)\n", ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());
  // An HTTPS handshake needs a large contiguous block. If it isn't free,
  // wait and try again rather than failing halfway through the connection.
  if (ESP.getMaxAllocHeap() < 40 * 1024) {
    fail("Not enough memory");
    return;
  }

  uint8_t* fresh = nullptr;
  size_t freshLen = 0;
  {
    // Certificate isn't checked. Same tradeoff as the weather fetch: fine for
    // a desk toy, pinning can come later.
    WiFiClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.useHTTP10(true);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.addHeader("Accept-Encoding", "identity");
    if (!http.begin(tls, requestUrl())) {
      fail("Couldn't load that badge");
      return;
    }
    fresh = download(http, freshLen);
    http.end();
  }

  if (!fresh) return;

  // Decode after the TLS buffers are freed, so the JPEG workspace has room.
  uint16_t w = 0;
  uint16_t h = 0;
  if (TJpgDec.getJpgSize(&w, &h, fresh, freshLen) != 0 || w == 0 || h == 0) {
    free(fresh);
    fail("That address isn't a picture");
    return;
  }

  clearImage();
  jpeg = fresh;
  jpegLen = freshLen;
  jpegW = w;
  jpegH = h;
  error = "";
  epoch++;
  Serial.printf("[badge] ok: %ux%u, %u bytes\n", w, h, static_cast<unsigned>(freshLen));
}

// One 8x8 or 16x16 block of pixels from the decoder. Coordinates are already
// on the screen (the decoder adds the top-left we passed to drawJpg).
bool pushBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (!screen) return false;
  if (y >= screen->height() || x >= screen->width()) return false;
  screen->pushImage(x, y, w, h, bitmap);
  return true;
}

}  // namespace

void prepare(const Settings& settings) { applyUrl(settings); }

void update(const Settings& settings) {
  // A WDGWars key uses this page for stats, so drop the picture and stay out
  // of the way of that download. Clearing the key fetches the picture again.
  if (settings.wdgKey.length()) {
    if (jpeg) {
      clearImage();
      epoch++;
    }
    soon = true;
    return;
  }
  applyUrl(settings);
  if (!imageUrl.length() || WiFi.status() != WL_CONNECTED) return;

  const uint32_t wait = error.length() ? RETRY_MS : REFRESH_MS;
  if (!soon && fetchedOnce && millis() - lastAttempt < wait) return;

  soon = false;
  fetchedOnce = true;
  lastAttempt = millis();
  fetch();
}

uint32_t generation() { return epoch; }
bool hasImage() { return jpeg != nullptr; }
bool stale() { return jpeg != nullptr && error.length() > 0; }
const String& lastError() { return error; }

bool paint(TFT_eSPI& tft, int& x, int& y, int& w, int& h) {
  if (!jpeg) return false;

  // The decoder only shrinks by 1, 2, 4 or 8. wsrv already fits the screen,
  // so this is a safety net for an image that came back larger.
  int scale = 1;
  while (scale < 8 && (jpegW / scale > tft.width() || jpegH / scale > tft.height())) scale *= 2;
  w = jpegW / scale;
  h = jpegH / scale;
  if (w < 1 || h < 1) return false;
  x = (tft.width() - w) / 2;
  y = (tft.height() - h) / 2;

  screen = &tft;
  TJpgDec.setJpgScale(scale);
  TJpgDec.setSwapBytes(true);  // decoder's byte order -> the display's
  TJpgDec.setCallback(pushBlock);
  const int rc = TJpgDec.drawJpg(x, y, jpeg, jpegLen);
  screen = nullptr;
  return rc == 0;
}

}  // namespace badge
