#include "bark.h"
#include "config.h"
#include "identity.h"
#include "timeutil.h"

#include <string.h>

// ============================================================================
// Bark notifier implementation — see include/bark.h for the contract.
// ============================================================================

#if BARK_ENABLED
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Live per-source toggles, seeded from the build defaults; barkBegin() then
// overlays whatever the user last persisted.
static bool enabledFlags[BARK_SRC_COUNT] = {
  BARK_RELAY1_DEFAULT, BARK_RELAY2_DEFAULT, BARK_MOTION_DEFAULT, BARK_BEAM_DEFAULT
};

// Master kill switch + server config, seeded from the build values and then
// overlaid from flash by barkBegin(). All editable live from the WebUI.
static bool   masterOn  = true;
static String pushUrl   = BARK_PUSH_URL;
static String deviceKey = BARK_DEVICE_KEY;

// millis() of the last notification actually sent per source, 0 = none yet this
// boot. Lets barkSend() report a "time since last trigger" smart-duration line.
static unsigned long lastTriggerMs[BARK_SRC_COUNT] = {0};

// Short, stable NVS keys (the Preferences namespace caps key length at 15).
static const char* nvsKeyFor(int s) {
  switch (s) {
    case BARK_SRC_RELAY1: return "bark_r1";
    case BARK_SRC_RELAY2: return "bark_r2";
    case BARK_SRC_MOTION: return "bark_mo";
    case BARK_SRC_LASER:  return "bark_la";
  }
  return "bark_x";
}

static String jsonEscape(const char* value) {
  String out;
  out.reserve(strlen(value) + 8);
  for (const char* p = value; *p; p++) {
    switch (*p) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:   out += *p; break;
    }
  }
  return out;
}

static void appendJsonString(String& json, const char* key, const char* value) {
  if (value[0] == '\0') return;
  json += ",\"";
  json += key;
  json += "\":\"";
  json += jsonEscape(value);
  json += "\"";
}

// The address a phone can actually open when tapping the notification: prefer
// the live LAN IP (always resolvable) over the mDNS ".local" name, which many
// mobile clients can't look up. Falls back to BARK_OPEN_URL only if WiFi is down
// (we don't normally send a push in that case).
static String deviceWebUrl() {
  if (WiFi.status() == WL_CONNECTED)
    return "http://" + WiFi.localIP().toString() + "/";
  return String(BARK_OPEN_URL);
}

static String buildBody(const String& body) {
  String out = body;
  String ts = formatEpoch(syncedEpoch());
  if (ts.length() > 0) { out += "\nTime: "; out += ts; }
  out += "\nDevice: "; out += deviceHostname();
  out += "\nOpen: ";   out += deviceWebUrl();
  return out;
}

static String buildPayload(const String& title, const String& body) {
  String json = "{\"device_key\":\"";
  json += jsonEscape(deviceKey.c_str());
  json += "\"";
  appendJsonString(json, "title", title.c_str());
  String b = buildBody(body);
  appendJsonString(json, "body", b.c_str());
  json += ",\"badge\":" + String(BARK_BADGE);
  appendJsonString(json, "sound", BARK_SOUND);
  appendJsonString(json, "icon", BARK_ICON);
  appendJsonString(json, "group", BARK_GROUP);
  String url = deviceWebUrl();
  appendJsonString(json, "url", url.c_str());
  json += "}";
  return json;
}

bool barkAvailable() { return true; }

bool barkEnabled(int s) {
  return (s >= 0 && s < BARK_SRC_COUNT) ? enabledFlags[s] : false;
}

void barkSetEnabled(Preferences& prefs, int s, bool on) {
  if (s < 0 || s >= BARK_SRC_COUNT) return;
  enabledFlags[s] = on;
  prefs.putBool(nvsKeyFor(s), on);
  Serial.printf("[bark] %s notifications %s\n", nvsKeyFor(s), on ? "enabled" : "disabled");
}

