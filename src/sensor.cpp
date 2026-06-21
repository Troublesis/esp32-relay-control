#include "sensor.h"
#include "eventlog.h"
#include "bark.h"

// ============================================================================
// Generic sensor implementation — see include/sensor.h for the contract.
// ============================================================================

void sensorBegin(Sensor& s) {
  if (!s.enabled) return;
  // INPUT_PULLDOWN / INPUT_PULLUP (per wrapper) instead of plain INPUT: an
  // unconnected pin floats and reads as random noise (phantom events with no
  // sensor attached). The pull resistor holds the idle/disconnected line at a
  // known level that a real sensor easily overrides when it asserts. (Input-only
  // pins GPIO 34-39 have no internal pulls — wire an external one there.)
  pinMode(s.pin, s.inputMode);
  bool raw = digitalRead(s.pin) == HIGH;
  s.curState   = (raw == s.alertHigh);
  s.rawLast    = raw;
  s.rawSince   = s.lastPoll = millis();
  s.lastAlertAt = 0;
  s.count      = 0;
  s.lastSeq    = 0;
  Serial.printf("[%s] enabled on GPIO %d (initial raw %s -> %s)\n",
                s.tag, s.pin, raw ? "HIGH" : "LOW", s.curState ? "ALERT" : "clear");
}

void sensorUpdate(Sensor& s) {
  if (!s.enabled) return;
  unsigned long now = millis();
  if (now - s.lastPoll < s.pollMs) return;
  s.lastPoll = now;

  bool raw = digitalRead(s.pin) == HIGH;
  if (raw != s.rawLast) {          // raw level moved — restart debounce window
    s.rawLast = raw;
    s.rawSince = now;
    return;
  }

  bool alert = (raw == s.alertHigh);
  if (alert == s.curState) return;             // no logical change
  if (now - s.rawSince < s.debounceMs) return; // not stable long enough yet

  // Entering the alert state honours the user's detection-delay cooldown so a
  // jittery sensor (or a slow-moving target) doesn't spam the log / pushes.
  if (alert) {
    if (s.lastAlertAt != 0 && now - s.lastAlertAt < s.detectDelayMs) return;
    s.lastAlertAt = now;
  }

  s.curState = alert;
  s.lastSeq = logEvent(s.logSource, alert, alert ? s.alertMsg : s.clearMsg);
  if (alert) {
    s.count++;
    if (s.barkSource >= 0) barkSend(s.barkSource, s.barkTitle, s.barkBody);
  }
  Serial.printf("[%s] %s (seq %lu)\n", s.tag, alert ? s.alertMsg : s.clearMsg, s.lastSeq);
}

bool sensorActive(const Sensor& s) { return s.enabled && s.curState; }

unsigned long sensorDelay(const Sensor& s) { return s.detectDelayMs; }

void sensorSetDelay(Sensor& s, unsigned long ms) {
  s.detectDelayMs = ms;
  Serial.printf("[%s] detection delay -> %lu ms\n", s.tag, ms);
}

void sensorSetInput(Sensor& s, uint8_t inputMode, bool alertHigh) {
  if (!s.enabled) return;
  s.inputMode = inputMode;
  s.alertHigh = alertHigh;
  pinMode(s.pin, s.inputMode);                 // re-apply the pull (UP<->DOWN)
  bool raw = digitalRead(s.pin) == HIGH;
  s.curState = (raw == s.alertHigh);           // re-seed quietly (no event)
  s.rawLast  = raw;
  s.rawSince = s.lastPoll = millis();
  const char* pull = inputMode == INPUT_PULLUP ? "PULLUP"
                   : inputMode == INPUT_PULLDOWN ? "PULLDOWN" : "NONE";
  Serial.printf("[%s] input reconfigured: %s, alert on %s (raw %s -> %s)\n",
                s.tag, pull, alertHigh ? "HIGH" : "LOW",
                raw ? "HIGH" : "LOW", s.curState ? "ALERT" : "clear");
}

String sensorStatusJson(const Sensor& s) {
  if (!s.enabled) return String("{\"enabled\":false}");
  String j = "{\"enabled\":true";
  j += ",\"active\":" + String(s.curState ? "true" : "false");
  j += ",\"count\":" + String(s.count);
  j += ",\"lastSeq\":" + String(s.lastSeq);
  j += ",\"delay\":" + String(s.detectDelayMs);
  j += ",\"raw\":" + String(digitalRead(s.pin) == HIGH ? 1 : 0); // live pin level for wiring checks
  j += ",\"pin\":" + String(s.pin);
  j += "}";
  return j;
}
