# ESP32 Dual Relay Controller

WiFi-enabled firmware for an ESP32 that controls **two relays** from a built-in
web interface and a JSON HTTP API (usable as webhooks). Each relay can run on an
**Auto** timer (cycle ON/OFF) or be held **Manually**. Settings persist in flash
and can be changed live — no re-flashing needed.

- 🌐 Responsive WebUI (no internet/CDN required — fully self-contained)
- 🔗 GET/POST REST API + webhook endpoints for home automation
- 💾 Per-relay timing + mode saved to flash (survives reboots)
- 📶 mDNS (`http://relay.local/`) + auto-reconnect on WiFi drop
- 🆕 **AP setup hotspot** — provision WiFi from your phone, no re-flashing
- ⬆️ **OTA updates** — flash new firmware over the air (browser upload *or* `espota`)
- 🖥️ Optional **0.96" OLED** status display
- 🚶 Optional **PIR motion sensor** — live status + timestamped history log in the WebUI
- 🛟 Boots OFF for safety

## Hardware

### Relays
| ESP32 | Relay module |
|-------|--------------|
| GPIO 26 | Relay 1 IN |
| GPIO 27 | Relay 2 IN |
| GND | GND |
| 5V / VIN | VCC |

Most relay boards are **active-low** (ON when the pin is LOW) — the default. If
yours is active-high, flip `RELAY_ON_STATE` / `RELAY_OFF_STATE` in `config.h`.

### OLED display (optional — 0.96" SSD1306, 128×64, I2C)
The common blue/yellow two-color panel; 4 pins:

| OLED | ESP32 |
|------|-------|
| GND | GND |
| VCC | 3V3 (5V also fine on most modules) |
| SCL | GPIO 22 |
| SDA | GPIO 21 |

The top 16 px (yellow) shows the title + network line; the blue area shows each
relay's state, mode and auto-cycle countdown. Set `OLED_ENABLED 0` in `config.h`
if no display is attached. If the screen stays dark, try address `0x3D`.

### PIR motion sensor (optional — HS-S38P or any HIGH-on-motion PIR)
A small 3-pin passive-infrared sensor. Its signal pin goes **HIGH** while motion
is present and returns **LOW** after the sensor's own hold time.

| HS-S38P | ESP32 |
|---------|-------|
| VCC | 3V3 |
| GND | GND |
| S (signal) | GPIO 4 (`PIR_PIN`) |

The firmware debounces edges and records each change into an in-RAM history log
(up to `PIR_LOG_MAX`, default **999** events, oldest dropped). The WebUI shows the
live state plus a scrollable, timestamped log. Timestamps come from **NTP** — the
timezone is set with `TZ_INFO` (a POSIX TZ string that applies daylight-saving
automatically), defaulting to **Australia/Sydney** (`AEST-10AEDT,M10.1.0,M4.1.0/3`).
Before the clock syncs, events fall back to showing the device uptime. Set
`PIR_ENABLED 0` in `config.h` if no sensor is attached.

## Setup

