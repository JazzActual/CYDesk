// User settings entered through the setup portal and stored in the ESP32's
// flash ("NVS", via the Preferences library), so they survive power cuts.
//
// The Wi-Fi network and password are NOT stored here: the ESP32's Wi-Fi
// system keeps those itself.
#pragma once
#include <Arduino.h>

struct Settings {
  String name;             // used in the greeting: "Morning, <name>!"
  String zip;              // weather location (US ZIP / postcode), e.g. "60601"
  String weatherKey;       // the user's own WeatherAPI.com API key
  String badgeUrl;         // image address taken from the pasted badge HTML
  bool fahrenheit = true;  // false = Celsius

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
