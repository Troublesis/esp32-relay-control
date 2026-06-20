#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================================
// Device network identity
// ============================================================================
// Single source of truth for the name this board uses on the network. It is
// DEVICE_HOSTNAME, optionally suffixed with a lowercase tag derived from the
// chip's MAC (e.g. "relay-3a9c") when HOSTNAME_AUTO_SUFFIX is set, so several
// identical boards flashed with the same firmware stay unique on the LAN.
//
// The same value drives the mDNS name, the WiFi/OTA hostname AND the AP setup
// hotspot SSID — so the name you see while provisioning matches the name the
// device answers to once connected. The result is always lowercase and cached
// after the first call.
inline const String& deviceHostname() {
  static String name;
  if (name.length() == 0) {
    name = DEVICE_HOSTNAME;
#if HOSTNAME_AUTO_SUFFIX
    char suffix[6];
    snprintf(suffix, sizeof(suffix), "-%04x",
             (uint16_t)(ESP.getEfuseMac() >> 32)); // top 2 octets of the MAC
    name += suffix;
#endif
    name.toLowerCase();
  }
  return name;
}
