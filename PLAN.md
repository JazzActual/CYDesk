# CYDesk plan

A desk buddy for two Cheap Yellow Display boards. The 2.8" E32R28T (ESP32-2432S028, 240×320, ILI9341) is the one this was built on. The 3.5" ESP32-3248S035R (320×480, ST7796, resistive touch) is a second build, `pio run -e cyd35`. The capacitive 3.5" (ending in C) and the 4" boards are not supported. Landscape layout anchors the weather block to the bottom-right, so the 2.8" positions stay as approved.

## Pages (tap anywhere to go to the next one; it wraps around)

1. **Home** (`home_page.cpp`):
   ```
   ┌──────────────────────────────────────┐
   │ Morning, Jazz!                       │  greeting (name from settings)
   │ Wednesday, November 4                │  date (14 px)
   │ 10:42 AM                             │  clock
   │                                      │
   │ ┌──────┐              ☀▲ 7:19  ☀▼ 5:40│  previous + next sun (over weather)
   │ │ pet  │           64°   │ icon │   │  72 px weather icon
   │ │      │         ☂ 60%   │      │   │  chance of rain/snow this hour
   │ └──────┘         90%RH   └──────┘   │  relative humidity
   └──────────────────────────────────────┘
   ```
   - Greeting by hour: Morning 5–11, Afternoon 12–16, Evening 17–21, "Up late, <name>?" otherwise.
   - Clock: 12-hour (default) or 24-hour, chosen on the settings page.
   - Sun events: two columns above the weather block, right-aligned, in time order. The past
     event is muted and the next one is bright. By day that's sunrise | sunset; after sunset
     it's sunset | tomorrow's sunrise; before sunrise, yesterday's sunset is estimated as
     today's minus 24 h.
   - Weather icon (`weather_icons.cpp`): 11 kinds drawn from smooth primitives at 72×72 (designed
     at 96 and scaled). By day the sun is drawn only for clear and partly cloudy; the other
     icons are the weather alone. Every icon shows the moon at night. The phase comes from
     today's astronomy data (0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter), with cloud,
     rain, snow, sleet, fog and lightning layers on top. `/icons` on
     the settings server steps through every icon, day and night.
   - **Desk pet, bottom left (see below).**
2. **Hourly** (`hourly_page.cpp`): the next 12 hours as 6×2 tiles. Each tile is the hour
   (cyan when it is the current hour), a 40 px icon, the temperature and the chance of rain.
   Night icons use today's moon phase. "Weather by WeatherAPI.com" sits along the bottom.
3. **Badge** (`badge_page.cpp`): the page draws one WDGWars account's stats (handle, gang, total,
   WiFi, BLE, aircraft, mesh, badge count, today and the last 7 days) from a read-only
   `GET /api/me`. The key is stored like the weather key and is never printed. The board does not
   upload anything. Refreshes every 15 minutes. A failed refresh keeps the last result and marks
   it stale. Nothing saved: "No WDGWars key yet". A picture badge can come back later (step 10).
