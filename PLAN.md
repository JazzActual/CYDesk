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

Holding a finger on the screen for about 5 seconds opens the setup portal.

## Settings: setup portal

On first boot (or after a long press) the board starts a Wi-Fi network called
`DeskBuddy-Setup`. Joining it from a phone opens a form for:
Wi-Fi network and password (2.4 GHz only), greeting name, city, badge HTML, and °F or °C.
The settings are stored in the ESP32's flash (`Preferences`). Nothing private is kept in the repo.

## Data sources (no API keys)

- **Weather**: Open-Meteo forecast API. It returns the current conditions, the hourly forecast,
  whether it's daytime, and the location's current UTC offset, which the clock uses so daylight
  saving switches over automatically.
- **City to coordinates**: Open-Meteo geocoding API, called once when settings are saved.
- **Badge rendering**: wsrv.nl turns the SVG into a JPEG.
  Checked on 2026-11-04: the WDGWars card renders correctly as a 320×133 baseline JPEG of about
  14 KB. The small labels end up about 6 px tall.

## Build steps (each one ends with something visible on the screen, then a commit)

1. [x] Scaffold + hello screen: confirmed ILI9341 (ILI9341_2_DRIVER), colours correct, rotation 1, uploads at 460800 baud
2. [ ] Touch (XPT2046 on its own SPI bus): cycle 3 placeholder pages, detect a long press
3. [ ] Setup portal, Wi-Fi and saved settings, including the city lookup
4. [ ] Home: clock, date, greeting
5. [ ] Weather fetch + home weather block + icon set (with a test mode that steps through every icon)
6. [ ] Moon phase with the weather layers drawn on top
7. [ ] Hourly page
8. [ ] Badge page (wsrv.nl → TJpg_Decoder)
9. [ ] Polish: offline/stale markers, memory check, fallbacks

## Constraints

- About 520 KB of RAM and no PSRAM. An HTTPS connection costs about 40–50 KB, so check
  `ESP.getMaxAllocHeap()` before allocating big buffers. Don't use full-screen sprites
  (one would need 150 KB).
- HTTPS uses `setInsecure()` for now (noted in the code). Pinning the root certificate can come later.
- The main loop never blocks: every part checks timing with `millis()`.
