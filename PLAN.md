# CYDesk plan

A desk buddy for the E32R28T 2.8" ESP32 touchscreen (240×320, ESP32-WROOM-32E, no PSRAM).

## Pages (tap anywhere to go to the next one; it wraps around)

1. **Home**: greeting with the user's name ("Morning, Jazz!"), local time (12-hour), date,
   temperature, humidity and a weather icon.
   - Day: sun plus a weather layer (cloud, rain, snow, fog, storm).
   - Night: the current moon phase (calculated on the board) plus the same weather layers.
   - Greeting by hour: Morning 5–11, Afternoon 12–16, Evening 17–21, Night otherwise.
2. **Hourly**: the next 12 hours as 6×2 tiles: hour, small icon, temperature, chance of rain.
3. **Badge**: one user-supplied HTML image badge (e.g. the WDGWars card). The board takes the
   `src` address out of the snippet and fetches it through wsrv.nl as a 320-pixel-wide JPEG,
   refreshing every 10 minutes, with a "stale" marker when a fetch fails.

Holding a finger on the screen for about 5 seconds reopens the Wi-Fi portal.

## Settings: two-part setup

1. **Wi-Fi portal** (`portal.cpp`, WiFiManager): opens when no Wi-Fi is saved, or after a
   5-second hold. The board starts a network called `DeskBuddy-Setup`, and the page that opens
   on the phone asks only for the home Wi-Fi network and password (2.4 GHz).
   *Why it's kept this small:* the iPhone shows this page in a mini-browser that closes, and
   leaves the network, as soon as you switch apps. So it must never need copying and pasting.
2. **Settings page** (`settings_web.cpp`): served by the board on the home network at
   `http://cydesk.local` (or its IP). It holds the name, ZIP code, WeatherAPI key, badge HTML and
   °F or °C. While anything is missing, the screen shows a QR code that opens the page. The QR
   code uses the IP address, because `.local` lookups can take about 5 seconds on Macs (the board
   doesn't answer the IPv6 address lookup).

Settings live in the ESP32's flash (`Preferences`). The API key is never sent back to the
browser or printed to the serial log. Nothing private is kept in the repo.

## Data sources

- **Weather**: WeatherAPI.com, using the user's own free key (100K calls a month, a 3-day hourly
  forecast, astronomy data). The location is given as `q=<ZIP>`. Free users are asked to credit
  WeatherAPI.com, so show "Weather by WeatherAPI.com" on the hourly page.
  - Always call `http.useHTTP10(true)`. Over HTTP/1.1 the reply arrives chunked, and ArduinoJson
    can't read that straight from the network.
  - Clock offset: `location.localtime` minus `location.localtime_epoch` gives the local UTC
    offset, so daylight saving switches over automatically.
- **Badge rendering**: wsrv.nl turns the SVG into a JPEG.
  Checked on 2026-11-04: the WDGWars card renders correctly as a 320×133 baseline JPEG of about
  14 KB. The small labels end up about 6 px tall.

## Build steps (each one ends with something visible on the screen, then a commit)

1. [x] Scaffold + hello screen: confirmed ILI9341 (ILI9341_2_DRIVER), colours correct, rotation 1, uploads at 460800 baud
2. [x] Touch (XPT2046 on VSPI, display moved to HSPI): tap cycles pages, 5 s hold with progress bar
3. [x] Setup: Wi-Fi-only portal, settings page at cydesk.local with a QR code, ZIP + WeatherAPI key check
4. [ ] Home: clock, date, greeting
5. [ ] Weather fetch + home weather block + icon set (with a test mode that steps through every icon)
6. [ ] Moon phase with the weather layers drawn on top
7. [ ] Hourly page
8. [ ] Badge page (wsrv.nl → TJpg_Decoder)
9. [ ] Polish: offline/stale markers, memory check, fallbacks, optional 4-digit code for the settings page, Wi-Fi portal: the page is slow to pop up after joining DeskBuddy-Setup (the phone's connectivity checks hit "request handler not found"; try answering them directly or pre-scanning networks before opening the access point)

## Constraints

- About 520 KB of RAM and no PSRAM. An HTTPS connection costs about 40–50 KB, so check
  `ESP.getMaxAllocHeap()` before allocating big buffers. Don't use full-screen sprites
  (one would need 150 KB).
- HTTPS uses `setInsecure()` for now (noted in the code). Pinning the root certificate can come later.
- The main loop never blocks: every part checks timing with `millis()`.
