#include <Arduino.h>

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
// NOTE: Most relay modules are ACTIVE LOW — relay turns ON when pin is LOW.
//       If your relay module is ACTIVE HIGH, set relayOnState/relayOffState below.
// ============================================================================

// ============================================================================
// CONFIGURABLE TIMING (milliseconds)
// ============================================================================

// Relay 1 timing
const unsigned long RELAY1_ON_DURATION  = 10000;  // ON for 10 seconds
const unsigned long RELAY1_OFF_DURATION = 15000;  // OFF for 15 seconds

// Relay 2 timing
const unsigned long RELAY2_ON_DURATION  = 2000;   // ON for 2 seconds
const unsigned long RELAY2_OFF_DURATION = 5000;   // OFF for 5 seconds

// ============================================================================
// PIN CONFIGURATION
// ============================================================================

const int RELAY1_PIN = 26;
const int RELAY2_PIN = 27;

// Relay active state — adjust these if your relay module differs
// Active LOW (most common): ON = LOW, OFF = HIGH
// Active HIGH:              ON = HIGH, OFF = LOW
const int relayOnState  = LOW;
const int relayOffState = HIGH;

// ============================================================================

enum RelayPhase { ON_PHASE, OFF_PHASE };

struct RelayState {
  int pin;
  unsigned long onDuration;
  unsigned long offDuration;
  RelayPhase phase;
  unsigned long lastToggleTime;
};

RelayState relay1;
RelayState relay2;

void initRelay(RelayState& relay, int pin, unsigned long onDuration, unsigned long offDuration) {
  relay.pin = pin;
  relay.onDuration = onDuration;
  relay.offDuration = offDuration;
  relay.phase = ON_PHASE;
  relay.lastToggleTime = millis();

  pinMode(pin, OUTPUT);
  digitalWrite(pin, relayOnState);
}

void updateRelay(RelayState& relay) {
  unsigned long now = millis();
  unsigned long elapsed = now - relay.lastToggleTime;

  if (relay.phase == ON_PHASE && elapsed >= relay.onDuration) {
    relay.phase = OFF_PHASE;
    relay.lastToggleTime = now;
    digitalWrite(relay.pin, relayOffState);
    Serial.print("Relay on pin ");
    Serial.print(relay.pin);
    Serial.println(" -> OFF");
  } else if (relay.phase == OFF_PHASE && elapsed >= relay.offDuration) {
    relay.phase = ON_PHASE;
    relay.lastToggleTime = now;
    digitalWrite(relay.pin, relayOnState);
    Serial.print("Relay on pin ");
    Serial.print(relay.pin);
    Serial.println(" -> ON");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== ESP32 Dual Relay Controller ===");
  Serial.print("Relay 1: GPIO ");
  Serial.print(RELAY1_PIN);
  Serial.print(" | ON ");
  Serial.print(RELAY1_ON_DURATION / 1000);
  Serial.print("s / OFF ");
  Serial.print(RELAY1_OFF_DURATION / 1000);
  Serial.println("s");

  Serial.print("Relay 2: GPIO ");
  Serial.print(RELAY2_PIN);
  Serial.print(" | ON ");
  Serial.print(RELAY2_ON_DURATION / 1000);
  Serial.print("s / OFF ");
  Serial.print(RELAY2_OFF_DURATION / 1000);
  Serial.println("s");
  Serial.println("===================================");

  initRelay(relay1, RELAY1_PIN, RELAY1_ON_DURATION, RELAY1_OFF_DURATION);
  initRelay(relay2, RELAY2_PIN, RELAY2_ON_DURATION, RELAY2_OFF_DURATION);
}

void loop() {
  updateRelay(relay1);
  updateRelay(relay2);
}
