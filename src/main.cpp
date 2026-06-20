#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <time.h>

#include "config.h"
#include "web_ui.h"
#include "setup_ui.h"
#include "display.h"
#include "motion.h"

// ============================================================================
// WIRING GUIDE
// ============================================================================
//
// Relay 1:
//   ESP32 GPIO 26  -->  Relay 1 IN (signal pin)
//   ESP32 GND      -->  Relay module GND
//   ESP32 VIN/5V   -->  Relay module VCC
//
// Relay 2:
//   ESP32 GPIO 27  -->  Relay 2 IN (signal pin)
//   (shared GND and VCC from above)
//
// OLED 0.96" SSD1306 (128x64, I2C, the blue/yellow two-color panel):
//   OLED GND  -->  ESP32 GND
//   OLED VCC  -->  ESP32 3V3   (5V also works on most modules)
//   OLED SCL  -->  ESP32 GPIO 22
//   OLED SDA  -->  ESP32 GPIO 21
//   The top 16 px (yellow) shows title + network; the blue area shows relays.
//   See include/display.h / src/display.cpp. Set OLED_ENABLED 0 to skip it.
//
// PIR motion sensor HS-S38P (or any HIGH-on-motion PIR):
//   PIR VCC  -->  ESP32 3V3
//   PIR GND  -->  ESP32 GND
//   PIR S    -->  ESP32 GPIO 4   (PIR_PIN)
//   Signal goes HIGH on motion, LOW after the sensor's hold time. The WebUI
//   shows live state + a timestamped history log. Set PIR_ENABLED 0 to skip it.
//
// Pins / active state / WiFi credentials are all configured in include/config.h
//
// NOTE: Most relay modules are ACTIVE LOW — relay turns ON when pin is LOW.
//       If yours is ACTIVE HIGH, flip RELAY_ON_STATE / RELAY_OFF_STATE there.
// ============================================================================

// ----------------------------------------------------------------------------
// Relay model
// ----------------------------------------------------------------------------
// Each relay can run in one of two modes:
//   AUTO   — cycles ON for onDuration, then OFF for offDuration, forever.
//   MANUAL — holds whatever state was last commanded (via UI / API / webhook).
//
// Settings (durations + mode) persist in flash so they survive reboots and can
// be changed live from the WebUI or REST API.

enum RelayMode { MODE_MANUAL = 0, MODE_AUTO = 1 };

struct Relay {
  const char* nvsKey;        // namespace key prefix for persistence
  int pin;
  unsigned long onDuration;  // ms — time held ON in AUTO mode
  unsigned long offDuration; // ms — time held OFF in AUTO mode
  RelayMode mode;

  bool isOn;                 // current logical state
  unsigned long lastToggle;  // millis() of last state change (AUTO timing)
};

Relay relays[2] = {
  { "r1", RELAY1_PIN, RELAY1_DEFAULT_ON_MS, RELAY1_DEFAULT_OFF_MS, MODE_MANUAL, false, 0 },
  { "r2", RELAY2_PIN, RELAY2_DEFAULT_ON_MS, RELAY2_DEFAULT_OFF_MS, MODE_MANUAL, false, 0 },
};

WebServer server(WEB_SERVER_PORT);
Preferences prefs;
DNSServer dnsServer;

bool apMode = false;        // true when running the setup hotspot
const byte DNS_PORT = 53;

// ----------------------------------------------------------------------------
// Hardware + persistence helpers
// ----------------------------------------------------------------------------

void writeRelayPin(Relay& r) {
  digitalWrite(r.pin, r.isOn ? RELAY_ON_STATE : RELAY_OFF_STATE);
}

// Apply a logical state immediately and reset the AUTO timing window.
void setRelay(Relay& r, bool on) {
  r.isOn = on;
  r.lastToggle = millis();
  writeRelayPin(r);
  Serial.printf("[relay %s] -> %s (%s)\n", r.nvsKey, on ? "ON" : "OFF",
                r.mode == MODE_AUTO ? "auto" : "manual");
}

