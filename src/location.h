// Turns the saved country + postal code into something WeatherAPI.com
// understands.
//
// WeatherAPI reads US ZIP codes, UK postcodes and Canadian postal codes
// directly. Any other country's code would be misread (a German "10115" is
// taken as a US ZIP), so those are first looked up on zippopotam.us (free,
// no key) and sent to WeatherAPI as "latitude,longitude". The lookup result
// is saved to flash, so it happens once per postal code, not every refresh.
//
// "Other" (country code ZZ) passes whatever was typed straight through:
// a city name ("Dublin"), or "lat,lon" ("53.35,-6.26").
#pragma once
#include <Arduino.h>

#include "settings.h"

namespace location {

struct Country {
  const char* code;     // ISO 3166 two letters, upper case. "ZZ" = Other.
  const char* name;     // shown in the settings menu
  const char* example;  // placeholder for the postal code box
};

constexpr const char* DEFAULT_COUNTRY = "US";
constexpr const char* OTHER = "ZZ";

size_t countryCount();
const Country& countryAt(size_t index);
// nullptr when the code is not in the list.
const Country* findCountry(const String& code);

// True for the countries WeatherAPI reads on its own (US, GB, CA) and Other.
bool nativeToWeatherApi(const String& country);

// Cleans up a typed code into the form the lookup expects, e.g.
// NL "1012 AB" -> "1012", PL "00001" -> "00-001", US "60601-1234" -> "60601".
String normalize(const String& country, const String& code);

// Percent-encodes text for a URL (spaces, commas, accents...).
String urlEncode(const String& in);

// Identifies the country + code a saved lookup belongs to.
String cacheKey(const Settings& settings);

enum class Result : uint8_t {
  Ok,        // `query` is ready for WeatherAPI's q=
  NotFound,  // the lookup service doesn't know this code; retrying won't help
  Failed,    // network trouble; try again later
};

// Fills `query` for WeatherAPI. Uses (and refreshes) the lookup saved in
// `settings`; when a new lookup was needed and worked, settings are saved.
// Needs Wi-Fi for a new lookup. Blocks up to ~8 s.
Result resolve(Settings& settings, String& query, String& error);

}  // namespace location
