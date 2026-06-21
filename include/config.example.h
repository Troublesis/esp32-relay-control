#pragma once

// ============================================================================
// USER CONFIGURATION — TEMPLATE
// ============================================================================
// Copy this file to "config.h" and fill in your values:
//     cp include/config.example.h include/config.h
// config.h is gitignored so your WiFi password never lands in the repo. You can
// also leave the WiFi values as placeholders and set them later from the
// device's AP setup portal (see README).
// ============================================================================

// ----------------------------------------------------------------------------
// WiFi
// ----------------------------------------------------------------------------
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

// Hostname for mDNS. Once connected, the UI is reachable at http://<HOST>.local
#define DEVICE_HOSTNAME "sensor"

// Append a lowercase MAC-derived suffix to DEVICE_HOSTNAME (e.g. "sensor-3a9c")
// so multiple identical boards stay unique on the network. The same name is
// used for the mDNS/OTA hostname AND the AP setup hotspot SSID. Set 0 to use
// DEVICE_HOSTNAME verbatim.
#define HOSTNAME_AUTO_SUFFIX 1

// Seconds to wait for WiFi before giving up. On failure the device starts its
// own setup hotspot (AP mode) so you can enter credentials from a phone.
#define WIFI_CONNECT_TIMEOUT_S 20

// The SSID/PASSWORD above are only first-boot defaults. Once you save WiFi
// credentials from the WebUI or the AP setup portal, they persist in flash and
// take priority. Erase saved credentials via POST /api/wifi/reset.

// ----------------------------------------------------------------------------
// AP (setup hotspot) — used when WiFi can't connect / on first boot
// ----------------------------------------------------------------------------
// The hotspot SSID is the device hostname (DEVICE_HOSTNAME + optional MAC
// suffix above), so the network name you see while provisioning matches the
// name the device uses on your LAN.
// Open network by default. For a secured hotspot set 8+ characters here.
#define AP_PASSWORD ""

// ----------------------------------------------------------------------------
// OTA (over-the-air firmware update)
// ----------------------------------------------------------------------------
// Protects both the web "/update" page (HTTP Basic auth, user "admin") and
// ArduinoOTA network uploads. Leave empty to disable auth (LAN-only use).
#define OTA_PASSWORD ""

// Reported by /api/status and shown in the UI footer.
#define FW_VERSION "1.3.0"

// ----------------------------------------------------------------------------
// Relay wiring (see WIRING GUIDE in main.cpp)
// ----------------------------------------------------------------------------
#define RELAY1_PIN 26
#define RELAY2_PIN 27

// Relay active state — adjust if your relay module differs.
//   Active LOW (most common): ON = LOW,  OFF = HIGH
//   Active HIGH:              ON = HIGH, OFF = LOW
#define RELAY_ON_STATE  LOW
#define RELAY_OFF_STATE HIGH

// ----------------------------------------------------------------------------
// Default timing (milliseconds) — used the first time the device boots.
// After that, values are loaded from / saved to flash (Preferences) and can be
// changed live from the WebUI or API.
// ----------------------------------------------------------------------------
#define RELAY1_DEFAULT_ON_MS  10000  // Relay 1 ON for 10 s in Auto mode
#define RELAY1_DEFAULT_OFF_MS 15000  // Relay 1 OFF for 15 s in Auto mode

#define RELAY2_DEFAULT_ON_MS  2000   // Relay 2 ON for 2 s in Auto mode
#define RELAY2_DEFAULT_OFF_MS 5000   // Relay 2 OFF for 5 s in Auto mode

// Default mode on first boot: true = Auto (cycle), false = Manual (hold).
#define RELAY_DEFAULT_AUTO true

// HTTP server port
#define WEB_SERVER_PORT 80

// ----------------------------------------------------------------------------
// OLED display — 0.96" SSD1306, 128x64, I2C (the blue/yellow two-color one)
// ----------------------------------------------------------------------------
//   OLED GND -> ESP32 GND
//   OLED VCC -> ESP32 3V3   (most modules also tolerate 5V)
//   OLED SCL -> ESP32 GPIO 22
//   OLED SDA -> ESP32 GPIO 21
// Set OLED_ENABLED to 0 if no display is attached.
// displayBegin() auto-probes the bus: if the panel doesn't answer here it also
// tries the other common SDA (21<->23) and address (0x3C<->0x3D) and logs what
// worked, so a slightly different board still lights up.
#define OLED_ENABLED 1
#define OLED_SDA     21   // esp32dev default I2C SDA (HUZZAH32 Feather uses 23)
#define OLED_SCL     22
#define OLED_ADDR    0x3C   // try 0x3D if your module doesn't light up
#define OLED_WIDTH   128
#define OLED_HEIGHT  64

// ----------------------------------------------------------------------------
// PIR motion sensor — e.g. HS-S38P (3 pins: VCC / GND / S)
// ----------------------------------------------------------------------------
//   PIR VCC -> ESP32 3V3
//   PIR GND -> ESP32 GND
//   PIR S   -> ESP32 GPIO 4  (PIR_PIN — any input-capable GPIO)
// The signal pin goes HIGH while motion is present and returns LOW after the
// sensor's own hold time. The WebUI shows live state plus a unified event log.
// Set PIR_ENABLED to 0 if no sensor is attached.
#define PIR_ENABLED 1
#define PIR_PIN     4
// How often the input is sampled, in ms (PIR output changes slowly).
#define PIR_POLL_MS 50
// Default "detection delay" (ms): minimum gap between two logged detections.
// 0 = most responsive (log every edge). Editable live from the WebUI.
#define PIR_DEFAULT_DELAY_MS 0

