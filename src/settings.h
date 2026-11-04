// User settings entered through the setup portal and stored in the ESP32's
// flash ("NVS", via the Preferences library), so they survive power cuts.
//
// The Wi-Fi network and password are NOT stored here: the ESP32's Wi-Fi
// system keeps those itself.
#pragma once
#include <Arduino.h>

struct Settings {
  String name;             // used in the greeting: "Morning, <name>!"
  String city;             // as the user typed it, e.g. "Chicago, IL"
  String badgeUrl;         // image address taken from the pasted badge HTML
  bool fahrenheit = true;  // false = Celsius

  // Filled in by the city lookup (step 3b). NAN = not looked up yet.
  float latitude = NAN;
  float longitude = NAN;
  String placeLabel;       // e.g. "Chicago, Illinois, US"

  void load();
  void save() const;

  // The minimum we need before the pages make sense.
  bool isComplete() const { return name.length() > 0 && city.length() > 0; }
};

// Takes whatever the user pasted for the badge and returns just the image
// address. Accepts a full HTML embed like
//   <a href="..."><img src="https://example.com/card.svg" ...></a>
// or a bare URL. Returns "" if no address could be found.
String extractImageUrl(const String& htmlOrUrl);
