#include "location.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace location {
namespace {

constexpr const char* LOOKUP_HOST = "api.zippopotam.us";
constexpr uint32_t HTTP_TIMEOUT_MS = 8000;

// US, UK and Canada first (most likely picks), then every other country
// zippopotam.us has postal codes for, A-Z by name, then Other.
const Country COUNTRIES[] = {
    {"US", "United States", "60601"},
    {"GB", "United Kingdom", "SW1A 1AA"},
    {"CA", "Canada", "K1A 0B1"},
    {"AS", "American Samoa", "96799"},
    {"AD", "Andorra", "AD500"},
    {"AR", "Argentina", "5000"},
    {"AU", "Australia", "2000"},
    {"AT", "Austria", "1010"},
    {"BD", "Bangladesh", "1000"},
    {"BE", "Belgium", "1000"},
    {"BR", "Brazil", "01000-000"},
    {"BG", "Bulgaria", "1000"},
    {"HR", "Croatia", "10000"},
    {"CZ", "Czech Republic", "110 00"},
    {"DK", "Denmark", "1050"},
    {"DO", "Dominican Republic", "10101"},
    {"FO", "Faroe Islands", "100"},
    {"FI", "Finland", "00100"},
    {"FR", "France", "75001"},
    {"GF", "French Guiana", "97300"},
    {"DE", "Germany", "10115"},
    {"GL", "Greenland", "3900"},
    {"GP", "Guadeloupe", "97100"},
    {"GU", "Guam", "96910"},
    {"GT", "Guatemala", "01001"},
    {"GG", "Guernsey", "GY1"},
    {"HU", "Hungary", "1011"},
    {"IS", "Iceland", "101"},
    {"IN", "India", "110001"},
    {"IM", "Isle of Man", "IM1"},
    {"IT", "Italy", "20121"},
    {"JP", "Japan", "100-0001"},
    {"JE", "Jersey", "JE2"},
    {"LI", "Liechtenstein", "9490"},
    {"LT", "Lithuania", "01001"},
    {"LU", "Luxembourg", "L-1009"},
    {"MY", "Malaysia", "50000"},
    {"MH", "Marshall Islands", "96960"},
    {"MQ", "Martinique", "97200"},
    {"YT", "Mayotte", "97600"},
    {"MX", "Mexico", "06000"},
    {"MD", "Moldova", "MD-2000"},
    {"MC", "Monaco", "98000"},
    {"NL", "Netherlands", "1012"},
    {"NZ", "New Zealand", "6011"},
    {"MK", "North Macedonia", "1000"},
    {"MP", "Northern Mariana Islands", "96950"},
    {"NO", "Norway", "0150"},
    {"PK", "Pakistan", "44000"},
    {"PH", "Philippines", "1000"},
    {"PL", "Poland", "00-001"},
    {"PT", "Portugal", "1000-001"},
    {"PR", "Puerto Rico", "00901"},
    {"RE", "Reunion", "97400"},
    {"RU", "Russia", "101000"},
    {"PM", "Saint Pierre and Miquelon", "97500"},
    {"SM", "San Marino", "47890"},
    {"SK", "Slovakia", "811 01"},
    {"SI", "Slovenia", "1000"},
    {"ZA", "South Africa", "2000"},
    {"ES", "Spain", "28001"},
    {"LK", "Sri Lanka", "00100"},
    {"SJ", "Svalbard and Jan Mayen", "9170"},
    {"SE", "Sweden", "11120"},
    {"CH", "Switzerland", "8001"},
    {"TH", "Thailand", "10200"},
    {"TR", "Turkey", "06100"},
    {"VI", "U.S. Virgin Islands", "00802"},
    {"VA", "Vatican City", "00120"},
    {"ZZ", "Other (city name or lat,lon)", "Dublin  or  53.35,-6.26"},
};
constexpr size_t COUNTRY_COUNT = sizeof(COUNTRIES) / sizeof(COUNTRIES[0]);

// Only the digits, e.g. "00 001" -> "00001".
String digitsOnly(const String& in) {
  String out;
  for (char c : in) {
    if (isdigit(static_cast<unsigned char>(c))) out += c;
  }
  return out;
}

String withoutSpaces(const String& in) {
  String out;
  for (char c : in) {
    if (c != ' ') out += c;
  }
  return out;
}

// "GY1 1AA" -> "GY1". The lookup only knows the first half of these codes.
String outwardCode(const String& in) {
  const int space = in.indexOf(' ');
  if (space > 0) return in.substring(0, space);
  // Typed without a space: the inward half is always the last three characters.
  if (in.length() >= 5) return in.substring(0, in.length() - 3);
  return in;
}

struct Lookup {
  int status = 0;   // HTTP status, or a negative HTTPClient error
  String latLon;    // "52.5323,13.3872" when status is 200
};

// One GET to zippopotam.us. Plain HTTP needs very little memory; HTTPS is a
// fallback in case the service ever stops answering on port 80.
Lookup fetchOnce(bool secure, const String& path) {
  Lookup out;
  WiFiClient plain;
  WiFiClientSecure tls;
  if (secure) tls.setInsecure();  // public, non-secret data; same choice as the weather
  WiFiClient& client = secure ? static_cast<WiFiClient&>(tls) : plain;

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.useHTTP10(true);  // no chunked replies, so JSON can be read straight off the stream
  const String url = String(secure ? "https://" : "http://") + LOOKUP_HOST + path;
  if (!http.begin(client, url)) {
    out.status = -1;
    return out;
  }
  http.setUserAgent("CYDesk");
  out.status = http.GET();
  if (out.status != 200) {
    http.end();
    return out;
  }

  JsonDocument filter;
  JsonObject placeFilter = filter["places"].add<JsonObject>();
  placeFilter["latitude"] = true;
  placeFilter["longitude"] = true;
  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    out.status = -2;
    return out;
  }
  // The first place is enough: every place under one code is close together.
  JsonObject place = doc["places"][0];
  String lat = place["latitude"] | "";
  String lon = place["longitude"] | "";
  lat.trim();
  lon.trim();
  if (lat.isEmpty() || lon.isEmpty()) {
    out.status = 404;  // a reply with no places is the same as "not found"
    return out;
  }
  out.latLon = lat + "," + lon;
  return out;
}

