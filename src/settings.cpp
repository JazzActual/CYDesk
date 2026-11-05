#include "settings.h"

#include <Preferences.h>

#include "theme.h"

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
  // isKey first: getString logs an error when the key has never been saved.
  wdgKey     = prefs.isKey("wdgkey") ? prefs.getString("wdgkey", "") : "";
  fahrenheit = prefs.getBool("fahrenheit", true);
  clock24h   = prefs.getBool("clock24h", false);
  petAnimal  = prefs.getUChar("pet", static_cast<uint8_t>(PetAnimal::Fox));
  petMode    = prefs.getUChar("petmode", static_cast<uint8_t>(PetMode::Reacts));
  // isKey first: getUInt logs an error when the key has never been saved.
  accent     = prefs.isKey("accent") ? prefs.getUInt("accent", theme::DEFAULT_ACCENT)
                                     : theme::DEFAULT_ACCENT;
  accent &= 0xFFFFFFu;
  if (petAnimal > static_cast<uint8_t>(PetAnimal::Owl)) petAnimal = static_cast<uint8_t>(PetAnimal::Fox);
  if (petMode > static_cast<uint8_t>(PetMode::Reacts)) petMode = static_cast<uint8_t>(PetMode::Reacts);
  theme::setAccent(accent);
  placeLabel = prefs.getString("place", "");
  const bool dropBadge = prefs.isKey("badge");
  prefs.end();
  // Drop a picture address saved by an older version. One write, then it's gone.
  if (dropBadge) {
    Preferences wipe;
    wipe.begin(NVS_NAMESPACE, /*readOnly=*/false);
    wipe.remove("badge");
    wipe.end();
  }
}

void Settings::save() const {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
  prefs.putString("name", name);
  prefs.putString("zip", zip);
  prefs.putString("wkey", weatherKey);
  prefs.remove("badge");  // image addresses are no longer used
  prefs.putString("wdgkey", wdgKey);
  prefs.putBool("fahrenheit", fahrenheit);
  prefs.putBool("clock24h", clock24h);
  prefs.putUChar("pet", petAnimal);
  prefs.putUChar("petmode", petMode);
  prefs.putUInt("accent", accent & 0xFFFFFFu);
  prefs.putString("place", placeLabel);
  prefs.remove("city");  // left over from before the switch to ZIP codes
  prefs.end();
}
