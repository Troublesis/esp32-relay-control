#pragma once
#include <Arduino.h>
#include <Preferences.h>

// ============================================================================
// Bark push notifications — one generic client, four independent toggles
// ============================================================================
// Any source (relay 1, relay 2, the PIR motion sensor, the laser-beam receiver)
// can request a push. Each source has its own on/off switch, persisted in flash
// and flipped from the WebUI (/api/bark). "Available" means the client was
// compiled in (BARK_ENABLED); only then can anything be sent or toggled.
//
// All functions are safe to call when BARK_ENABLED is 0 — they become no-ops /
// report "unavailable", so callers need no #if guards.
// ============================================================================

// Source identifiers — also the index into the persisted toggle array.
enum BarkSource {
  BARK_SRC_RELAY1 = 0,
  BARK_SRC_RELAY2 = 1,
  BARK_SRC_MOTION = 2,
  BARK_SRC_LASER  = 3,   // laser-beam receiver (beam broken)
  BARK_SRC_COUNT  = 4,
};

// True when the Bark client is compiled in (BARK_ENABLED).
bool barkAvailable();

// Live on/off for a source (false when unavailable or out of range).
bool barkEnabled(int source);

// Flip a source toggle and persist it. No-op when unavailable / out of range.
void barkSetEnabled(Preferences& prefs, int source, bool on);

// Restore the persisted toggles + server config, seeded from the BARK_* build
// values (per-source defaults, BARK_PUSH_URL, BARK_DEVICE_KEY).
void barkBegin(Preferences& prefs);

// Master on/off — a single kill switch over every source (persisted). When off,
// barkSend() is a no-op regardless of the per-source toggles.
bool barkMasterEnabled();
void barkSetMaster(Preferences& prefs, bool on);

// Live push endpoint + credentials (persisted; fall back to the build defaults).
String barkPushUrl();
String barkDeviceKey();

// Update + persist the push URL and device key. An empty `key` keeps the current
// one (so the UI never has to echo the secret back). No-op when unavailable.
void barkSetConfig(Preferences& prefs, const String& url, const String& key);

// Send a push for `source` — only if available AND that source is enabled. The
// module appends device name, timestamp and the live LAN URL to `body`.
void barkSend(int source, const String& title, const String& body);

// {"available":bool,"master":bool,"url":"..","keySet":bool,
//  "relay1":bool,"relay2":bool,"motion":bool,"laser":bool}
// The device key itself is never returned — only whether one is set.
String barkStatusJson();
