// User settings entered through the setup portal and stored in the ESP32's
// flash ("NVS", via the Preferences library), so they survive power cuts.
//
// The Wi-Fi network and password are NOT stored here: the ESP32's Wi-Fi
// system keeps those itself.
#pragma once
#include <Arduino.h>

enum class PetAnimal : uint8_t { None = 0, Fox = 1, Raccoon = 2, Owl = 3, Bear = 4 };
enum class PetMode : uint8_t { Off = 0, Idle = 1, Reacts = 2 };

// Which account feeds the three-line strip on Home. Off hides the strip.
enum class HomeStripSource : uint8_t {
  Off = 0,
  Wdg = 1,
  Wigle = 2,
  Wardrift = 3
};

// Tap order. The bit in Settings::screens matches this index.
enum Screen : uint8_t {
  ScreenHome = 0,
  ScreenHourly,
  ScreenBadge,
  ScreenWigle,
  ScreenWardrift,
  ScreenStars,
  ScreenRepos,
  ScreenStatus,
  ScreenCount
};

struct Settings {
  String name;             // used in the greeting: "Morning, <name>!"
  String zip;              // weather location: postal code (or city / lat,lon for Other)
  String country = "US";   // ISO two-letter country for `zip`; "ZZ" = Other (see location.h)
  String weatherKey;       // the user's own WeatherAPI.com API key
  String wdgKey;           // WDGWars API key; the badge page shows that account's stats
  String wigleName;        // WiGLE API name; never printed
  String wigleToken;       // WiGLE API token; never printed
  String wardriftUser;     // Wardrift username
  String wardriftPass;     // Wardrift password; never printed
  String githubRepos;      // up to four "owner/repo" lines, public repos
  // When to ask WiGLE, as minutes after local midnight. Default 4:00 AM.
  // One successful check per day; the last numbers stay up until the next one.
  uint16_t wigleMinute = 4 * 60;
  bool fahrenheit = true;  // false = Celsius
  bool clock24h = false;   // true = "13:07", false = "1:07 PM"
  // Highlight color, #RRGGBB. Default cyan. Pink is not part of this.
  uint32_t accent = 0x00E5FFu;

  // Desk pet. Defaults show a fox that reacts, so there's something on screen
  // before the settings page is opened.
  uint8_t petAnimal = static_cast<uint8_t>(PetAnimal::Fox);
  uint8_t petMode = static_cast<uint8_t>(PetMode::Reacts);

  // Which screens a tap visits. Every bit starts on. Status is always on.
  // Home is forced on if every other screen is off.
  static constexpr uint8_t SCREENS_ALL = (1u << ScreenCount) - 1;
  uint8_t screens = SCREENS_ALL;
  bool screenOn(uint8_t index) const {
    return index < ScreenCount && (screens & (1u << index)) != 0;
  }

  // Home top-right strip: one source, three fields from that source's metrics.
  // Always starts Off (no strip) until the user picks WiGLE / WDGWars /
  // Wardrift on the settings page. Missing NVS key "hstrip" also means Off.
  uint8_t homeStripSource = static_cast<uint8_t>(HomeStripSource::Off);
  uint8_t homeStripField[3] = {0, 0, 0};
  HomeStripSource stripSource() const {
    return static_cast<HomeStripSource>(homeStripSource);
  }

  // Filled in once WeatherAPI confirms the ZIP, e.g. "Chicago, Illinois".
  // Empty = not checked yet (or ZIP/key changed since).
  String placeLabel;

  // Saved postal-code lookup for countries WeatherAPI can't read directly:
  // "lat,lon", and the "country:code" it belongs to. See location.h.
  String geoQuery;
  String geoFor;

  // Rear RGB LED. Off by default (the LED stayed dark before this setting).
  bool ledOn = false;
  uint32_t ledColor = 0x00E5FFu;  // #RRGGBB
  uint8_t ledBrightness = 40;     // percent, 1-100

  void load();
  void save() const;

  // The minimum we need before the pages make sense.
  bool isComplete() const {
    return name.length() > 0 && zip.length() > 0 && weatherKey.length() > 0;
  }
};
