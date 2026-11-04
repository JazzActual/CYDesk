# CYDesk

A desk clock for the Cheap Yellow Display: a small ESP32 with a color touchscreen. It shows the time, the weather, a pixel pet, and optionally a WDGWars account. Tap the screen to move to the next page. Hold for about five seconds to change the Wi-Fi network.

Work in progress. The 2.8" board is the one this was built on.

## Put it on a board

Open the installer in **Chrome, Edge, or Firefox** on a computer (not a phone, and not Safari):

**[https://jazzactual.github.io/CYDesk/](https://jazzactual.github.io/CYDesk/)**

1. Plug the board in with a **data** USB cable. A charge-only cable will not show a port.
2. These boards use a **CH340** USB chip. Windows and Mac need the driver from [WCH](https://github.com/WCH-IC/download/releases/tag/CH341) before a port appears. Run `CH341SER.EXE` on Windows, or install `CH341SER_MAC.ZIP` on a Mac (allow it under System Settings → Privacy & Security if macOS asks). Linux already has the driver. Unplug the board and plug it back in after installing.
3. On the installer page, click the button for the size printed on the back of the board. The port is usually named USB Serial, `usbserial`, or `wchusbserial`.
4. Leave **Erase device** unchecked unless you want a blank board. Erase clears the saved Wi-Fi and keys.

Installing the wrong size will not damage the board. The picture will look wrong until you install the matching one.

## First boot

If the board has no Wi-Fi saved, it opens a network called `DeskBuddy-Setup`. Join that from a phone or computer and pick your own network. It has to be **2.4 GHz**.

When it is on your Wi-Fi, the screen shows a QR code until setup is finished. The code opens the settings page on the board (also at `http://cydesk.local`, or the IP address printed on the screen). Add:

- the name used in the greeting
- a ZIP code
- a free [WeatherAPI.com](https://www.weatherapi.com/signup.aspx) key
- an optional WDGWars key, if you want that account's stats

Those stay on the board. They are not in this repo, and the weather and WDGWars keys are not sent back to the settings page after you save them.

Two boards on the same Wi-Fi both answer to `cydesk.local`. The QR code uses the IP address, which still works.

## Pages

Tap anywhere to go to the next one.

1. **Home.** Greeting, date, clock, sunrise and sunset, temperature, the chance of rain, humidity, and a desk pet (fox, raccoon, or owl). The pet can sleep at night, hold an umbrella when rain is likely, or wear sunglasses when it is clear.
2. **Hourly.** The next 12 hours. Weather data is from WeatherAPI.com.
3. **Badge.** WDGWars stats when a key is saved. Otherwise it asks for one.
4. **Status.** Wi-Fi, whether the keys are saved, and the result of the weather check.

## Boards

| Size | Board | Screen | Install button |
| --- | --- | --- | --- |
| 2.8" | E32R28T / ESP32-2432S028 | 240×320, ILI9341 | 2.8 inch |
| 3.5" resistive | ESP32-3248S035**R** | 320×480, ST7796 | 3.5 inch |

The 3.5" build has not been tried on that hardware yet. The capacitive 3.5" (the model ending in **C**) and the 4" boards are not supported.

## Build from source

[PlatformIO](https://platformio.org/). The 2.8" board is the default, so this flashes whatever is plugged in as a 2.8":

```
pio run -t upload
```

The 3.5" resistive board is a separate build. Only use this when that board is the one plugged in:

```
pio run -e cyd35 -t upload
```

`pio run` and `pio run -e cyd35` also refresh the files the installer page serves. Pushing `main` publishes them.
