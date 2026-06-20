#include "motion.h"
#include "config.h"

#include <string.h>
#include <time.h>

#if PIR_ENABLED && BARK_ENABLED
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif

// ============================================================================
// PIR motion sensor implementation — see include/motion.h for the contract.
// ============================================================================

#if PIR_ENABLED

// One recorded state change. Kept small: 999 of these live in RAM at once.
struct MotionEvent {
  unsigned long seq;   // monotonically increasing id (1-based)
  time_t        epoch; // wall-clock at the event, 0 if NTP not yet synced
  unsigned long up;    // millis() at the event (always available)
  bool          motion; // true = detected (HIGH), false = cleared (LOW)
};

// Fixed-capacity ring buffer. `head` is the next write slot; `count` saturates
// at PIR_LOG_MAX. Oldest entry is overwritten once full.
static MotionEvent logBuf[PIR_LOG_MAX];
static int           logHead = 0;
static int           logCount = 0;

static unsigned long seqCounter = 0;   // id of the last appended event
static unsigned long motionCount = 0;  // number of "detected" events since boot

static bool          curState = false;     // debounced logical state
static bool          rawLast = false;       // last raw sample (for debounce)
static unsigned long rawSince = 0;          // millis() the raw level last changed
static unsigned long lastPoll = 0;          // millis() of last sample

static const unsigned long DEBOUNCE_MS = 60; // raw level must hold this long

// Epoch only once NTP has actually set the clock. A value past 2020-09-13
// (1600000000) means configTime() has succeeded; before that time() returns a
// small boot-relative number we don't want to record as a real date.
static time_t syncedEpoch() {
  time_t now = time(nullptr);
  return now > 1600000000 ? now : 0;
}

// Format an epoch as "YYYY-MM-DD HH:MM:SS" in the configured local timezone,
// or "" when the timestamp is unknown.
static String formatTs(time_t epoch) {
  if (epoch == 0) return String();
  struct tm tmv;
  localtime_r(&epoch, &tmv);
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
  return String(buf);
}

#if BARK_ENABLED
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

static String barkBody(const MotionEvent& ev) {
  String body = BARK_BODY;
  String ts = formatTs(ev.epoch);
  if (ts.length() > 0) {
    body += "\nTime: ";
    body += ts;
  }
  body += "\nDevice: " DEVICE_HOSTNAME ".local";
  return body;
}

static String barkPayload(const MotionEvent& ev) {
  String json = "{\"device_key\":\"";
  json += jsonEscape(BARK_DEVICE_KEY);
  json += "\"";
  appendJsonString(json, "title", BARK_TITLE);
  String body = barkBody(ev);
  appendJsonString(json, "body", body.c_str());
  json += ",\"badge\":" + String(BARK_BADGE);
  appendJsonString(json, "sound", BARK_SOUND);
  appendJsonString(json, "icon", BARK_ICON);
  appendJsonString(json, "group", BARK_GROUP);
  appendJsonString(json, "url", BARK_OPEN_URL);
  json += "}";
  return json;
}

static void sendBarkMotionNotification(const MotionEvent& ev) {
  if (BARK_PUSH_URL[0] == '\0' || BARK_DEVICE_KEY[0] == '\0') {
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
  if (!http.begin(client, BARK_PUSH_URL)) {
    Serial.println("[bark] failed to start HTTP request");
    return;
  }

  http.addHeader("Content-Type", "application/json; charset=utf-8");
  int code = http.POST(barkPayload(ev));
  http.end();

  if (code > 0 && code < 400) {
    Serial.printf("[bark] notification sent (HTTP %d)\n", code);
  } else {
    Serial.printf("[bark] notification failed (HTTP %d)\n", code);
  }
}
#else
static void sendBarkMotionNotification(const MotionEvent&) {}
#endif

static void appendEvent(bool motion) {
  MotionEvent ev;
  ev.seq    = ++seqCounter;
  ev.epoch  = syncedEpoch();
  ev.up     = millis();
  ev.motion = motion;

  logBuf[logHead] = ev;
  logHead = (logHead + 1) % PIR_LOG_MAX;
  if (logCount < PIR_LOG_MAX) logCount++;

  if (motion) {
    motionCount++;
    sendBarkMotionNotification(ev);
  }
  Serial.printf("[pir] %s (seq %lu)\n", motion ? "motion detected" : "no motion", ev.seq);
}

void motionBegin() {
  pinMode(PIR_PIN, INPUT);
  // Seed state from the current level without logging a boot-time event.
  bool level = digitalRead(PIR_PIN) == HIGH;
  curState = rawLast = level;
  rawSince = lastPoll = millis();
  Serial.printf("[pir] enabled on GPIO %d (initial %s)\n",
                PIR_PIN, level ? "HIGH" : "LOW");
}

void motionUpdate() {
  unsigned long now = millis();
  if (now - lastPoll < PIR_POLL_MS) return;
  lastPoll = now;

  bool raw = digitalRead(PIR_PIN) == HIGH;
  if (raw != rawLast) {           // raw level moved — restart debounce window
    rawLast = raw;
    rawSince = now;
    return;
  }
  // Raw level has been stable; commit it once it outlasts the debounce window.
  if (raw != curState && now - rawSince >= DEBOUNCE_MS) {
    curState = raw;
    appendEvent(curState);
  }
}

bool motionEnabled() { return true; }
bool motionActive()  { return curState; }
unsigned long motionLatestSeq() { return seqCounter; }

String motionStatusJson() {
  time_t lastEpoch = 0;
  unsigned long lastUp = 0;
  if (logCount > 0) {
    int last = (logHead - 1 + PIR_LOG_MAX) % PIR_LOG_MAX;
    lastEpoch = logBuf[last].epoch;
    lastUp = logBuf[last].up;
  }
  String s = "{";
  s += "\"enabled\":true";
  s += ",\"active\":" + String(curState ? "true" : "false");
  s += ",\"count\":" + String(motionCount);
  s += ",\"lastSeq\":" + String(seqCounter);
  s += ",\"lastTs\":\"" + formatTs(lastEpoch) + "\"";
  s += ",\"lastUp\":" + String(lastUp);
  s += "}";
  return s;
}

String motionLogJson(unsigned long sinceSeq) {
  String s = "{\"latest\":" + String(seqCounter) + ",\"events\":[";
  int start = (logHead - logCount + PIR_LOG_MAX) % PIR_LOG_MAX; // oldest entry
  bool first = true;
  for (int i = 0; i < logCount; i++) {
    const MotionEvent& ev = logBuf[(start + i) % PIR_LOG_MAX];
    if (ev.seq <= sinceSeq) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"seq\":" + String(ev.seq);
    s += ",\"ts\":\"" + formatTs(ev.epoch) + "\"";
    s += ",\"up\":" + String(ev.up);
    s += ",\"m\":" + String(ev.motion ? 1 : 0);
    s += "}";
  }
  s += "]}";
  return s;
}

void motionClearLog() {
  logHead = 0;
  logCount = 0;
  seqCounter = 0;
  motionCount = 0;
  Serial.println("[pir] log cleared");
}

#else // PIR_ENABLED == 0 — compiled-out stubs keep main.cpp guard-free.

void motionBegin() {}
void motionUpdate() {}
bool motionEnabled() { return false; }
bool motionActive()  { return false; }
unsigned long motionLatestSeq() { return 0; }
String motionStatusJson() { return String("{\"enabled\":false}"); }
String motionLogJson(unsigned long) { return String("{\"latest\":0,\"events\":[]}"); }
void motionClearLog() {}

#endif
