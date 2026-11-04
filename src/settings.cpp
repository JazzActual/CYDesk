#include "settings.h"

#include <Preferences.h>

namespace {
// Preferences groups keys into "namespaces" (max 15 chars). Keys are max 15 chars too.
constexpr const char* NVS_NAMESPACE = "cydesk";
}  // namespace

void Settings::load() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
  name       = prefs.getString("name", "");
  zip        = prefs.getString("zip", "");
  weatherKey = prefs.getString("wkey", "");
  badgeUrl   = prefs.getString("badge", "");
  // isKey first: getString logs an error when the key has never been saved.
  wdgKey     = prefs.isKey("wdgkey") ? prefs.getString("wdgkey", "") : "";
  fahrenheit = prefs.getBool("fahrenheit", true);
  clock24h   = prefs.getBool("clock24h", false);
  petAnimal  = prefs.getUChar("pet", static_cast<uint8_t>(PetAnimal::Fox));
  petMode    = prefs.getUChar("petmode", static_cast<uint8_t>(PetMode::Reacts));
  if (petAnimal > static_cast<uint8_t>(PetAnimal::Owl)) petAnimal = static_cast<uint8_t>(PetAnimal::Fox);
  if (petMode > static_cast<uint8_t>(PetMode::Reacts)) petMode = static_cast<uint8_t>(PetMode::Reacts);
  placeLabel = prefs.getString("place", "");
  prefs.end();
}

void Settings::save() const {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
  prefs.putString("name", name);
  prefs.putString("zip", zip);
  prefs.putString("wkey", weatherKey);
  prefs.putString("badge", badgeUrl);
  prefs.putString("wdgkey", wdgKey);
  prefs.putBool("fahrenheit", fahrenheit);
  prefs.putBool("clock24h", clock24h);
  prefs.putUChar("pet", petAnimal);
  prefs.putUChar("petmode", petMode);
  prefs.putString("place", placeLabel);
  prefs.remove("city");  // left over from before the switch to ZIP codes
  prefs.end();
}

namespace {
// "&amp;" in an embed is the same as "&" in the real address.
String decodeEntities(String text) {
  text.replace("&amp;", "&");
  text.replace("&quot;", "\"");
  text.replace("&apos;", "'");
  text.replace("&#39;", "'");
  text.replace("&lt;", "<");
  text.replace("&gt;", ">");
  return text;
}

// Finds attr ("src=") where it is its own attribute. Skips lookalikes such as
// data-src=, which is a lazy-load placeholder and not the image itself.
int findAttr(const String& lower, const char* attr, int from, int limit) {
  const int n = strlen(attr);
  int at = from;
  while (true) {
    at = lower.indexOf(attr, at);
    if (at < 0 || (limit >= 0 && at >= limit)) return -1;
    const char prev = at > 0 ? lower[at - 1] : ' ';
    const bool boundary = prev == ' ' || prev == '\n' || prev == '\r' || prev == '\t' || prev == '<';
    if (boundary) return at;
    at += n;
  }
}

// The value of a src attribute. Handles src="...", src='...', and a bare src=url.
String readSrcValue(const String& text, int srcAt) {
  const int open = srcAt + 4;  // just past "src="
  if (open >= (int)text.length()) return "";
  const char quote = text[open];
  if (quote == '"' || quote == '\'') {
    const int close = text.indexOf(quote, open + 1);
    if (close < 0) return "";
    return text.substring(open + 1, close);
  }
  int end = open;
  while (end < (int)text.length()) {
    const char c = text[end];
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '>') break;
    end++;
  }
  return text.substring(open, end);
}

// Makes sure the address is one the board is allowed to ask wsrv.nl to fetch.
bool usableImageUrl(String& url) {
  url = decodeEntities(url);
  url.trim();
  if (url.startsWith("//")) url = "https:" + url;
  return url.startsWith("http://") || url.startsWith("https://");
}
}  // namespace

BadgeExtract extractBadge(const String& htmlOrUrl) {
  BadgeExtract out;
  String text = htmlOrUrl;
  text.trim();
  if (text.isEmpty()) return out;

  // A pasted address, not HTML. The first word is the URL.
  if (text.startsWith("http://") || text.startsWith("https://")) {
    int cut = text.length();
    for (int i = 0; i < (int)text.length(); i++) {
      const char c = text[i];
      if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
        cut = i;
        break;
      }
    }
    out.url = text.substring(0, cut);
    return out;
  }

  String lower = text;
  lower.toLowerCase();

  // Only an <img> counts. A <script> or <iframe> is a web widget, which this
  // screen can't draw — it can only show a picture.
  const int img = lower.indexOf("<img");
  if (img < 0) {
    out.error = "That embed has no image address. Paste an image embed or a direct image link.";
    return out;
  }
  const int tagEnd = lower.indexOf('>', img);
  const int srcAt = findAttr(lower, "src=", img, tagEnd);
  if (srcAt < 0 || (tagEnd >= 0 && srcAt > tagEnd)) {
    out.error = "That image embed has no src address.";
    return out;
  }

  String url = readSrcValue(text, srcAt);
  if (!usableImageUrl(url)) {
    out.error = "The image address has to be an http or https link.";
    return out;
  }
  out.url = url;
  return out;
}
