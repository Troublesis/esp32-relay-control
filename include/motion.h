#pragma once
#include <Arduino.h>

// ============================================================================
// PIR motion sensor module (e.g. HS-S38P)
// ============================================================================
// Polls a digital input pin, detects rising/falling edges (motion / no-motion)
// with light debouncing, and records each change into a bounded in-RAM ring
// buffer (newest wins, oldest dropped past PIR_LOG_MAX). Each event is stamped
// with the NTP epoch when available and always with the device uptime.
//
// Kept decoupled from the relay/WiFi globals in main.cpp — main only calls the
// lifecycle hooks and concatenates the JSON helpers into its existing payloads.
// All functions are safe to call when PIR_ENABLED is 0 (they become no-ops /
// report "disabled"), so the rest of the firmware needs no #if guards.
// ============================================================================

// Configure the input pin and seed the initial state (no event logged at boot).
void motionBegin();

// Sample the pin and append an event on a debounced state change. Call every
// loop(); it rate-limits itself to PIR_POLL_MS internally.
void motionUpdate();

// True when the sensor is compiled in (PIR_ENABLED).
bool motionEnabled();

// Current logical state — true while motion is present (signal HIGH).
bool motionActive();

// Sequence number of the most recent event (0 if none recorded yet). Increases
// monotonically; used by the WebUI to fetch the log incrementally.
unsigned long motionLatestSeq();

// Motion object for /api/status: {"enabled":..,"active":..,"count":..,
// "lastSeq":..,"lastTs":"..","lastUp":..}. Returns the full braced object.
String motionStatusJson();

// Event log as JSON, returning only events with seq > sinceSeq (oldest first):
//   {"latest":N,"events":[{"seq":..,"ts":"YYYY-MM-DD HH:MM:SS","up":ms,"m":1}]}
// "ts" is empty when the event predates an NTP sync; "m" is 1=detected, 0=clear.
String motionLogJson(unsigned long sinceSeq);

// Forget all recorded events and reset the sequence counter (keeps pin state).
void motionClearLog();
