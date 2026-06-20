#include "display.h"
#include "config.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// 0.96" SSD1306 OLED driver (128x64, I2C)
// ============================================================================
// The popular cheap panel is two-color: the top 16 px row is YELLOW and the
// remaining 48 px are BLUE (a hardware property of the glass, not software). We
// lean into that split:
//   • Yellow band  -> title + network line (always-visible status)
//   • Blue area    -> per-relay state, mode and auto-cycle countdown
//
// Wiring (see also include/config.h):
//   OLED GND -> ESP32 GND
//   OLED VCC -> ESP32 3V3   (5V also fine on most modules)
//   OLED SCL -> ESP32 GPIO 22   (OLED_SCL)
//   OLED SDA -> ESP32 GPIO 21   (OLED_SDA)
// ============================================================================

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool ready = false;

// Does a device ACK at `addr` on the given pins? Adafruit_SSD1306::begin() does
// NOT verify the panel actually responds on I2C (it returns true even with the
// wrong pin, then silently NAKs every write — "ready" in the log, blank glass),
// so we probe the bus ourselves before trusting it.
static bool i2cPresent(int sda, int scl, uint8_t addr) {
  Wire.begin(sda, scl);
  delay(10);
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0; // 0 = address ACKed
}

// Log every device answering on the active I2C bus (diagnostic on failure).
static void i2cScan() {
  byte found = 0;
  Serial.print("[oled] I2C scan:");
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); found++; }
  }
  Serial.println(found ? "" : " (no devices found)");
}

bool displayBegin() {
  // Try the configured pins/address first, then the alternate I2C address
  // (0x3C<->0x3D) and the other common SDA pin (21<->23). A board wired slightly
  // differently — or with OLED_SDA left at the wrong default — still lights up,
  // and the working combo is logged so you can lock it into config.h.
  const uint8_t altAddr = (OLED_ADDR == 0x3C) ? 0x3D : 0x3C;
  const int     altSda  = (OLED_SDA == 21) ? 23 : 21;
  const struct { int sda; int scl; uint8_t addr; } combos[] = {
    { OLED_SDA, OLED_SCL, (uint8_t)OLED_ADDR },
    { OLED_SDA, OLED_SCL, altAddr },
    { altSda,   OLED_SCL, (uint8_t)OLED_ADDR },
    { altSda,   OLED_SCL, altAddr },
  };

  for (auto& c : combos) {
    if (!i2cPresent(c.sda, c.scl, c.addr)) continue;
    Wire.begin(c.sda, c.scl);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, c.addr)) continue; // buffer alloc failed
    ready = true;
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.display();
    if (c.sda == OLED_SDA && c.addr == OLED_ADDR)
      Serial.printf("[oled] ready (SDA=%d SCL=%d addr=0x%02X)\n", c.sda, c.scl, c.addr);
    else
      Serial.printf("[oled] ready via FALLBACK (SDA=%d addr=0x%02X) — set OLED_SDA=%d / OLED_ADDR=0x%02X in config.h\n",
                    c.sda, c.addr, c.sda, c.addr);
    return true;
  }

  // Nothing answered anywhere — scan the configured bus so the cause is visible.
  Wire.begin(OLED_SDA, OLED_SCL);
  i2cScan();
  Serial.println("[oled] SSD1306 not found — display disabled (check wiring/power/pins)");
  return false;
}

void displaySplash(const char* line1, const char* line2) {
  if (!ready) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  int16_t x1, y1; uint16_t w, h;

  oled.setTextSize(2);
  oled.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((OLED_WIDTH - w) / 2, 16);
  oled.print(line1);

  oled.setTextSize(1);
  oled.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((OLED_WIDTH - w) / 2, 44);
  oled.print(line2);

  oled.display();
}

