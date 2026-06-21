#pragma once
#include <Arduino.h>

// ============================================================================
// Laser-beam receiver — a thin, named wrapper over the generic Sensor
// (sensor.h). Point the laser emitter at the receiver; "alert" means the beam
// is broken (not received). Beam-break / beam-restore events land in the unified
// event log (eventlog.h) and can fire a Bark push (bark.h) when the
// BARK_SRC_LASER toggle is on.
//
// All functions are safe to call when RECEIVER_ENABLED is 0 (they become
// no-ops / report "disabled"), so the rest of the firmware needs no #if guards.
// ============================================================================

// Configure the input pin and seed the initial state (no event logged at boot).
void receiverBegin();

// Sample the pin and append an event on a debounced state change. Call every
// loop(); it rate-limits itself internally.
void receiverUpdate();

// True when the receiver is compiled in (RECEIVER_ENABLED).
bool receiverEnabled();

// Current logical state — true while the beam is broken (not received).
bool receiverActive();

// User-tunable detection delay (ms): minimum gap between logged break events.
void          receiverSetDelay(unsigned long ms);
unsigned long receiverDelay();

// Beam-present signal level (runtime-adjustable, persisted by the caller).
//   true  = the receiver reads HIGH while the beam lands on it (RECEIVER_BEAM_HIGH)
//   false = it reads LOW while the beam lands on it
// Flipping this also swaps the input pull (PULLDOWN<->PULLUP) so a disconnected
// sensor still fails safe to "beam broken". Use it to correct an inverted laser
// receiver module live, without reflashing.
void receiverSetBeamHigh(bool high);
bool receiverBeamHigh();

// Receiver object for /api/status:
//   {"enabled":..,"active":..,"count":..,"lastSeq":..,"delay":..}
String receiverStatusJson();
