# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

PlatformIO firmware for an **ESP32 (esp32dev)** that controls two relays. It exposes a WiFi WebUI and a GET/POST JSON API (also usable as webhooks) for manual control, plus a per-relay Auto mode that cycles ON/OFF on configurable timers. Adds an AP setup hotspot for WiFi provisioning, OTA firmware updates, and an optional 0.96" SSD1306 OLED. Settings persist in flash.

## Commands

PlatformIO's `pio` is not on `PATH`; invoke it at `~/.platformio/penv/bin/pio`.

```bash
~/.platformio/penv/bin/pio run                          # build
~/.platformio/penv/bin/pio run -t upload                # build + flash over USB
~/.platformio/penv/bin/pio run -t upload --upload-port relay.local  # OTA flash (espota)
~/.platformio/penv/bin/pio device monitor               # serial @ 115200
~/.platformio/penv/bin/pio run -t clean                 # clean build artifacts
```

There is no test suite (`test/` is the PlatformIO default placeholder).

## Config / secrets workflow

`include/config.h` is **gitignored** and holds real secrets (WiFi password). `include/config.example.h` is the committed template. **When adding any new `#define`, add it to BOTH files** or the example drifts. To bootstrap: `cp include/config.example.h include/config.h`.

## Architecture

Headers in `include/`, logic in `src/`. All user-tunable config lives in `config.h` so firmware code stays generic.

- **`include/config.h`** / **`config.example.h`** — WiFi creds + defaults, mDNS hostname, relay pins, active-low/high (`RELAY_ON_STATE`/`OFF_STATE`), default timings, AP hotspot (`AP_SSID`/`AP_PASSWORD`), `OTA_PASSWORD`, `FW_VERSION`, and OLED pins/`OLED_ENABLED`.
- **`include/web_ui.h`** — main dashboard as one `PROGMEM` raw string `WEB_UI_HTML` (vanilla HTML/CSS/JS, no external deps; works offline).
- **`include/setup_ui.h`** — `SETUP_HTML` (AP-mode WiFi portal) and `OTA_HTML` (firmware upload page with progress bar).
- **`include/display.h` + `src/display.cpp`** — OLED module, decoupled via a `DisplayInfo` snapshot struct (no direct access to relay/WiFi globals).
- **`src/main.cpp`** — relays, web server/routing, WiFi+AP, OTA, persistence.

### Relay model (the core abstraction)

A `Relay[2]` array drives everything. Each `Relay` carries persisted settings (pin, `onDuration`/`offDuration` in **ms**, `mode`) plus live runtime state (`isOn`, `lastToggle`). Two modes:

- **`MODE_AUTO`** — `updateRelay()` (every `loop()`) flips the relay when `millis() - lastToggle` exceeds the current window.
- **`MODE_MANUAL`** — holds state; `updateRelay()` is a no-op. Any `/api/control` command forces MANUAL so it isn't overridden by the cycle.

`setRelay()` is the single choke point that writes the pin + resets the AUTO window. Settings persist via `Preferences` (NVS namespace `"relay"`, keys prefixed per relay e.g. `r1_on`, `r1_mode`). WiFi credentials live in the same namespace (`wifi_ssid`, `wifi_pass`) and **override** the `config.h` defaults when present (`wifiSsid()`/`wifiPass()`).

### Network state machine

`setup()` calls `connectWiFi()`; on failure it falls into `startAPMode()` which raises a SoftAP + a `DNSServer` captive portal (everything resolves to the device, `handleNotFound` 302-redirects to `/`). The global `bool apMode` switches behavior throughout: `/` serves `SETUP_HTML` vs `WEB_UI_HTML`, and `loop()` runs `dnsServer.processNextRequest()` (AP) vs `ArduinoOTA.handle()` + non-blocking reconnect (STA). `/api/wifi` saves creds and reboots to apply.

### HTTP + OTA layer

Synchronous `WebServer`. Status JSON is hand-built in `statusJson()`/`relayJson()` (no ArduinoJson). **Durations are seconds over the wire, ms internally.** The `onGetPost()` helper registers each route for both GET and POST so any webhook source works. Browser OTA uses the streaming upload pattern: `server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload)` where the upload hook feeds `Update.write()`. `requireAuth()` gates OTA with HTTP Basic auth (user `admin`) when `OTA_PASSWORD` is set.

### OLED

`renderDisplay()` in main.cpp (guarded by `#if OLED_ENABLED`) snapshots globals into `DisplayInfo` and calls `displayRender()` every 500 ms. The panel is the two-color type: top 16 px is yellow (title + network line), the rest blue (relay rows). If the SSD1306 isn't found, `displayBegin()` returns false and all draws become no-ops.

## Conventions / gotchas

- Relays are `1`/`2` in the API but indexed `0`/`1` internally (`relayIndexArg()` validates).
- Default I2C pins SDA=21/SCL=22; OLED address `0x3C` (some panels `0x3D`).
- IDE clang flags `Arduino.h not found` / undeclared ESP32 macros / `String` unknown — these are not real errors; only a `pio run` build is authoritative.
- Boots all relays OFF for safety; AUTO relays switch on after their `offDuration`.
- Keep the firmware lean: only the OLED needs external libs — everything else ships with the ESP32 core.