Lookup lookup(const String& country, const String& code) {
  String cc = country;
  cc.toLowerCase();
  const String path = "/" + cc + "/" + urlEncode(code);
  Lookup result = fetchOnce(false, path);
  // Connection trouble or a redirect: try HTTPS, if there's room for it.
  const bool retry = result.status <= 0 || (result.status >= 300 && result.status < 400);
  if (retry && ESP.getMaxAllocHeap() >= 40 * 1024) {
    Serial.printf("[place] HTTP lookup gave %d, trying HTTPS\n", result.status);
    result = fetchOnce(true, path);
  }
  return result;
}

}  // namespace

size_t countryCount() { return COUNTRY_COUNT; }
const Country& countryAt(size_t index) {
  return COUNTRIES[index < COUNTRY_COUNT ? index : 0];
}

const Country* findCountry(const String& code) {
  for (const Country& c : COUNTRIES) {
    if (code.equalsIgnoreCase(c.code)) return &c;
  }
  return nullptr;
}

bool nativeToWeatherApi(const String& country) {
  return country == "US" || country == "GB" || country == "CA" || country == OTHER;
}

String normalize(const String& country, const String& code) {
  String in = code;
  in.trim();
  // Other: a city name or coordinates. Leave the text as typed.
  if (country == OTHER) return in;

  // Upper case, and squeeze runs of spaces down to one.
  String s;
  for (char c : in) {
    if (c == ' ' && (s.isEmpty() || s.charAt(s.length() - 1) == ' ')) continue;
    s += static_cast<char>(toupper(static_cast<unsigned char>(c)));
  }
  s.trim();

  const String digits = digitsOnly(s);
  if (country == "US") {
    // ZIP+4 ("60601-1234") -> the five-digit ZIP.
    if (digits.length() == 5 || digits.length() == 9) return digits.substring(0, 5);
    return s;
  }
  if (country == "GB" || country == "CA") return s;  // WeatherAPI reads these as typed
  if (country == "NL") return digits.length() >= 4 ? digits.substring(0, 4) : s;  // "1012 AB"
  if (country == "CZ" || country == "SK") {
    return digits.length() == 5 ? digits.substring(0, 3) + " " + digits.substring(3) : s;
  }
  if (country == "PL") {
    return digits.length() == 5 ? digits.substring(0, 2) + "-" + digits.substring(2) : s;
  }
  if (country == "PT") {
    return digits.length() == 7 ? digits.substring(0, 4) + "-" + digits.substring(4) : s;
  }
  if (country == "JP") {
    return digits.length() == 7 ? digits.substring(0, 3) + "-" + digits.substring(3) : s;
  }
  if (country == "BR") {
    if (digits.length() == 8) return digits.substring(0, 5) + "-" + digits.substring(5);
    if (digits.length() == 5) return digits + "-000";
    return s;
  }
  if (country == "LU") return digits.length() == 4 ? "L-" + digits : s;
  if (country == "MD") return digits.length() == 4 ? "MD-" + digits : s;
  if (country == "AD") return digits.length() == 3 ? "AD" + digits : s;
  if (country == "GG" || country == "JE" || country == "IM") return outwardCode(s);
  // Everyone else: the lookup stores the code without spaces ("114 55" -> "11455").
  return withoutSpaces(s);
}

String urlEncode(const String& in) {
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  String out;
  out.reserve(in.length() * 3);
  for (char ch : in) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += HEX_DIGITS[c >> 4];
      out += HEX_DIGITS[c & 0x0F];
    }
  }
  return out;
}

String cacheKey(const Settings& settings) {
  return settings.country + ":" + normalize(settings.country, settings.zip);
}

Result resolve(Settings& settings, String& query, String& error) {
  const String code = normalize(settings.country, settings.zip);
  if (code.isEmpty()) {
    error = "No postal code saved";
    return Result::NotFound;
  }
  if (nativeToWeatherApi(settings.country)) {
    query = code;
    return Result::Ok;
  }

  const String key = cacheKey(settings);
  if (settings.geoFor == key && settings.geoQuery.length()) {
    query = settings.geoQuery;
    return Result::Ok;
  }
  if (WiFi.status() != WL_CONNECTED) {
    error = "No Wi-Fi";
    return Result::Failed;
  }

  // The postal code is not printed: the log shows no places.
  Serial.printf("[place] looking up a %s postal code\n", settings.country.c_str());
  Lookup found = lookup(settings.country, code);
  // Brazil's list mostly has the "-000" code for each area. Try that too.
  if (found.status == 404 && settings.country == "BR" && code.length() == 9 &&
      !code.endsWith("-000")) {
    found = lookup(settings.country, code.substring(0, 5) + "-000");
  }

  if (found.status == 200) {
    settings.geoQuery = found.latLon;
    settings.geoFor = key;
    settings.save();
    query = found.latLon;
    Serial.println("[place] found");
    return Result::Ok;
  }
  if (found.status == 404) {
    error = "Postal code not found";
    Serial.println("[place] not found");
    return Result::NotFound;
  }
  error = "Can't reach postcode lookup";
  Serial.printf("[place] lookup failed (%d)\n", found.status);
  return Result::Failed;
}

}  // namespace location