// ----------------------------------------------------------------------------
// Laser emitter — controllable laser diode output (3 pins: VCC / GND / Signal)
// ----------------------------------------------------------------------------
//   Laser VCC -> ESP32 3V3 (or 5V per module)
//   Laser GND -> ESP32 GND
//   Laser S   -> ESP32 GPIO 25  (LASER_PIN — any output-capable GPIO)
// Behaves like a relay: AUTO cycles ON/OFF on a timer, MANUAL holds the last
// commanded state. Most laser modules are ACTIVE HIGH (signal HIGH = beam on).
#define LASER_ENABLED   1
#define LASER_PIN       25
#define LASER_ON_STATE  HIGH
#define LASER_OFF_STATE LOW
#define LASER_DEFAULT_ON_MS  1000   // Laser ON for 1 s in Auto mode
#define LASER_DEFAULT_OFF_MS 1000   // Laser OFF for 1 s in Auto mode
// Default mode on first boot: true = Auto (cycle), false = Manual (hold off).
#define LASER_DEFAULT_AUTO false

// ----------------------------------------------------------------------------
// Laser receiver — beam-break sensor (3 pins: VCC / GND / Signal)
// ----------------------------------------------------------------------------
//   Receiver VCC -> ESP32 3V3
//   Receiver GND -> ESP32 GND
//   Receiver S   -> ESP32 GPIO 33  (RECEIVER_PIN — input-capable GPIO)
// Point the laser emitter at the receiver. The signal reflects whether the beam
// is landing on the sensor; crossing the beam ("not received") is logged and
// can fire a Bark push. Set RECEIVER_ENABLED to 0 if no receiver is attached.
#define RECEIVER_ENABLED 1
#define RECEIVER_PIN     33
// Signal level while the beam IS being received. Most modules read HIGH when the
// beam hits the sensor and drop LOW when interrupted — set 0 if yours is wired
// the other way around. The "alert" (logged/notified) state is "not received".
#define RECEIVER_BEAM_HIGH 1
#define RECEIVER_POLL_MS   20
// Default "detection delay" (ms): minimum gap between two logged break events.
#define RECEIVER_DEFAULT_DELAY_MS 0

// ----------------------------------------------------------------------------
// Unified event log ("General Info") — bounded in-RAM ring buffer
// ----------------------------------------------------------------------------
// Holds relay, laser, motion and beam events (newest wins, oldest dropped past
// this cap). Each entry costs ~40 bytes of RAM, so keep it sane.
#define EVENT_LOG_MAX 500

// ----------------------------------------------------------------------------
// Bark push notifications — per-source toggles (relay 1/2, motion, laser beam)
// ----------------------------------------------------------------------------
// Leave disabled until your local config.h contains your Bark server details.
// Each source has an independent on/off toggle in the WebUI, persisted in flash;
// the defaults below seed it on first boot.
#define BARK_ENABLED     0
#define BARK_PUSH_URL    "https://your-bark-server.example/push"
#define BARK_DEVICE_KEY  "YOUR_BARK_DEVICE_KEY"
#define BARK_BADGE       1
#define BARK_SOUND       "door-close"
#define BARK_ICON        ""
#define BARK_GROUP       "esp32"
#define BARK_TIMEOUT_MS  3000
// Fallback only: the push normally opens the device's live LAN IP (http://<ip>/),
// which resolves on phones; this is used just if WiFi is down when sending.
#define BARK_OPEN_URL    "http://sensor.local/"

// Per-source titles + first-boot enable defaults (1 = notify, 0 = silent).
#define BARK_MOTION_TITLE   "Human Motion Detected"
#define BARK_MOTION_BODY    "The ESP32 PIR sensor detected movement."
#define BARK_MOTION_DEFAULT 0

#define BARK_BEAM_TITLE     "Laser Beam Broken"
#define BARK_BEAM_BODY      "The ESP32 laser tripwire was interrupted."
#define BARK_BEAM_DEFAULT   0

#define BARK_RELAY_TITLE    "Relay Switched"   // body is built per relay/state
#define BARK_RELAY1_DEFAULT 0
#define BARK_RELAY2_DEFAULT 0

// ----------------------------------------------------------------------------
// NTP time — gives the event log real timestamps (needs WiFi/internet)
// ----------------------------------------------------------------------------
// Without a sync the log falls back to showing the device uptime. TZ_INFO is a
// POSIX timezone string and handles daylight-saving transitions automatically.
// Default is Australia/Sydney (AEST/AEDT). Examples for other zones:
//   Vietnam (UTC+7, no DST): "ICT-7"
//   UK:                      "GMT0BST,M3.5.0/1,M10.5.0"
//   US Eastern:              "EST5EDT,M3.2.0,M11.1.0"
//   UTC:                     "UTC0"
// Look up yours: https://github.com/nayarsystems/posix_tz_db
#define NTP_SERVER "pool.ntp.org"
#define TZ_INFO    "AEST-10AEDT,M10.1.0,M4.1.0/3"   // Australia/Sydney