void saveRelaySettings(const Relay& r) {
  prefs.putULong((String(r.nvsKey) + "_on").c_str(), r.onDuration);
  prefs.putULong((String(r.nvsKey) + "_off").c_str(), r.offDuration);
  prefs.putUChar((String(r.nvsKey) + "_mode").c_str(), (uint8_t)r.mode);
}

void loadRelaySettings(Relay& r) {
  r.onDuration  = prefs.getULong((String(r.nvsKey) + "_on").c_str(), r.onDuration);
  r.offDuration = prefs.getULong((String(r.nvsKey) + "_off").c_str(), r.offDuration);
  r.mode = (RelayMode)prefs.getUChar((String(r.nvsKey) + "_mode").c_str(),
                                     RELAY_DEFAULT_AUTO ? MODE_AUTO : MODE_MANUAL);
}

// AUTO-mode cycling — call every loop iteration.
void updateRelay(Relay& r) {
  if (r.mode != MODE_AUTO) return;
  unsigned long elapsed = millis() - r.lastToggle;
  if (r.isOn && elapsed >= r.onDuration) {
    setRelay(r, false);
  } else if (!r.isOn && elapsed >= r.offDuration) {
    setRelay(r, true);
  }
}

// Seconds until the next automatic switch (0 in manual mode).
unsigned long remainingSeconds(const Relay& r) {
  if (r.mode != MODE_AUTO) return 0;
  unsigned long window = r.isOn ? r.onDuration : r.offDuration;
  unsigned long elapsed = millis() - r.lastToggle;
  return elapsed >= window ? 0 : (window - elapsed + 999) / 1000;
}

// WiFi credentials: saved values in flash take priority over config.h defaults.
String wifiSsid() { return prefs.getString("wifi_ssid", WIFI_SSID); }
String wifiPass() { return prefs.getString("wifi_pass", WIFI_PASSWORD); }

// ----------------------------------------------------------------------------
// JSON status (built by hand — payload is tiny, avoids an extra dependency)
// ----------------------------------------------------------------------------

String relayJson(const Relay& r, int id) {
  String s = "{";
  s += "\"id\":" + String(id);
  s += ",\"state\":\"" + String(r.isOn ? "on" : "off") + "\"";
  s += ",\"mode\":\"" + String(r.mode == MODE_AUTO ? "auto" : "manual") + "\"";
  s += ",\"onDuration\":" + String(r.onDuration / 1000);
  s += ",\"offDuration\":" + String(r.offDuration / 1000);
  s += ",\"remaining\":" + String(remainingSeconds(r));
  s += "}";
  return s;
}

String statusJson() {
  bool up = WiFi.status() == WL_CONNECTED;
  String s = "{\"wifi\":{";
  s += "\"connected\":" + String(up ? "true" : "false");
  s += ",\"apMode\":" + String(apMode ? "true" : "false");
  s += ",\"ssid\":\"" + (apMode ? String(AP_SSID) : wifiSsid()) + "\"";
  s += ",\"ip\":\"" + (apMode ? WiFi.softAPIP().toString()
                              : (up ? WiFi.localIP().toString() : String("0.0.0.0"))) + "\"";
  s += ",\"rssi\":" + String(up ? WiFi.RSSI() : 0);
  s += "},\"device\":{";
  s += "\"version\":\"" FW_VERSION "\"";
  s += ",\"uptime\":" + String(millis() / 1000);
  s += ",\"heap\":" + String(ESP.getFreeHeap());
  s += "},\"relays\":[";
  s += relayJson(relays[0], 1) + "," + relayJson(relays[1], 2);
  s += "],\"motion\":" + motionStatusJson();
  s += "}";
  return s;
}

void sendStatus() { server.send(200, "application/json", statusJson()); }
void sendError(int code, const String& msg) {
  server.send(code, "application/json", "{\"error\":\"" + msg + "\"}");
}

