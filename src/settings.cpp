#include "settings.h"

#include <Preferences.h>

#include "home_strip.h"
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
  // Saved before countries existed = a US ZIP.
  country    = prefs.isKey("country") ? prefs.getString("country", "US") : "US";
  if (country.length() != 2) country = "US";
  geoQuery   = prefs.isKey("geoq") ? prefs.getString("geoq", "") : "";
  geoFor     = prefs.isKey("geofor") ? prefs.getString("geofor", "") : "";
  weatherKey = prefs.getString("wkey", "");
  // isKey first: getString logs an error when the key has never been saved.
  wdgKey     = prefs.isKey("wdgkey") ? prefs.getString("wdgkey", "") : "";
  wigleName  = prefs.isKey("wiglname") ? prefs.getString("wiglname", "") : "";
  wigleToken = prefs.isKey("wigltoken") ? prefs.getString("wigltoken", "") : "";
  wardriftUser = prefs.isKey("warduser") ? prefs.getString("warduser", "") : "";
  wardriftPass = prefs.isKey("wardpass") ? prefs.getString("wardpass", "") : "";
  githubRepos = prefs.isKey("ghrepos") ? prefs.getString("ghrepos", "") : "";
  wigleMinute = prefs.isKey("wiglmin") ? prefs.getUShort("wiglmin", 4 * 60) : 4 * 60;
  if (wigleMinute > 23 * 60 + 59) wigleMinute = 4 * 60;
  fahrenheit = prefs.getBool("fahrenheit", true);
  clock24h   = prefs.getBool("clock24h", false);
  petAnimal  = prefs.getUChar("pet", static_cast<uint8_t>(PetAnimal::Fox));
  petMode    = prefs.getUChar("petmode", static_cast<uint8_t>(PetMode::Reacts));
  // isKey first: getUInt logs an error when the key has never been saved.
  accent     = prefs.isKey("accent") ? prefs.getUInt("accent", theme::DEFAULT_ACCENT)
                                     : theme::DEFAULT_ACCENT;
  accent &= 0xFFFFFFu;
  ledOn = prefs.isKey("ledon") ? prefs.getBool("ledon", false) : false;
  ledColor = prefs.isKey("ledcolor") ? prefs.getUInt("ledcolor", 0x00E5FFu) : 0x00E5FFu;
  ledColor &= 0xFFFFFFu;
  ledBrightness = prefs.isKey("ledbright") ? prefs.getUChar("ledbright", 40) : 40;
  if (ledBrightness < 1 || ledBrightness > 100) ledBrightness = 40;
  screens = prefs.isKey("screens") ? prefs.getUChar("screens", SCREENS_ALL) : SCREENS_ALL;
  screens &= SCREENS_ALL;
  // The build before Repos stored "all on" as 0x7F, with Status in bit 6.
  if (screens == 0x7F) screens = SCREENS_ALL;
  screens |= 1u << ScreenStatus;
  if ((screens & ~(1u << ScreenStatus)) == 0) screens |= 1u << ScreenHome;
  // Home strip pack (source + three fields). No key yet = never configured =
  // stay Off so fresh installs and upgraded desks without a choice show no
  // strip. A saved pack keeps the user's source (including an explicit Off).
  if (prefs.isKey("hstrip")) {
    const uint32_t strip = prefs.getUInt("hstrip", 0);
    homeStripSource = static_cast<uint8_t>(strip & 0xFFu);
    homeStripField[0] = static_cast<uint8_t>((strip >> 8) & 0xFFu);
    homeStripField[1] = static_cast<uint8_t>((strip >> 16) & 0xFFu);
    homeStripField[2] = static_cast<uint8_t>((strip >> 24) & 0xFFu);
  } else {
    homeStripSource = static_cast<uint8_t>(HomeStripSource::Off);
    homeStripField[0] = homeStripField[1] = homeStripField[2] = 0;
  }
  if (petAnimal > static_cast<uint8_t>(PetAnimal::Bear)) petAnimal = static_cast<uint8_t>(PetAnimal::Fox);
  if (petMode > static_cast<uint8_t>(PetMode::Reacts)) petMode = static_cast<uint8_t>(PetMode::Reacts);
  home_strip::clamp(*this);
  theme::setAccent(accent);
  placeLabel = prefs.getString("place", "");
  const bool dropBadge = prefs.isKey("badge");
  // Nearby-name alerts were removed: Bluetooth left too little memory for HTTPS.
  const bool dropBle = prefs.isKey("blenames");
  prefs.end();
  // Drop a picture address saved by an older version, and any saved name list.
  // One write, then it's gone.
  if (dropBadge || dropBle) {
    Preferences wipe;
    wipe.begin(NVS_NAMESPACE, /*readOnly=*/false);
    if (dropBadge) wipe.remove("badge");
    if (dropBle) wipe.remove("blenames");
    wipe.end();
  }
}

void Settings::save() const {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
  prefs.putString("name", name);
  prefs.putString("zip", zip);
  prefs.putString("country", country);
  prefs.putString("geoq", geoQuery);
  prefs.putString("geofor", geoFor);
  prefs.putString("wkey", weatherKey);
  prefs.remove("badge");  // image addresses are no longer used
  prefs.putString("wdgkey", wdgKey);
  prefs.putString("wiglname", wigleName);
  prefs.putString("wigltoken", wigleToken);
  prefs.putString("warduser", wardriftUser);
  prefs.putString("wardpass", wardriftPass);
  prefs.putString("ghrepos", githubRepos);
  prefs.remove("blenames");
  prefs.remove("wardkey");  // an upload key cannot read the dashboard
  prefs.putUShort("wiglmin", wigleMinute);
  prefs.putBool("fahrenheit", fahrenheit);
  prefs.putBool("clock24h", clock24h);
  prefs.putUChar("pet", petAnimal);
  prefs.putUChar("petmode", petMode);
  prefs.putUChar("screens", screens);
  prefs.putUInt("accent", accent & 0xFFFFFFu);
  prefs.putBool("ledon", ledOn);
  prefs.putUInt("ledcolor", ledColor & 0xFFFFFFu);
  prefs.putUChar("ledbright", ledBrightness);
  const uint32_t strip = static_cast<uint32_t>(homeStripSource) |
                         (static_cast<uint32_t>(homeStripField[0]) << 8) |
                         (static_cast<uint32_t>(homeStripField[1]) << 16) |
                         (static_cast<uint32_t>(homeStripField[2]) << 24);
  prefs.putUInt("hstrip", strip);
  prefs.putString("place", placeLabel);
  prefs.remove("city");  // left over from before the switch to ZIP codes
  prefs.end();
}
