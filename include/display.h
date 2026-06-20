#pragma once
#include <Arduino.h>

// Snapshot of everything the OLED needs to draw, so the display module stays
// decoupled from the relay/WiFi globals in main.cpp.
struct DisplayInfo {
  bool apMode;
  bool wifiConnected;
  String ssid;
  String ip;
  int rssi;
  bool relayOn[2];
  bool relayAuto[2];
  unsigned long remaining[2]; // seconds to next auto switch (0 in manual)
  bool motionEnabled;         // PIR sensor compiled in
  bool motionActive;          // PIR currently detecting motion
  const char* version;
};

// Initialise the I2C OLED. Returns false if no panel responds (rendering then
// becomes a no-op, so the rest of the firmware is unaffected).
bool displayBegin();

// Centered two-line message — used for boot / status splashes.
void displaySplash(const char* line1, const char* line2);

// Render the live dashboard. Safe to call even if displayBegin() failed.
void displayRender(const DisplayInfo& info);