4. **WiGLE** (`wigle_page.cpp`): one account's rank, month rank, and discovery counts
   (Wi-Fi, cell, Bluetooth, plus this month's uploads) from a read-only
   `GET https://api.wigle.net/api/v2/stats/user`. The API name and token are HTTP Basic
   auth, stored like the other keys, and never printed. The board does not upload anything.
   Checks once a day at a time chosen on the settings page (default 4:00). If the desk is
   off then, it checks once the next time it is on. A failed check retries every 15 minutes
   until one works; a rate limit waits until the next day. The last result stays on screen.
   Nothing saved: "No WiGLE login yet".
5. **Wardrift** (`wardrift_page.cpp`): one character's level, faction, class, and lifetime
   wardrive counts from a read-only `GET https://wardrift.net/v1/dashboard`. The API key
   cannot read that page, so the desk signs in with the saved username and password and
   sends the session token. Neither secret is printed. The board does not upload anything.
   Refreshes every 15 minutes. A failed refresh keeps the last result and marks it stale.
   Nothing saved: "No Wardrift login yet".
6. **Stars** (`github_page.cpp`): the first public `owner/repo` as a big count, from a
   read-only `GET https://api.github.com/repos/<owner>/<repo>`. No token. The address sits
   along the bottom. One star count per local day (up to 21 days) is drawn as a thin line
   once two days are saved. Nothing saved: "No repos yet".
7. **Repos** (`github_page.cpp`): the list of saved repos, name and star count. It asks for
   a second repo until two are saved. Same fetch as Stars: every 15 minutes, one request
   at a time, up to four repos. A failed check keeps the last count.
8. **Status** (temporary diagnostics): saved settings, Wi-Fi and the weather check result.

## Desk pet

- Pixel art: a 20×20 animal on a 26×26 canvas (room for an umbrella and "z z"), drawn at 3×
  (78 px) in the bottom-left corner. Animals: **fox, raccoon, owl, bear**, chosen on the settings page.
- Accessories are overlays shared by all animals.
- **Reactions** (priority order): asleep at night (eyes closed, "z z") > umbrella when this hour's
  rain/snow chance is over 50% > sunglasses when it's sunny (clear by day) > idle.
  The idle animation is a blink or small wiggle, swapping frames every ~0.5 s and redrawing only
  the pet's corner from an off-screen sprite.
- Setting: **Desk pet: Off / Idle / Reacts to weather**, plus the animal (None / Fox / Raccoon / Owl / Bear).
  Default is a fox that reacts, so the corner isn't empty before setup.
- More reactions can come later (e.g. a scarf below freezing).
- The approved pixel designs live in the local preview script (see `CLAUDE.local.md`). Port them
  as-is.

Holding a finger on the screen for about 5 seconds reopens the Wi-Fi portal.

## Settings: two-part setup

1. **Wi-Fi portal** (`portal.cpp`, WiFiManager): opens when no Wi-Fi is saved, or after a
   5-second hold. The board starts a network called `DeskBuddy-Setup`, and the page that opens
   on the phone asks only for the home Wi-Fi network and password (2.4 GHz).
   *Why it's kept this small:* the iPhone shows this page in a mini-browser that closes, and
   leaves the network, as soon as you switch apps. So it must never need copying and pasting.
   The board joins that network by name, not by one mesh node's radio address. If the node
   drops it, it looks for any other node of the same network. A lost link does not open
   this portal; a 5-second hold still does. After the radio settles, the screen is drawn
   again: a scan can mirror the panel, and the setup page was being painted with the
   clock font (digits only), so letters came out as empty boxes.
2. **Settings page** (`settings_web.cpp`): served by the board on the home network at
   `http://cydesk.local` (or its IP). It holds the name, ZIP code, WeatherAPI key, WDGWars key,
   °F or °C, and the 12/24-hour clock (later also the desk pet). While anything is missing, the screen shows a QR code that opens the page. The QR
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
  - One `forecast.json?days=2` request every 15 minutes covers current conditions, sunrise and
    sunset for today and tomorrow, and 48 hourly forecasts (rain chance = max of rain and snow).
- **WDGWars**: one read-only `GET https://wdgwars.pl/api/me` every 15 minutes, after the weather
  fetch, so only one HTTPS download runs at a time. The key goes in a header and is never printed.
- **WiGLE**: one read-only `GET https://api.wigle.net/api/v2/stats/user` per local day, at the
  time saved on the settings page, after the WDGWars fetch. HTTP Basic auth with the API name
  and token. The fields drawn are `user`,
  `rank`, `monthRank`, and `statistics.discoveredWiFi`, `discoveredCell`, `discoveredBt`, and
  `eventMonthCount`. Neither credential is printed.
- **Wardrift**: one read-only `GET https://wardrift.net/v1/dashboard` every 15 minutes, after the
  WiGLE check. Sign-in is `POST /v1/auth/sign-in/username`; the session token is sent as
  `Authorization: Bearer`. The password and token are never printed. The fields drawn
  are the first character's `name`, `level`, `class_name`, `faction_id`, `currency`, and
  `wardrive.lifetime` routes, access points, and threats, plus `streak_days`.
- **GitHub**: one read-only `GET https://api.github.com/repos/<owner>/<repo>` at a time, after
  Wardrift, for up to four public repos. No token. The field drawn is `stargazers_count`.
  Each repo refreshes every 15 minutes. A failed check keeps the last count.

## Build steps (each one ends with something visible on the screen, then a commit)

1. [x] Scaffold + hello screen: confirmed ILI9341 (ILI9341_2_DRIVER), colours correct, rotation 1, uploads at 460800 baud
2. [x] Touch (XPT2046 on VSPI, display moved to HSPI): tap cycles pages, 5 s hold with progress bar
3. [x] Setup: Wi-Fi-only portal, settings page at cydesk.local with a QR code, ZIP + WeatherAPI key check
4. [x] Home: clock, date, greeting in Rajdhani smooth fonts (tools/make_fonts.py); clock offset from WeatherAPI, saved in flash
5. [x] Weather on Home: icon set (+ /icons test), temperature, rain chance, %RH, sunrise/sunset
5b. [x] Desk pet: fox / raccoon / owl / none, reactions, settings (Off / Idle / Reacts + animal)
6. [x] Moon phase with the weather layers drawn on top
7. [x] Hourly page
8. [ ] Badge page: WDGWars stats from a saved key
9. [ ] Polish: offline/stale markers, memory check, fallbacks, optional 4-digit code for the settings page, Wi-Fi portal: the page is slow to pop up after joining DeskBuddy-Setup (the phone's connectivity checks hit "request handler not found"; try answering them directly or pre-scanning networks before opening the access point)
10. [ ] Later: picture badge again. An image embed or a bare image address, fitted through wsrv.nl and drawn with TJpg_Decoder. Shown when no WDGWars key is saved. Taken out so the badge page is WDGWars only for now.

## Next batch (added 2026-11-04, not started)

Do the screen toggles first, so each new page can be switched off as it lands. BLE is last: it shares the radio with Wi-Fi.

11. [x] **WiGLE stats page.** Rank, month rank, and discovery counts (Wi-Fi, cell, Bluetooth, this month) from a read-only `GET /api/v2/stats/user`. API name and token are HTTP Basic auth, stored in flash, never printed. One successful check per day, at a time chosen on the settings page. Counts on this page and the WDGWars badge are shortened (1.2k, 12.5k, 1.25M). Empty state when no login is saved.
12. [x] **Bear** on the desk pet list, same 20×20 mirrored left-half style as the fox, raccoon, and owl (tail overlay, shared accessories). Chosen next to the other animals on the settings page.
13. [x] **Toggle screens.** Home, Hourly, Badge, WiGLE, Wardrift, Stars, and Repos each have a checkbox on the settings page and are skipped when off, including their fetch. Status always stays in the cycle. A page that is on but has no key still shows its empty state. If every box is cleared, Home stays on so the desk always has a page.
14. [x] **GitHub star tracker.** Up to four public `owner/repo` names. `GET /repos/<owner>/<repo>` returns `stargazers_count` with no token (60 requests an hour; a few repos every 15 minutes fits). Stars is the first repo as a big count, with the address and a daily line. Repos is the list, shown once a second repo is saved. One request at a time. A failed check keeps the last count. A token for private repos was left out.
15. [ ] **BLE alerts.** The settings page holds a list of device names or prefixes the user cares about (Axon cameras are the example). The board passive-scans advertisements and shows an alert when one of those names is heard, then clears it after the device goes quiet. Only those matches are kept; the scan does not store or print every device nearby. Wi-Fi and BLE share one radio, so the scan pauses around the HTTPS fetches and must not stall touch. Draw the alert layout as a preview before putting it on the board.
16. [x] **Accent color.** A color picker on the settings page, saved in flash, default cyan (`#00e5ff`). It replaces the cyan accent on every page, including the settings page itself (the page updates as the picker moves). Pink (`#ff2d95`) stays the second accent.

## Look

- Colours come from the WDGWars card: navy background, cyan and pink accents (`theme.h`).
  No cyan stripe along the top edge (the user didn't want it).
- Font: **Rajdhani SemiBold** (SIL OFL; the file and license are in `assets/fonts/`), converted
  to TFT_eSPI smooth fonts by `tools/make_fonts.py`. Sizes: 14 (date, %RH), 16 (sun times, rain
  chance), 24 (greeting, AM/PM), 40 (temperature), 56 (clock). Adding a size means adding a line to `FONTS` and
  rerunning the script.

## Constraints

- About 520 KB of RAM and no PSRAM. An HTTPS connection costs about 40–50 KB, so check
  `ESP.getMaxAllocHeap()` before allocating big buffers. Don't use full-screen sprites
  (one would need 150 KB).
- HTTPS uses `setInsecure()` for now (noted in the code). Pinning the root certificate can come later.
- The main loop avoids blocking: every part checks timing with `millis()`. Known exception: the
  weather fetch blocks for about 2–3 s every 15 minutes, so touch is unresponsive for that moment.
  Moving it to its own FreeRTOS task is a polish item.