// One relay status line in the blue area: "R1 [ON ] AUTO 12s".
static void drawRelayRow(int y, int idx, const DisplayInfo& info, bool showCountdown) {
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  // Label
  oled.setCursor(0, y + 2);
  oled.print("R");
  oled.print(idx + 1);

  // State "pill" — filled when ON, outlined when OFF
  const int px = 16, pw = 28, ph = 13;
  bool on = info.relayOn[idx];
  if (on) {
    oled.fillRoundRect(px, y, pw, ph, 3, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
  } else {
    oled.drawRoundRect(px, y, pw, ph, 3, SSD1306_WHITE);
    oled.setTextColor(SSD1306_WHITE);
  }
  oled.setCursor(px + (on ? 7 : 5), y + 3);
  oled.print(on ? "ON" : "OFF");
  oled.setTextColor(SSD1306_WHITE);

  // Mode
  oled.setCursor(52, y + 2);
  oled.print(info.relayAuto[idx] ? "AUTO" : "MAN");

  // Auto countdown (right aligned-ish) — hidden when the motion box claims the
  // right side of the blue area; the live value stays available in the WebUI.
  if (showCountdown && info.relayAuto[idx]) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%lus", info.remaining[idx]);
    int16_t x1, y1; uint16_t w, h;
    oled.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    oled.setCursor(OLED_WIDTH - w, y + 2);
    oled.print(buf);
  }
}

// One labelled detection pill on the right side of the blue area. Mirrors the
// relay "pill" invert: outlined with the label showing when idle, fully filled
// with the label knocked out to black when the sensor is in its alert state.
//   NOTE: this lower region of the two-colour panel is physically BLUE (only the
//   top 16 px band is yellow), so the box renders blue regardless of draw colour.
static void drawSensorPill(int x, int y, int w, int h, const char* label, bool active) {
  if (active) oled.fillRoundRect(x, y, w, h, 3, SSD1306_WHITE);
  else        oled.drawRoundRect(x, y, w, h, 3, SSD1306_WHITE);
  oled.setTextColor(active ? SSD1306_BLACK : SSD1306_WHITE);

  oled.setTextSize(1);
  int16_t bx, by; uint16_t bw, bh;
  oled.getTextBounds(label, 0, 0, &bx, &by, &bw, &bh);
  oled.setCursor(x + (w - (int)bw) / 2, y + (h - 8) / 2 + 1);
  oled.print(label);
  oled.setTextColor(SSD1306_WHITE); // restore for later draws
}

// Stacked detection indicators ("Motion" + "Laser") down the right side. Each is
// shown only when its sensor is compiled in, taking the next free slot.
static void drawSensorColumn(const DisplayInfo& info) {
  const int x = 84, w = 44, h = 15;
  int y = 20;
  if (info.motionEnabled) { drawSensorPill(x, y, w, h, "Motion", info.motionActive); y += 18; }
  if (info.laserEnabled)  { drawSensorPill(x, y, w, h, "Laser",  info.laserActive);  y += 18; }
}

void displayRender(const DisplayInfo& info) {
  if (!ready) return;

  static bool blink = false;
  blink = !blink;

  oled.clearDisplay();

  // ---- Yellow band: title + network ----
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.print("Relay v");
  oled.print(info.version);

  // top-right status tag
  if (info.apMode) {
    oled.setCursor(OLED_WIDTH - 12, 0);
    oled.print("AP");
  } else if (info.wifiConnected) {
    char rb[8];
    snprintf(rb, sizeof(rb), "%d", info.rssi);
    int16_t x1, y1; uint16_t w, h;
    oled.getTextBounds(rb, 0, 0, &x1, &y1, &w, &h);
    oled.setCursor(OLED_WIDTH - w, 0);
    oled.print(rb);
  }
  // liveness heartbeat dot
  if (blink) oled.fillCircle(OLED_WIDTH - 2, 12, 1, SSD1306_WHITE);

  oled.setCursor(0, 8);
  if (info.apMode) {
    oled.print(info.ssid);
  } else if (info.wifiConnected) {
    oled.print(info.ip);
  } else {
    oled.print("WiFi connecting...");
  }

  oled.drawFastHLine(0, 16, OLED_WIDTH, SSD1306_WHITE);

  // ---- Blue area: relays (left) + Motion/Laser indicators (right) ----
  bool showSensors = info.motionEnabled || info.laserEnabled;
  drawRelayRow(22, 0, info, !showSensors); // countdown yields to the sensor pills
  drawRelayRow(40, 1, info, !showSensors);
  if (showSensors) drawSensorColumn(info);

  // ---- Footer hint ----
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 56);
  if (info.apMode) oled.print("Join AP to set WiFi");
  else             oled.print(info.hostname + ".local");
  // (The standalone footer PIR tag is gone — the motion box above now shows it.)

  oled.display();
}