void barkBegin(Preferences& prefs) {
  for (int s = 0; s < BARK_SRC_COUNT; s++)
    enabledFlags[s] = prefs.getBool(nvsKeyFor(s), enabledFlags[s]);
  masterOn  = prefs.getBool("bark_on", masterOn);
  pushUrl   = prefs.getString("bark_url", pushUrl);
  deviceKey = prefs.getString("bark_key", deviceKey);
}

bool barkMasterEnabled() { return masterOn; }

void barkSetMaster(Preferences& prefs, bool on) {
  masterOn = on;
  prefs.putBool("bark_on", on);
  Serial.printf("[bark] master switch %s\n", on ? "ON" : "OFF");
}

String barkPushUrl()   { return pushUrl; }
String barkDeviceKey() { return deviceKey; }

void barkSetConfig(Preferences& prefs, const String& url, const String& key) {
  pushUrl = url;
  prefs.putString("bark_url", url);
  if (key.length() > 0) {           // empty = keep the current key (never echoed)
    deviceKey = key;
    prefs.putString("bark_key", key);
  }
  Serial.printf("[bark] server config updated (url set, key %s)\n",
                key.length() ? "changed" : "unchanged");
}

void barkSend(int s, const String& title, const String& body) {
  if (!masterOn) return;       // global kill switch overrides every source
  if (!barkEnabled(s)) return; // unavailable, out of range, or this source is off
  if (pushUrl.length() == 0 || deviceKey.length() == 0) {
    Serial.println("[bark] skipped: missing URL or device key");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[bark] skipped: WiFi is not connected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(BARK_TIMEOUT_MS);
  http.setTimeout(BARK_TIMEOUT_MS);
  if (!http.begin(client, pushUrl)) {
    Serial.println("[bark] failed to start HTTP request");
    return;
  }

  // Smart "time since the last push from this source" line, then re-arm the
  // clock for next time. 0 means this is the first trigger since boot.
  String b = body;
  unsigned long now = millis();
  if (lastTriggerMs[s] != 0) {
    b += "\nLast trigger: " + formatDuration(now - lastTriggerMs[s]) + " ago";
  } else {
    b += "\nLast trigger: first since boot";
  }
  lastTriggerMs[s] = now;

  http.addHeader("Content-Type", "application/json; charset=utf-8");
  int code = http.POST(buildPayload(title, b));
  http.end();

  if (code > 0 && code < 400) Serial.printf("[bark] notification sent (HTTP %d)\n", code);
  else                        Serial.printf("[bark] notification failed (HTTP %d)\n", code);
}

String barkStatusJson() {
  String s = "{\"available\":true";
  s += ",\"master\":" + String(masterOn ? "true" : "false");
  s += ",\"url\":\"" + jsonEscape(pushUrl.c_str()) + "\"";
  s += ",\"keySet\":" + String(deviceKey.length() ? "true" : "false");
  s += ",\"relay1\":" + String(enabledFlags[BARK_SRC_RELAY1] ? "true" : "false");
  s += ",\"relay2\":" + String(enabledFlags[BARK_SRC_RELAY2] ? "true" : "false");
  s += ",\"motion\":" + String(enabledFlags[BARK_SRC_MOTION] ? "true" : "false");
  s += ",\"laser\":"  + String(enabledFlags[BARK_SRC_LASER]  ? "true" : "false");
  s += "}";
  return s;
}

#else // BARK_ENABLED == 0 — compiled-out stubs keep callers guard-free.

bool   barkAvailable() { return false; }
bool   barkEnabled(int) { return false; }
void   barkSetEnabled(Preferences&, int, bool) {}
void   barkBegin(Preferences&) {}
void   barkSend(int, const String&, const String&) {}
String barkStatusJson() { return String("{\"available\":false}"); }
bool   barkMasterEnabled() { return false; }
void   barkSetMaster(Preferences&, bool) {}
String barkPushUrl() { return String(BARK_PUSH_URL); }
String barkDeviceKey() { return String(BARK_DEVICE_KEY); }
void   barkSetConfig(Preferences&, const String&, const String&) {}

#endif
