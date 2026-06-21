# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

PlatformIO firmware for an **ESP32 (esp32dev)** that controls two relays **plus a laser emitter**, and reads two digital sensors (a **PIR motion sensor** and a **laser-beam receiver**). It exposes a WiFi WebUI and a GET/POST JSON API (also usable as webhooks) for manual control, plus a per-output Auto mode that cycles ON/OFF on configurable timers. Sensor events stream into a unified "General Info" event log and can fire per-source **Bark** push notifications. Adds an AP setup hotspot for WiFi provisioning, OTA firmware updates, and an optional 0.96" SSD1306 OLED. Settings persist in flash.

The relays, laser and sensors are independently compiled in/out via `*_ENABLED` config flags; all subsystem calls are safe (no-ops) when disabled, so `main.cpp` stays mostly guard-free.

## Commands

PlatformIO's `pio` is not on `PATH`; invoke it at `~/.platformio/penv/bin/pio`.

```bash
~/.platformio/penv/bin/pio run                          # build
~/.platformio/penv/bin/pio run -t upload                # build + flash over USB
~/.platformio/penv/bin/pio run -t upload --upload-port sensor.local  # OTA flash (espota)
~/.platformio/penv/bin/pio device monitor               # serial @ 115200
~/.platformio/penv/bin/pio run -t clean                 # clean build artifacts
```

There is no test suite (`test/` is the PlatformIO default placeholder).

## Config / secrets workflow

`include/config.h` is **gitignored** and holds real secrets (WiFi password). `include/config.example.h` is the committed template. **When adding any new `#define`, add it to BOTH files** or the example drifts. To bootstrap: `cp include/config.example.h include/config.h`.

## Architecture

Headers in `include/`, logic in `src/`. All user-tunable config lives in `config.h` so firmware code stays generic.

- **`include/config.h`** / **`config.example.h`** — WiFi creds + defaults, mDNS hostname, relay/laser/sensor pins, active-low/high levels, default timings, AP hotspot/`OTA_PASSWORD`, `FW_VERSION`, OLED pins, PIR + laser-receiver pins/delays, `EVENT_LOG_MAX`, and Bark server + per-source defaults.
- **`include/web_ui.h`** — main dashboard as one `PROGMEM` raw string `WEB_UI_HTML` (vanilla HTML/CSS/JS, no external deps; works offline). Renders relay cards, the laser card, motion/beam sensor cards, the 4 Bark toggles, and the unified "General Info" log.
- **`include/setup_ui.h`** — `SETUP_HTML` (AP-mode WiFi portal) and `OTA_HTML` (firmware upload page with progress bar).
- **`include/display.h` + `src/display.cpp`** — OLED module, decoupled via a `DisplayInfo` snapshot struct (no direct access to relay/WiFi globals).
- **`include/timeutil.h`** — header-only `syncedEpoch()` / `formatEpoch()` NTP-time helpers shared by the log and Bark.
- **`include/eventlog.h` + `src/eventlog.cpp`** — unified "General Info" ring buffer (`logEvent`/`logJson`/`logClear`). Every subsystem writes here; the WebUI fetches it incrementally by seq.
- **`include/bark.h` + `src/bark.cpp`** — one Bark push client with **four independent persisted toggles** (`BARK_SRC_RELAY1/RELAY2/MOTION/LASER`), a **master on/off kill switch** over all sources, and a **runtime-editable push URL + device key** (persisted, override the `config.h` defaults; the key is never echoed back over the API). Callers pass title+body; the module appends device/time/URL plus a smart "Last trigger" line (per-source time since the previous push, e.g. `3m 20s ago`).
- **`include/sensor.h` + `src/sensor.cpp`** — generic debounced digital-input `Sensor` (poll → debounce → detection-delay cooldown → `logEvent` + `barkSend`). The single home for edge/debounce logic.
- **`include/motion.h` + `src/motion.cpp`** — PIR sensor: a thin named wrapper configuring one `Sensor` (alert = motion HIGH, `LOG_MOTION`, `BARK_SRC_MOTION`).
- **`include/receiver.h` + `src/receiver.cpp`** — laser-beam receiver: a thin named wrapper configuring one `Sensor` (alert = beam broken, `LOG_BEAM`, `BARK_SRC_LASER`). The beam-present signal level is **runtime-invertible** (`receiverSetBeamHigh()`, persisted as `rx_beam`): flipping it swaps both the alert polarity and the input pull (PULLDOWN↔PULLUP) live via `sensorSetInput()`, so an inverted/open-collector module can be corrected from the WebUI without a reflash.
- **`src/main.cpp`** — relays + laser, web server/routing, WiFi+AP, OTA, persistence; snapshots state into the OLED + status JSON.

### Relay model (the core abstraction)

