```
 ██████╗██╗   ██╗██████╗ ███████╗███████╗██╗  ██╗
██╔════╝╚██╗ ██╔╝██╔══██╗██╔════╝██╔════╝██║ ██╔╝
██║      ╚████╔╝ ██║  ██║█████╗  ███████╗█████╔╝ 
██║       ╚██╔╝  ██║  ██║██╔══╝  ╚════██║██╔═██╗ 
╚██████╗   ██║   ██████╔╝███████╗███████║██║  ██╗
 ╚═════╝   ╚═╝   ╚═════╝ ╚══════╝╚══════╝╚═╝  ╚═╝
```

A desk clock for the Cheap Yellow Display: a small ESP32 with a color touchscreen. It shows the time, the weather, a pixel pet, and a few optional accounts. Tap the screen to move to the next page. Hold for about five seconds to change the Wi-Fi network.

CYDesk supports the 2.8" resistive Cheap Yellow Display, the NiceMCU 2.8" capacitive board (ST7789 + CST816D), the 3.5" / 4.0" resistive Cheap Yellow Displays, and the 3.5" / 4.0" capacitive GT911 variants on classic ESP32 (ST7796). The 2.8" resistive board is the one that has been lived with day to day. Capacitive 2.8" C keeps the same 320×240 landscape layout. The taller boards use the same pages and settings on a 480×320 landscape panel; icons bump a little more on the 4" glass. The no-touch E32N40T and the Guition 480×480 ESP32-S3 (4848S040) are not supported.

## Put it on a board

Open the installer in **Chrome, Edge, or Firefox** on a computer (not a phone, and not Safari):

