// User settings entered through the setup portal and stored in the ESP32's
// flash ("NVS", via the Preferences library), so they survive power cuts.
//
// The Wi-Fi network and password are NOT stored here: the ESP32's Wi-Fi
// system keeps those itself.
#pragma once
#include <Arduino.h>

enum class PetAnimal : uint8_t { None = 0, Fox = 1, Raccoon = 2, Owl = 3, Bear = 4 };
enum class PetMode : uint8_t { Off = 0, Idle = 1, Reacts = 2 };

struct Settings {
  String name;             // used in the greeting: "Morning, <name>!"
  String zip;              // weather location (US ZIP / postcode), e.g. "60601"
  String weatherKey;       // the user's own WeatherAPI.com API key
  String wdgKey;           // WDGWars API key; the badge page shows that account's stats
  String wigleName;        // WiGLE API name; never printed
  String wigleToken;       // WiGLE API token; never printed
  String wardriftUser;     // Wardrift username
  String wardriftPass;     // Wardrift password; never printed
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

  // Filled in once WeatherAPI confirms the ZIP, e.g. "Chicago, Illinois".
  // Empty = not checked yet (or ZIP/key changed since).
  String placeLabel;

  void load();
  void save() const;

  // The minimum we need before the pages make sense.
  bool isComplete() const {
    return name.length() > 0 && zip.length() > 0 && weatherKey.length() > 0;
  }
};