A `Relay[2]` array drives the relays, and the **laser emitter reuses the exact same `Relay` struct + helpers** (`setRelay`/`updateRelay`/`remainingSeconds`/`load`/`saveRelaySettings`). Each `Relay` carries persisted settings (pin, per-instance `onLevel`/`offLevel`, `onDuration`/`offDuration` in **ms**, `mode`) plus live runtime state (`isOn`, `lastToggle`). The laser is just an output with its own active level (active HIGH vs the relays' active LOW). Two modes:

- **`MODE_AUTO`** — `updateRelay()` (every `loop()`) flips the output when `millis() - lastToggle` exceeds the current window.
- **`MODE_MANUAL`** — holds state; `updateRelay()` is a no-op. Any control command forces MANUAL so it isn't overridden by the cycle.

`setRelay()` is the single choke point that writes the pin + resets the AUTO window — it does **not** log/notify (so auto-cycle ticks don't flood). The control handlers log + Bark only deliberate (manual) switches. Settings persist via `Preferences` (NVS namespace `"relay"`, keys prefixed per output e.g. `r1_on`, `laser_mode`); sensor delays under `pir_delay`/`rx_delay`, receiver beam polarity under `rx_beam`; Bark toggles under `bark_r1`/`bark_r2`/`bark_mo`/`bark_la`, plus the master switch (`bark_on`) and editable server config (`bark_url`/`bark_key`). WiFi credentials live in the same namespace (`wifi_ssid`, `wifi_pass`) and **override** the `config.h` defaults when present (`wifiSsid()`/`wifiPass()`).

### Sensor + event-log model

`Sensor` (sensor.h) is a config-only struct: pin, input pull mode, `alertHigh`, poll/debounce timings, a tunable `detectDelayMs` cooldown, plus which `LogSource`/`BarkSource` to fire. `sensorUpdate()` debounces the raw level and, on a committed edge, calls `logEvent()` and (entering alert) `barkSend()`. Motion and receiver are two instances behind named wrappers, so there's one copy of the edge logic. All events — relays, laser, motion, beam, system boot — land in the **single** `eventlog` ring buffer the WebUI shows as "General Info".

### Network state machine

`setup()` calls `connectWiFi()`; on failure it falls into `startAPMode()` which raises a SoftAP + a `DNSServer` captive portal (everything resolves to the device, `handleNotFound` 302-redirects to `/`). The global `bool apMode` switches behavior throughout: `/` serves `SETUP_HTML` vs `WEB_UI_HTML`, and `loop()` runs `dnsServer.processNextRequest()` (AP) vs `ArduinoOTA.handle()` + non-blocking reconnect (STA). `/api/wifi` saves creds and reboots to apply.

### HTTP + OTA layer

Synchronous `WebServer`. Status JSON is hand-built in `statusJson()`/`relayJson()`/`laserJson()` + the modules' `*StatusJson()` (no ArduinoJson). **Durations are seconds over the wire, ms internally.** The `onGetPost()` helper registers each route for both GET and POST so any webhook source works. Browser OTA uses the streaming upload pattern: `server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload)` where the upload hook feeds `Update.write()`. `requireAuth()` gates OTA with HTTP Basic auth (user `admin`) when `OTA_PASSWORD` is set.

Routes (each GET+POST): `/api/status`, `/api/control?relay=1|2&action=`, `/api/settings?relay=`, `/webhook` (alias of control), `/api/laser/control?action=`, `/api/laser/settings`, `/api/log[?since=N]`, `/api/log/clear`, `/api/motion/delay?ms=`, `/api/receiver/delay?ms=`, `/api/receiver/config?beamHigh=0|1`, `/api/bark?source=relay1|relay2|motion|laser&enabled=0|1`, `/api/bark/config?master=0|1&url=&key=` (blank key keeps current), plus the WiFi + `/update` routes.

### OLED

`renderDisplay()` in main.cpp (guarded by `#if OLED_ENABLED`) snapshots globals into `DisplayInfo` and calls `displayRender()` every 500 ms. The panel is the two-color type: top 16 px is yellow (title + network line), the rest blue. The blue area shows the two relay rows on the left and a stacked **"Motion" / "Laser"** detection-pill column on the right (filled/inverted when in alert; `Laser` = the beam receiver state). If the SSD1306 isn't found, `displayBegin()` returns false and all draws become no-ops.

## Conventions / gotchas

- Relays are `1`/`2` in the API but indexed `0`/`1` internally (`relayIndexArg()` validates). The laser is a singleton at `/api/laser/*`.
- Default pins: relays 26/27, laser 25, PIR 4, receiver 33, I2C SDA=21 (the committed example uses 21; the HUZZAH32 board in `config.h` uses 23) / SCL=22; OLED address `0x3C` (some panels `0x3D`).
- Laser + receiver are each 3-pin (VCC/GND/Signal). Laser is active-HIGH; the receiver's beam-present level defaults to `RECEIVER_BEAM_HIGH` but is **flippable live** from the WebUI (persisted `rx_beam`) — alert = beam broken. The receiver's input pull is chosen so a *disconnected* sensor fails to "beam broken". If pointing the laser at the receiver shows BROKEN, the module is inverted: flip the card's "Beam present = HIGH/LOW" toggle (no reflash). The card's "Signal: HIGH/LOW (GPIO 33)" readout shows the live raw pin level for wiring checks.
- "Detection delay" (motion + receiver) is a per-sensor cooldown (ms) between logged alerts; default 0 (most responsive), editable live and persisted.
- Bark has 4 independent toggles. The "laser" toggle fires on **beam break** (receiver), not on emitter on/off. Auto-cycle relay/laser ticks never log or notify — only manual switches do.
- IDE clang flags `Arduino.h not found` / undeclared ESP32 macros / `String` unknown — these are not real errors; only a `pio run` build is authoritative.
- Boots all relays + the laser OFF for safety; AUTO outputs switch on after their `offDuration`.
- Keep the firmware lean: only the OLED needs external libs — everything else ships with the ESP32 core.
