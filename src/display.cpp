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

bool displayBegin() {
  Wire.begin(OLED_SDA, OLED_SCL);
  ready = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!ready) {
    Serial.println("[oled] SSD1306 not found — display disabled");
    return false;
  }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.display();
  Serial.println("[oled] ready");
  return true;
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

// Two-line motion indicator down the right side of the blue area. Mirrors the
// relay "pill" invert: outlined with the label showing when idle, fully filled
// with the label knocked out to black when motion is live.
//   NOTE: this lower region of the two-colour panel is physically BLUE (only the
//   top 16 px band is yellow), so the box renders blue regardless of draw colour.
//   "Motion Detected" is trimmed to "Motion" / "Detect" to fit the box width.
static void drawMotionBox(const DisplayInfo& info) {
  const int x = 84, y = 20, w = 44, h = 32;
  bool on = info.motionActive;

  if (on) oled.fillRoundRect(x, y, w, h, 4, SSD1306_WHITE);
  else    oled.drawRoundRect(x, y, w, h, 4, SSD1306_WHITE);
  oled.setTextColor(on ? SSD1306_BLACK : SSD1306_WHITE);

  oled.setTextSize(1);
  const char* lines[2] = { "Motion", "Detect" };
  const int   ly[2]    = { y + 7, y + 18 };
  for (int i = 0; i < 2; i++) {
    int16_t bx, by; uint16_t bw, bh;
    oled.getTextBounds(lines[i], 0, 0, &bx, &by, &bw, &bh);
    oled.setCursor(x + (w - (int)bw) / 2, ly[i]);
    oled.print(lines[i]);
  }
  oled.setTextColor(SSD1306_WHITE); // restore for later draws
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

  // ---- Blue area: relays (left) + motion box (right) ----
  bool showMotion = info.motionEnabled;
  drawRelayRow(22, 0, info, !showMotion); // countdown yields to the motion box
  drawRelayRow(40, 1, info, !showMotion);
  if (showMotion) drawMotionBox(info);

  // ---- Footer hint ----
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 56);
  if (info.apMode) oled.print("Join AP to set WiFi");
  else             oled.print(info.hostname + ".local");
  // (The standalone footer PIR tag is gone — the motion box above now shows it.)

  oled.display();
}