// Resolve the ?relay=1|2 argument to an index (0 or 1), or -1 if invalid.
int relayIndexArg() {
  if (!server.hasArg("relay")) return -1;
  int id = server.arg("relay").toInt();
  return (id == 1 || id == 2) ? id - 1 : -1;
}

// Gate sensitive routes (OTA) behind HTTP Basic auth when OTA_PASSWORD is set.
bool requireAuth() {
  if (strlen(OTA_PASSWORD) == 0) return true;
  if (server.authenticate("admin", OTA_PASSWORD)) return true;
  server.requestAuthentication();
  return false;
}

// ----------------------------------------------------------------------------
// HTTP route handlers — relay control
// ----------------------------------------------------------------------------

void handleRoot() {
  server.send_P(200, "text/html", apMode ? SETUP_HTML : WEB_UI_HTML);
}

// POST/GET /api/control?relay=1&action=on|off|toggle
// Manually drive a relay. This also switches it to MANUAL mode so the command
// is not immediately overridden by the auto-cycle.
void handleControl() {
  int idx = relayIndexArg();
  if (idx < 0) return sendError(400, "missing or invalid 'relay' (1 or 2)");
  if (!server.hasArg("action")) return sendError(400, "missing 'action'");

  String action = server.arg("action");
  Relay& r = relays[idx];

  if (action == "on" || action == "off" || action == "toggle") {
    r.mode = MODE_MANUAL;
    saveRelaySettings(r);
    bool target = (action == "toggle") ? !r.isOn : (action == "on");
    setRelay(r, target);
    return sendStatus();
  }
  sendError(400, "action must be on, off or toggle");
}

// POST/GET /api/settings?relay=1[&onDuration=s][&offDuration=s][&mode=auto|manual]
// Update timing and/or mode. Durations are in SECONDS. Persists to flash.
void handleSettings() {
  int idx = relayIndexArg();
  if (idx < 0) return sendError(400, "missing or invalid 'relay' (1 or 2)");
  Relay& r = relays[idx];

  if (server.hasArg("onDuration")) {
    long s = server.arg("onDuration").toInt();
    if (s < 1) return sendError(400, "onDuration must be >= 1 second");
    r.onDuration = (unsigned long)s * 1000UL;
  }
  if (server.hasArg("offDuration")) {
    long s = server.arg("offDuration").toInt();
    if (s < 1) return sendError(400, "offDuration must be >= 1 second");
    r.offDuration = (unsigned long)s * 1000UL;
  }
  if (server.hasArg("mode")) {
    String m = server.arg("mode");
    if (m == "auto") r.mode = MODE_AUTO;
    else if (m == "manual") r.mode = MODE_MANUAL;
    else return sendError(400, "mode must be auto or manual");
    r.lastToggle = millis(); // start a fresh window on mode change
  }

  saveRelaySettings(r);
  sendStatus();
}

// ----------------------------------------------------------------------------
// HTTP route handlers — WiFi provisioning
// ----------------------------------------------------------------------------

// GET /api/wifi/scan -> [{"ssid":"..","rssi":-50,"secure":true}, ...]
void handleWifiScan() {
  int n = WiFi.scanNetworks();
  String s = "[";
  for (int i = 0; i < n; i++) {
    if (i) s += ",";
    s += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
         ",\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  s += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", s);
}

// POST/GET /api/wifi?ssid=..&pass=..  -> save credentials and reboot to apply.
void handleWifiSave() {
  if (!server.hasArg("ssid") || server.arg("ssid").length() == 0)
    return sendError(400, "missing 'ssid'");
  prefs.putString("wifi_ssid", server.arg("ssid"));
  prefs.putString("wifi_pass", server.hasArg("pass") ? server.arg("pass") : String(""));
  server.send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
  Serial.println("[wifi] credentials saved, rebooting...");
  delay(600);
  ESP.restart();
}

// POST/GET /api/wifi/reset -> forget saved credentials and reboot.
void handleWifiReset() {
  prefs.remove("wifi_ssid");
  prefs.remove("wifi_pass");
  server.send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
  Serial.println("[wifi] credentials cleared, rebooting...");
  delay(600);
  ESP.restart();
}