This is a [PlatformIO](https://platformio.org/) project (board: `esp32dev`).

1. **Create your config** from the template (keeps your WiFi password out of git):
   ```bash
   cp include/config.example.h include/config.h
   ```
   Then edit `include/config.h`:
   ```c
   #define WIFI_SSID      "YOUR_WIFI_SSID"     // or leave blank & use AP setup
   #define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
   #define DEVICE_HOSTNAME "relay"             // → http://relay.local/
   ```
   `include/config.h` is gitignored; `include/config.example.h` is the committed template.

2. **Build & flash** (USB, first time):
   ```bash
   pio run -t upload
   pio device monitor          # 115200 baud — prints the device IP
   ```
   > If `pio` isn't on your PATH, use `~/.platformio/penv/bin/pio`.

3. **Open the UI** at `http://relay.local/` or the IP shown in the serial log / OLED.

## First-time WiFi setup (AP mode)

If the device can't join WiFi (wrong credentials, out of range, or none set), it
starts its own **setup hotspot**:

1. On your phone/laptop, join the WiFi network **`Relay-Setup`** (open by default).
2. A captive portal opens automatically (or browse to `http://192.168.4.1/`).
3. Tap **Scan**, pick your network, enter the password, **Save & Connect**.
4. The device reboots and joins your network. Reconnect your phone to normal WiFi
   and open `http://relay.local/`.

Saved credentials live in flash and override the `config.h` defaults. To forget
them: `curl -X POST http://relay.local/api/wifi/reset` (device reboots back into
AP mode). You can also change WiFi anytime from the **WiFi** card in the WebUI.

## Building firmware for OTA updates

When you make code changes and want to push them over the air, you need the
compiled `.bin` file. You do **not** need USB after the initial flash.

### Prerequisites

- [PlatformIO](https://platformio.org/install) installed (VS Code extension or CLI).
- A clone of this repo on your development machine:
  ```bash
  git clone https://github.com/Troublesis/esp32-relay-control.git
  cd esp32-relay-control
  cp include/config.example.h include/config.h
  # edit include/config.h with your settings (optional — the device already has them saved)
  ```

### Build the firmware binary

```bash
pio run
```

This compiles the firmware and produces:

```
.pio/build/esp32dev/firmware.bin
```

That file is what you upload to the device.

### Upload via browser

1. Open **`http://relay.local/update`** (or the device IP).
2. Click **Choose file**, select `.pio/build/esp32dev/firmware.bin`.
3. Click **Flash firmware** — you'll see a progress bar.
4. The device reboots automatically when done.

### Upload via curl

```bash
curl -F "firmware=@.pio/build/esp32dev/firmware.bin" http://relay.local/update
```

### Upload via PlatformIO (espota)

If `OTA_PASSWORD` is set, pass it via `--upload-password`:

```bash
# No password
pio run -t upload --upload-port relay.local

# With password
pio run -t upload --upload-port relay.local --upload-password YOUR_PASSWORD
```

### Version bump (optional)

The firmware reports its version in `/api/status` and the OLED/WebUI footer.
Update it in `include/config.h` before building:

```c
#define FW_VERSION "1.2.0"
```

### Security note

Set `OTA_PASSWORD` in `config.h` to require authentication (HTTP Basic auth,
user `admin`, for the browser/curl upload; passphrase for espota). Leave it
empty for no auth (LAN-only use).

## Web interface

Each relay card has:

- **Turn ON / Turn OFF** buttons (these switch the relay to Manual mode).
- **Auto / Manual** mode toggle. *Auto* cycles using the durations below; *Manual* holds the last state.
- Editable **ON duration** and **OFF duration** (seconds) with a **Save** button.
- Live state badge and an Auto-mode countdown. Refreshes every 2 s.

Plus a **WiFi** card (change network) and a **Firmware** card (link to the updater).

When a PIR sensor is enabled, a **Motion Sensor** section appears below: a live
`MOTION` / `CLEAR` badge, the last trigger time, a total detection count, and a
scrollable **history log** (newest at the bottom, capped at 999 lines) with a
**Clear log** button. The log loads incrementally — the page only fetches events
it hasn't seen yet, so it stays light even with WiFi auto-refresh.

## API / Webhooks

Every endpoint accepts **both `GET` and `POST`**, so any webhook service (or a
plain browser URL) works. Durations are in **seconds**. Responses are JSON.

| Endpoint | Params | Description |
|----------|--------|-------------|
| `/api/status` | — | Full state: WiFi, device info, both relays |
| `/api/control` | `relay=1\|2`, `action=on\|off\|toggle` | Drive a relay (switches it to Manual) |
| `/api/settings` | `relay=1\|2`, `onDuration`, `offDuration`, `mode=auto\|manual` (any subset) | Update timing / mode, persisted to flash |
| `/webhook` | same as `/api/control` | Alias for convenience |
| `/api/wifi/scan` | — (GET) | List nearby networks |
| `/api/wifi` | `ssid`, `pass` | Save WiFi credentials, then reboot |
| `/api/wifi/reset` | — | Forget saved credentials, then reboot |
| `/api/motion/log` | `since=N` (optional) | Motion events with `seq > N` (oldest first) |
| `/api/motion/clear` | — | Wipe the in-RAM motion history log |
| `/update` | (GET) page · (POST) multipart `firmware` | OTA firmware upload |

### Examples

```bash
# Turn relay 1 on
curl "http://relay.local/webhook?relay=1&action=on"

# Toggle relay 2
curl "http://relay.local/api/control?relay=2&action=toggle"

# Put relay 2 on a 3s-on / 8s-off auto cycle
curl "http://relay.local/api/settings?relay=2&onDuration=3&offDuration=8&mode=auto"

# Read state
curl "http://relay.local/api/status"

# Read the full motion history (since=0 returns everything buffered)
curl "http://relay.local/api/motion/log?since=0"

# Clear the motion log
curl -X POST "http://relay.local/api/motion/clear"
```

### `/api/status` response

```json
{
  "wifi": { "connected": true, "apMode": false, "ssid": "MyWiFi", "ip": "192.168.0.42", "rssi": -55 },
  "device": { "version": "1.2.0", "uptime": 1280, "heap": 210000 },
  "relays": [
    { "id": 1, "state": "off", "mode": "auto",   "onDuration": 10, "offDuration": 15, "remaining": 12 },
    { "id": 2, "state": "on",  "mode": "manual", "onDuration": 2,  "offDuration": 5,  "remaining": 0 }
  ],
  "motion": { "enabled": true, "active": false, "count": 7, "lastSeq": 14, "lastTs": "2026-06-20 14:31:28", "lastUp": 845231 }
}
```

When `PIR_ENABLED` is 0 the motion object is just `{ "enabled": false }`. A
single `/api/motion/log` event looks like
`{ "seq": 14, "ts": "2026-06-20 14:31:28", "up": 845231, "m": 0 }` where `m` is
`1` for *motion detected* and `0` for *no motion* (`ts` is empty before NTP syncs).

Invalid requests return HTTP 400 with `{"error": "..."}`.

## Project layout

```
include/config.example.h  ← committed template (copy to config.h)
include/config.h          ← your local settings (gitignored): WiFi, pins, OLED, OTA
include/web_ui.h          ← main WebUI (HTML/CSS/JS)
include/setup_ui.h        ← AP setup portal + OTA upload pages
include/display.h         ← OLED data struct + API
src/display.cpp           ← SSD1306 rendering
include/motion.h          ← PIR motion sensor module API
src/motion.cpp            ← PIR polling, debounce, history log + JSON
src/main.cpp              ← WiFi/AP, web server, OTA, relay state machine, persistence
platformio.ini            ← board / build config / lib_deps
```

## Notes

- The control API has **no authentication** — intended for a trusted LAN. Only
  OTA can be password-gated (`OTA_PASSWORD`).
- WiFi, WebServer, ESPmDNS, Preferences, DNSServer, ArduinoOTA and Update ship
  with the ESP32 Arduino core. Only the OLED pulls external libraries
  (Adafruit SSD1306 + GFX), declared in `platformio.ini`.