**[https://jazzactual.github.io/CYDesk/](https://jazzactual.github.io/CYDesk/)**

1. Plug the board in with a **data** USB cable. A charge-only cable will not show a port.
2. These boards use a **CH340** USB chip. Windows and Mac need the driver from [WCH](https://github.com/WCH-IC/download/releases/tag/CH341) before a port appears. Run `CH341SER.EXE` on Windows, or install `CH341SER_MAC.ZIP` on a Mac (allow it under System Settings → Privacy & Security if macOS asks). Linux already has the driver. Unplug the board and plug it back in after installing.
3. On the installer page, pick the size printed on the back of the board from the list, then **Install or update**. The port is usually named USB Serial, `usbserial`, or `wchusbserial`.
4. Leave **Erase device** unchecked unless you want a blank board (that keeps Wi‑Fi and keys; Erase clears them).

Installing the wrong size will not damage the board. The picture will look wrong until you install the matching one.

## Updating firmware

Three ways to get a new build onto a board that already runs CYDesk. Prefer the first that fits; USB and espota are backups if Wi‑Fi update fails.

### Prerequisites

- The desk must already be on a recent CYDesk image with **two OTA app slots** (`min_spiffs` partition table). Older “huge app / no OTA” builds need **one USB Install or update first** (Erase **unchecked**) so the partition table migrates. After that, in-app and espota work.
- Pick the **matching board** (2.8 R / 2.8 C / 3.5 R / 3.5 C / 4.0 R / 4.0 C). Wrong choice will not brick the board; the picture or touch will look wrong until you flash the right one.
- Settings and status live at **http://cydesk.local** once the desk is on your 2.4 GHz Wi‑Fi (or use the IP shown on Status / the setup QR). Two boards on the same LAN both answer `cydesk.local` — use the IP if that happens.

### 1. Web flasher over USB

Same page as first install: [https://jazzactual.github.io/CYDesk/](https://jazzactual.github.io/CYDesk/)

1. Plug in a **data** USB cable; use Chrome, Edge, or Firefox on a computer.
2. Choose your board size → **Install or update** → pick the serial port.
3. **Erase device unchecked** — updates firmware and **keeps** saved Wi‑Fi and API keys. The installer writes bootloader, partition table, and app only; it does not touch the NVS settings region. Use this for normal upgrades and for the one-time partition migrate from older “huge app” builds (NVS stays at the same flash address).
4. **Erase device checked** — full wipe. Use only when you want a blank board (clears Wi‑Fi and keys). You’ll go through the `CYDesk` portal and settings again afterward.

### 2. In-app update (Settings → Firmware)

No laptop after the desk is on Wi‑Fi and running a build that includes the Firmware UI.

1. Open **http://cydesk.local** (or the IP).
2. Scroll to **Firmware** → **Check for updates**.
3. If a newer build is listed, **Update now** (or cancel). Watch the desk for progress; keep power on. It reboots when finished.

The board fetches its variant from the published channel on GitHub Pages (same site as the installer), for example:

- 2.8″ R — `https://jazzactual.github.io/CYDesk/cyd28/channel.json` → `app.bin`
- 2.8″ C (NiceMCU) — `…/nicemcu28/channel.json`
- 3.5″ R — `…/cyd35/channel.json`
- 3.5″ C — `…/cyd35c/channel.json`
- 4.0″ R — `…/cyd40/channel.json`
- 4.0″ C — `…/cyd40c/channel.json`

Status shows the build id and whether an update is ready. If Check fails (offline channel, wrong board in the JSON, no Wi‑Fi), use USB or espota below.

### 3. PlatformIO espota (laptop on the same Wi‑Fi)

From a machine that can build the project, push over the network. Password is **`cydesk`**. Use the env that matches the panel:

```
pio run -e e32r28t -t upload --upload-protocol espota --upload-port cydesk.local --upload-flags --auth=cydesk
```

Use `-e nicemcu28`, `-e cyd35`, `-e cyd35c`, `-e cyd40`, or `-e cyd40c` for those boards. If `cydesk.local` does not resolve, pass the desk’s IP as `--upload-port` instead.

### Caveats

- In-app Check needs the channel live on Pages (published when `flasher/` lands on `main`).
- TLS for weather and the update channel uses the same relaxed HTTPS trust as the rest of CYDesk; treat updates like any other LAN/settings access.
- E32N40T and Guition 4848S040 (ESP32-S3) are not supported builds.

## Pages

Tap anywhere to go to the next one. It wraps around. After a page has been up for 30 seconds, a tap returns to Home. On Home, a tap still moves on. On the settings page, uncheck a screen to skip it. Status always stays in the cycle. If every other box is cleared, Home stays on.

1. **Home.** Greeting, date, and clock. The greeting follows the hour: morning, afternoon, evening, or "Up late". Sunrise and sunset sit above the weather, in time order, with the next one bright and the one that already passed muted. Temperature, the chance of rain, humidity, and a weather icon. By day the sun is drawn for clear and partly cloudy. At night every icon uses the moon phase from the forecast. The desk pet sits in the bottom-left corner. An optional three-line stats strip can sit top-right once you turn it on in settings (Off by default); pick one account (WDGWars, WiGLE, or Wardrift) and three of its numbers. A tap on the strip opens that account's page.

   ![Home](docs/screens/home.png)

2. **Hourly.** The next 12 hours as tiles: hour, icon, temperature, and chance of rain. The current hour uses the accent color. "Weather by WeatherAPI.com" sits along the bottom.

   ![Hourly](docs/screens/hourly.png)

3. **Badge.** One [WDGWars](https://wdgwars.pl/) account: name, gang, badge count, total, Wi-Fi, BLE, aircraft, mesh, and today plus the last 7 days. Asks for a key when none is saved. Read-only. Refreshes about every 15 minutes. A failed check keeps the last numbers and marks them stale.

   ![Badge](docs/screens/badge.png)

4. **WiGLE.** Rank, month rank, and discovery counts (Wi-Fi, cell, Bluetooth, and this month) from a read-only [WiGLE](https://wigle.net/) stats request. The API name and token are saved on the board. One successful check per day, at a time you pick (default 4:00). If the desk is off then, it checks the next time it is on. The last numbers stay up until the next check.

   ![WiGLE](docs/screens/wigle.png)

5. **Wardrift.** One [wardrift.net](https://wardrift.net/) character: name, faction, level, class, networks, routes, threats, and a streak when there is one. The upload API key cannot read the dashboard, so the desk signs in with the username and password. Read-only. Refreshes about every 15 minutes. A failed check keeps the last numbers and marks them stale.

   ![Wardrift](docs/screens/wardrift.png)

6. **Stars.** The first public GitHub repo in the list, as a big star count, with the repo address along the bottom. A thin line of one point per day appears once two local days have been saved (up to about three weeks). No token. Public repos only, written as `owner/repo`.

   ![Stars](docs/screens/stars.png)

7. **Repos.** The same repos as a list, name and count, once a second repo is saved. With only one, this page says to add another. Up to four repos. One request at a time, about every 15 minutes. A failed check keeps the last count.

   ![Repos](docs/screens/repos.png)

8. **Status.** Whether Wi-Fi is up, the weather check, and how much memory is free for a download. This page cannot be turned off.

   ![Status](docs/screens/status.png)

Big counts are shortened (1.2k, 12.5k, 1.25M). A page that is on but has nothing saved shows its empty message instead of disappearing. The pictures above use sample names and numbers.

## First boot

If the board has no Wi-Fi saved, it opens a network called `CYDesk`. Join that from a phone or computer and pick your own network. It has to be **2.4 GHz**. The portal only asks for the network. Everything else is on the settings page, because a phone closes that mini-browser as soon as you switch apps.

When it is on your Wi-Fi, the screen shows a QR code until setup is finished. The code opens the settings page on the board (also at `http://cydesk.local`, or the IP address printed on the screen). The QR code uses the IP address, because `.local` names can take a few seconds to look up.

Add at least:

- the name used in the greeting
- a ZIP code
- a free [WeatherAPI.com](https://www.weatherapi.com/signup.aspx) key

The rest of the settings page is optional. Password boxes start empty after a key is saved. Leave one blank to keep what is already stored.

Those stay on the board. They are not in this repo, and the keys and passwords are not printed in the serial log.

Two boards on the same Wi-Fi both answer to `cydesk.local`. The QR code uses the IP address, which still works.

Hold a finger on the screen for about five seconds any time you want to change the Wi-Fi network. A dropped link does not open the portal by itself. On a mesh, the board looks for any node of the saved network name, not only the first radio it joined.

## Desk pet

A small pixel animal in the corner of Home: fox, raccoon, owl, or bear, or none. Off, idle, or reacting. Reactions, in order: asleep at night, an umbrella when rain or snow is likely, sunglasses when it is clear, otherwise a blink or a small wiggle. The default, before you open settings, is a fox that reacts.

## Settings page

Served by the board at `http://cydesk.local` once it is on your Wi-Fi.

- Accent color. Default cyan. It replaces the highlights on the desk and on this page as you move the picker. Pink stays for errors, the umbrella, and the sunset arrow.
- Which screens a tap visits.
- Home strip. Off until you pick WDGWars, WiGLE, or Wardrift, then choose which three metrics from that account show top-right on Home.
- Name, ZIP code, and WeatherAPI key. °F or °C. 12-hour or 24-hour clock. The clock's offset comes from the weather location, including daylight saving.
- WDGWars key.
- WiGLE API name, token, and the time of day to check.
- Wardrift username and password.
- Up to four public GitHub repos, one per line.
- Desk pet mode and animal.
- Firmware. Build id, **Check for updates** against the published channel for this board size, and a short note on USB / espota backups (see [Updating firmware](#updating-firmware)).

Weather refreshes about every 15 minutes and covers the current conditions, sunrise and sunset, and the hourly page in one request.

## Boards

| Size | Board | Screen | Flasher choice |
| --- | --- | --- | --- |
| 2.8" resistive | E32R28T / ESP32-2432S028 | 240×320, ILI9341, XPT2046 | 2.8 inch R |
| 2.8" capacitive | NiceMCU-32S-DEV_2.8IPS | 240×320, ST7789, CST816D | 2.8 inch C |
| 3.5" resistive | ESP32-3248S035**R** | 320×480, ST7796, XPT2046 | 3.5 inch R |
| 3.5" capacitive | ESP32-3248S035**C** | 320×480, ST7796, GT911 | 3.5 inch C |
| 4.0" resistive | E32R40T / ESP32-32E | 320×480, ST7796, XPT2046 | 4.0 inch R |
| 4.0" capacitive | classic ESP32 ST7796 + GT911 | 320×480, ST7796, GT911 | 4.0 inch C |

On the taller boards, Home still anchors weather to the bottom-right (so the 2.8" clock block stays where it was approved), and the hero-stat pages (Badge, WiGLE, Wardrift, Stars) stretch their mid band instead of sliding a fixed 240 px strip down. Hourly tiles and the setup QR grow with the taller screen. The 4.0" panels share the 3.5" pixel count; icons and the QR are a step larger for the bigger glass. Pick **R** vs **C** carefully — resistive and capacitive use different touch chips. Guition 4848S040 (ESP32-S3) is not these builds. If colours look swapped or touch feels dead on a new panel, say so.

## Build from source

[PlatformIO](https://platformio.org/). The 2.8" resistive board is the default, so this flashes whatever is plugged in as that build:

```
pio run -t upload
```

Other boards are separate builds. Only use these when that board is the one plugged in:

```
pio run -e nicemcu28 -t upload
pio run -e cyd35 -t upload
pio run -e cyd35c -t upload
pio run -e cyd40 -t upload
pio run -e cyd40c -t upload
```

`pio run` and the matching `-e` builds also refresh the files the installer page serves. Pushing `main` publishes them.