// ----------------------------------------------------------------------------
// HTTP route handlers — PIR motion sensor
// ----------------------------------------------------------------------------

// GET/POST /api/motion/log[?since=N] -> events with seq > N (oldest first).
// The WebUI passes the highest seq it has seen so only new events come back.
void handleMotionLog() {
  unsigned long since = server.hasArg("since")
                          ? (unsigned long)server.arg("since").toInt() : 0UL;
  server.send(200, "application/json", motionLogJson(since));
}

// POST/GET /api/motion/clear -> wipe the in-RAM history log.
void handleMotionClear() {
  motionClearLog();
  server.send(200, "application/json", "{\"ok\":true}");
}

// ----------------------------------------------------------------------------
// HTTP route handlers — OTA (browser firmware upload)
// ----------------------------------------------------------------------------

void handleUpdatePage() {
  if (!requireAuth()) return;
  server.send_P(200, "text/html", OTA_HTML);
}

// Final response after the multipart body has been consumed by the upload hook.
void handleUpdateDone() {
  if (!requireAuth()) return;
  if (Update.hasError()) {
    server.send(500, "text/plain", "Update failed");
  } else {
    server.send(200, "text/plain", "OK");
    Serial.println("[ota] update applied, rebooting...");
    delay(600);
    ESP.restart();
  }
}

// Streaming upload hook — writes the incoming firmware to the OTA partition.
void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  switch (upload.status) {
    case UPLOAD_FILE_START:
      Serial.printf("[ota] receiving %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      break;
    case UPLOAD_FILE_WRITE:
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        Update.printError(Serial);
      break;
    case UPLOAD_FILE_END:
      if (Update.end(true)) Serial.printf("[ota] success: %u bytes\n", upload.totalSize);
      else Update.printError(Serial);
      break;
    default:
      break;
  }
}

// In AP mode, send unknown hosts to the setup page (captive-portal behavior).
void handleNotFound() {
  if (apMode) {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  sendError(404, "not found");
}

// Register a route for both GET and POST so any webhook source works.
void onGetPost(const char* path, void (*fn)()) {
  server.on(path, HTTP_GET, fn);
  server.on(path, HTTP_POST, fn);
}

void setupRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  onGetPost("/api/status", sendStatus);
  onGetPost("/api/control", handleControl);
  onGetPost("/api/settings", handleSettings);
  onGetPost("/webhook", handleControl); // alias: /webhook?relay=1&action=on

  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  onGetPost("/api/wifi", handleWifiSave);
  onGetPost("/api/wifi/reset", handleWifiReset);

  onGetPost("/api/motion/log", handleMotionLog);
  onGetPost("/api/motion/clear", handleMotionClear);

  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);

  server.onNotFound(handleNotFound);
}

// ----------------------------------------------------------------------------
// WiFi + OTA bring-up
// ----------------------------------------------------------------------------

void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  bool secured = strlen(AP_PASSWORD) >= 8;
  if (secured) WiFi.softAP(AP_SSID, AP_PASSWORD);
  else         WiFi.softAP(AP_SSID);
  IPAddress ip = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", ip); // captive portal: resolve everything to us
  Serial.printf("[ap] setup hotspot \"%s\" (%s)\n", AP_SSID, secured ? "secured" : "open");
  Serial.printf("[ap] join it, then browse to http://%s/\n", ip.toString().c_str());
}

