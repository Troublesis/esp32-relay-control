#pragma once
#include <Arduino.h>

// ============================================================================
// Generic debounced digital-input sensor
// ============================================================================
// Polls one digital pin, debounces it, and on each committed edge records an
// event into the unified log (eventlog.h) and — when entering its "alert"
// state — optionally fires a Bark push (bark.h). The PIR motion sensor and the
// laser-beam receiver are both thin wrappers over this (see motion.* /
// receiver.*), so the edge/debounce/cooldown logic lives in exactly one place.
//
// Every function is safe to call when `enabled` is false (they become no-ops),
// so wrappers need no #if guards.
// ============================================================================

struct Sensor {
  // ---- configuration: set these before sensorBegin() ----
  bool          enabled;       // compiled / wired in
  int           pin;
  uint8_t       inputMode;     // INPUT_PULLDOWN / INPUT_PULLUP / INPUT
  bool          alertHigh;     // raw level (HIGH = true) that means "alert"
  unsigned long pollMs;        // how often the pin is sampled
  unsigned long debounceMs;    // raw level must hold this long before it counts
  int           logSource;     // LogSource value for the event log
  int           barkSource;    // BarkSource value, or -1 to never notify
  const char*   tag;           // serial prefix, e.g. "pir"
  const char*   alertMsg;      // log/serial text entering alert ("Motion detected")
  const char*   clearMsg;      // log/serial text leaving alert  ("No motion")
  const char*   barkTitle;     // bark title on alert
  const char*   barkBody;      // bark body on alert

  // ---- runtime: managed internally ----
  unsigned long detectDelayMs; // user-tunable minimum gap between alert events
  bool          curState;      // debounced alert state
  bool          rawLast;       // last raw sample (for debounce)
  unsigned long rawSince;      // millis() the raw level last changed
  unsigned long lastPoll;      // millis() of last sample
  unsigned long lastAlertAt;   // millis() of last logged alert (for detectDelayMs)
  unsigned long count;         // alert events since boot
  unsigned long lastSeq;       // event-log seq of the most recent event
};

// Configure the pin and seed the initial state (no event logged at boot).
void sensorBegin(Sensor& s);

// Sample, debounce, and on a committed edge log + (on alert) notify. Call every
// loop(); it rate-limits itself to pollMs internally.
void sensorUpdate(Sensor& s);

// Current alert state (true = motion present / beam broken).
bool sensorActive(const Sensor& s);

// User-tunable detection delay (ms): minimum gap between two logged alerts.
unsigned long sensorDelay(const Sensor& s);
void          sensorSetDelay(Sensor& s, unsigned long ms);

// Re-apply the input wiring at runtime: pull mode (INPUT_PULLUP/PULLDOWN/INPUT)
// and which raw level counts as "alert". Re-runs pinMode() and re-seeds the
// debounced state from a fresh read (no event logged). Lets a wrapper flip a
// sensor's polarity live — e.g. correcting an inverted laser receiver without a
// reflash.
void sensorSetInput(Sensor& s, uint8_t inputMode, bool alertHigh);

// {"enabled":..,"active":..,"count":..,"lastSeq":..,"delay":..}
String sensorStatusJson(const Sensor& s);
