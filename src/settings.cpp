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
  fahrenheit = prefs.getBool("fahrenheit", true);
  clock24h   = prefs.getBool("clock24h", false);
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
  prefs.putBool("fahrenheit", fahrenheit);
  prefs.putBool("clock24h", clock24h);
  prefs.putString("place", placeLabel);
  prefs.remove("city");  // left over from before the switch to ZIP codes
  prefs.end();
}

String extractImageUrl(const String& htmlOrUrl) {
  String text = htmlOrUrl;
  text.trim();

  // A bare address: use it as-is.
  if (text.startsWith("http://") || text.startsWith("https://")) return text;

  // Otherwise look for src="..." or src='...' (any capitalisation).
  String lower = text;
  lower.toLowerCase();
  const int at = lower.indexOf("src=");
  if (at < 0) return "";
  const int open = at + 4;
  if (open >= (int)text.length()) return "";
  const char quote = text[open];
  if (quote != '"' && quote != '\'') return "";
  const int close = text.indexOf(quote, open + 1);
  if (close < 0) return "";
  return text.substring(open + 1, close);
}
