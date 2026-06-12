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

## OTA firmware updates

Once on the network, you can update without USB. Two ways:

- **Browser:** open `http://relay.local/update`, choose the compiled
  `firmware.bin` (from `.pio/build/esp32dev/`), and watch the progress bar. The
  device reboots automatically.
- **PlatformIO / espota:**
  ```bash
  pio run -t upload --upload-port relay.local
  ```

Set `OTA_PASSWORD` in `config.h` to require a password (HTTP Basic auth, user
`admin`, for the web page; passphrase for `espota`). Empty = no auth (LAN only).

## Web interface

Each relay card has:

- **Turn ON / Turn OFF** buttons (these switch the relay to Manual mode).
- **Auto / Manual** mode toggle. *Auto* cycles using the durations below; *Manual* holds the last state.
- Editable **ON duration** and **OFF duration** (seconds) with a **Save** button.
- Live state badge and an Auto-mode countdown. Refreshes every 2 s.

Plus a **WiFi** card (change network) and a **Firmware** card (link to the updater).

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
```

### `/api/status` response

```json
{
  "wifi": { "connected": true, "apMode": false, "ssid": "MyWiFi", "ip": "192.168.0.42", "rssi": -55 },
  "device": { "version": "1.1.0", "uptime": 1280, "heap": 210000 },
  "relays": [
    { "id": 1, "state": "off", "mode": "auto",   "onDuration": 10, "offDuration": 15, "remaining": 12 },
    { "id": 2, "state": "on",  "mode": "manual", "onDuration": 2,  "offDuration": 5,  "remaining": 0 }
  ]
}
```

Invalid requests return HTTP 400 with `{"error": "..."}`.

## Project layout

```
include/config.example.h  ← committed template (copy to config.h)
include/config.h          ← your local settings (gitignored): WiFi, pins, OLED, OTA
include/web_ui.h          ← main WebUI (HTML/CSS/JS)
include/setup_ui.h        ← AP setup portal + OTA upload pages
include/display.h         ← OLED data struct + API
src/display.cpp           ← SSD1306 rendering
src/main.cpp              ← WiFi/AP, web server, OTA, relay state machine, persistence
platformio.ini            ← board / build config / lib_deps
```

## Notes

- The control API has **no authentication** — intended for a trusted LAN. Only
  OTA can be password-gated (`OTA_PASSWORD`).
- WiFi, WebServer, ESPmDNS, Preferences, DNSServer, ArduinoOTA and Update ship
  with the ESP32 Arduino core. Only the OLED pulls external libraries
  (Adafruit SSD1306 + GFX), declared in `platformio.ini`.
