// User settings entered through the setup portal and stored in the ESP32's
// flash ("NVS", via the Preferences library), so they survive power cuts.
//
// The Wi-Fi network and password are NOT stored here: the ESP32's Wi-Fi
// system keeps those itself.
#pragma once
#include <Arduino.h>

enum class PetAnimal : uint8_t { None = 0, Fox = 1, Raccoon = 2, Owl = 3 };
enum class PetMode : uint8_t { Off = 0, Idle = 1, Reacts = 2 };

struct Settings {
  String name;             // used in the greeting: "Morning, <name>!"
  String zip;              // weather location (US ZIP / postcode), e.g. "60601"
  String weatherKey;       // the user's own WeatherAPI.com API key
  String badgeUrl;         // image address taken from the pasted badge HTML
  bool fahrenheit = true;  // false = Celsius
  bool clock24h = false;   // true = "13:07", false = "1:07 PM"

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

// Takes whatever the user pasted for the badge and returns just the image
// address. Accepts a full HTML embed like
//   <a href="..."><img src="https://example.com/card.svg" ...></a>
// or a bare URL. Returns "" if no address could be found.
String extractImageUrl(const String& htmlOrUrl);