void setupArduinoOTA() {
  ArduinoOTA.setHostname(DEVICE_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { Serial.println("[ota] ArduinoOTA start"); });
  ArduinoOTA.onEnd([]()   { Serial.println("\n[ota] ArduinoOTA done"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota] error %u\n", e); });
  ArduinoOTA.begin();
}

// Try to join the configured network. Returns true on success.
bool connectWiFi() {
  String ssid = wifiSsid();
  String pass = wifiPass();
  Serial.printf("[wifi] connecting to \"%s\" ", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long deadline = millis() + (unsigned long)WIFI_CONNECT_TIMEOUT_S * 1000UL;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(400);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" FAILED");
    return false;
  }

  Serial.printf("\n[wifi] connected. IP: %s\n", WiFi.localIP().toString().c_str());

  // Kick off NTP so the motion log can show real timestamps (non-blocking;
  // the clock becomes valid a few seconds later in the background). TZ_INFO is a
  // POSIX TZ string so DST is applied automatically per the local rules.
  configTzTime(TZ_INFO, NTP_SERVER);
  Serial.printf("[time] NTP sync requested (%s, TZ %s)\n", NTP_SERVER, TZ_INFO);

  if (MDNS.begin(DEVICE_HOSTNAME)) {
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
    Serial.printf("[wifi] UI at http://%s.local/  or  http://%s/\n",
                  DEVICE_HOSTNAME, WiFi.localIP().toString().c_str());
  }
  setupArduinoOTA();
  return true;
}

// ----------------------------------------------------------------------------
// OLED dashboard
// ----------------------------------------------------------------------------

#if OLED_ENABLED
// Snapshot current state into the display module's decoupled struct and draw.
void renderDisplay() {
  DisplayInfo d;
  d.apMode = apMode;
  d.wifiConnected = WiFi.status() == WL_CONNECTED;
  d.ssid = apMode ? String(AP_SSID) : wifiSsid();
  d.ip = apMode ? WiFi.softAPIP().toString()
                : (d.wifiConnected ? WiFi.localIP().toString() : String("--"));
  d.rssi = d.wifiConnected ? WiFi.RSSI() : 0;
  d.version = FW_VERSION;
  for (int i = 0; i < 2; i++) {
    d.relayOn[i] = relays[i].isOn;
    d.relayAuto[i] = relays[i].mode == MODE_AUTO;
    d.remaining[i] = remainingSeconds(relays[i]);
  }
  d.motionEnabled = motionEnabled();
  d.motionActive = motionActive();
  displayRender(d);
}
#endif

// ----------------------------------------------------------------------------
// Arduino entry points
// ----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 Dual Relay Controller v" FW_VERSION " ===");

  prefs.begin("relay", false);

#if OLED_ENABLED
  displayBegin();
  displaySplash("Relay", "starting...");
#endif

  for (int i = 0; i < 2; i++) {
    Relay& r = relays[i];
    pinMode(r.pin, OUTPUT);
    loadRelaySettings(r);
    // Boot OFF for safety; AUTO mode will switch it on after offDuration.
    setRelay(r, false);
    Serial.printf("[relay %d] pin %d | mode %s | ON %lus / OFF %lus\n",
                  i + 1, r.pin, r.mode == MODE_AUTO ? "auto" : "manual",
                  r.onDuration / 1000, r.offDuration / 1000);
  }

  motionBegin(); // PIR input (no-op when PIR_ENABLED is 0)

#if OLED_ENABLED
  displaySplash("Relay", "connecting WiFi");
#endif

  if (!connectWiFi()) startAPMode(); // fall back to setup hotspot

  setupRoutes();
  server.begin();
  Serial.println("[http] server started");
  Serial.println("===================================");

#if OLED_ENABLED
  renderDisplay();
#endif
}

unsigned long lastWifiCheck = 0;
unsigned long lastDisplay = 0;

void loop() {
  server.handleClient();
  if (apMode) dnsServer.processNextRequest();
  else        ArduinoOTA.handle();

  updateRelay(relays[0]);
  updateRelay(relays[1]);

  motionUpdate(); // poll PIR + record edges (no-op when PIR_ENABLED is 0)

#if OLED_ENABLED
  if (millis() - lastDisplay > 500) {  // refresh OLED ~2x/sec
    lastDisplay = millis();
    renderDisplay();
  }
#endif

  // While in station mode, if WiFi dropped, retry every 10 s without blocking.
  if (!apMode && WiFi.status() != WL_CONNECTED && millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    WiFi.reconnect();
  }
}
