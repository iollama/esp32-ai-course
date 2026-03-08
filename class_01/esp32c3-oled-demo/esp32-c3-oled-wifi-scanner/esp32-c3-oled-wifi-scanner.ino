#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ----------------------------------------------------
// CALIBRATED OFFSETS (from your calibration)
// ----------------------------------------------------
static const uint8_t OFFSET_X = 29;
static const uint8_t OFFSET_Y = 24;

// Controller framebuffer size
#define OLED_W 128
#define OLED_H 64

// Visible glass size
static const uint8_t VISIBLE_W = 72;
static const uint8_t VISIBLE_H = 40;

// I2C pins
static const int I2C_SDA = 5;
static const int I2C_SCL = 6;

// OLED I2C addresses
static const uint8_t OLED_ADDR_1 = 0x3C;
static const uint8_t OLED_ADDR_2 = 0x3D;

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);

// -----------------------
// TIMING
// -----------------------
static const uint32_t ROTATE_INTERVAL_MS = 3000;   // 3 seconds per network
static const uint32_t SCAN_SCREEN_MIN_MS = 3000;  // Wifi Scan screen min duration

// -----------------------
static const bool SHOW_HIDDEN = false;
static const int MAX_NETS = 40;

uint32_t lastRotateMs = 0;

int networkCount = 0;      // from scanComplete()
int currentIndex = 0;      // index in sorted list
int sortedIdx[MAX_NETS];

// Radar sweep
int sweepX = 0;
int sweepDir = 1;

// SSID marquee state
int ssidScrollX = 0;
int ssidScrollDir = -1;
uint32_t lastScrollMs = 0;

// -----------------------
static int clampi(int v, int lo, int hi) {
  return (v < lo) ? lo : (v > hi) ? hi : v;
}

static int rssiToStrength(int rssi) {
  int rr = clampi(rssi, -100, -30);
  long s = (long)(rr + 100) * 100L / 70L;
  return clampi((int)s, 0, 100);
}

static const char* encTypeToStr(wifi_auth_mode_t e) {
  switch (e) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/3";
    default: return "UNK";
  }
}

static const char* bandFromChannel(int ch) {
  // Wi-Fi channels 1-14 are 2.4 GHz; 36+ are 5 GHz (simple heuristic).
  // This is sufficient for a scanner UI; it matches common regulatory channel plans.
  if (ch >= 1 && ch <= 14) return "2.4Ghz";
  if (ch >= 32) return "5Ghz";
  return "?Ghz";
}

// -----------------------
// Visible-coordinate drawing helpers
// -----------------------
static void setCursorV(int x, int y) {
  display.setCursor(OFFSET_X + x, OFFSET_Y + y);
}
static void drawLineV(int x0, int y0, int x1, int y1) {
  display.drawLine(OFFSET_X + x0, OFFSET_Y + y0,
                   OFFSET_X + x1, OFFSET_Y + y1, SSD1306_WHITE);
}
static void drawRectV(int x, int y, int w, int h) {
  display.drawRect(OFFSET_X + x, OFFSET_Y + y, w, h, SSD1306_WHITE);
}
static void fillRectV(int x, int y, int w, int h) {
  display.fillRect(OFFSET_X + x, OFFSET_Y + y, w, h, SSD1306_WHITE);
}
static void fillCircleV(int x, int y, int r) {
  display.fillCircle(OFFSET_X + x, OFFSET_Y + y, r, SSD1306_WHITE);
}

// -----------------------
static bool initDisplay() {
  Wire.begin(I2C_SDA, I2C_SCL);
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_1)) return true;
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_2)) return true;
  return false;
}

static void sortByRSSI() {
  int n = min(networkCount, MAX_NETS);
  for (int i = 0; i < n; i++) sortedIdx[i] = i;

  for (int i = 0; i < n - 1; i++) {
    int best = i;
    for (int j = i + 1; j < n; j++) {
      if (WiFi.RSSI(sortedIdx[j]) > WiFi.RSSI(sortedIdx[best]))
        best = j;
    }
    if (best != i) {
      int t = sortedIdx[i];
      sortedIdx[i] = sortedIdx[best];
      sortedIdx[best] = t;
    }
  }
}

// -----------------------
// Radar + right-side power bar
// -----------------------
static void drawRadarWithRightBar(int strength) {
  const int radarTop = 14;
  const int radarBottom = VISIBLE_H - 1;
  const int radarH = radarBottom - radarTop + 1;

  const int barW = 6;
  const int barX = VISIBLE_W - barW;
  drawRectV(barX, radarTop, barW, radarH);

  int barH = (strength * (radarH - 2)) / 100;
  barH = clampi(barH, 0, radarH - 2);
  if (barH > 0) {
    fillRectV(barX + 1, radarBottom - barH + 1, barW - 2, barH);
  }

  int sweepMaxX = VISIBLE_W - barW - 1;
  int sx = clampi(sweepX, 0, sweepMaxX);
  drawLineV(sx, radarTop, sx, radarBottom);

  int y = radarBottom - (strength * (radarH - 3)) / 100;
  y = clampi(y, radarTop + 1, radarBottom - 1);
  fillCircleV(sx, y, 2);

  sweepX += sweepDir * 2;
  if (sweepX <= 0) { sweepX = 0; sweepDir = 1; }
  if (sweepX >= sweepMaxX) { sweepX = sweepMaxX; sweepDir = -1; }
}

// -----------------------
// SSID marquee renderer (left-right bounce if too long)
// -----------------------
static void drawSSIDMarquee(const String& ssid) {
  const int charW = 6; // default Adafruit font
  int textW = ssid.length() * charW;
  int maxX = (int)VISIBLE_W - textW;

  if (textW <= VISIBLE_W) {
    setCursorV(0, 10);
    display.print(ssid);
    return;
  }

  if (millis() - lastScrollMs > 80) {
    lastScrollMs = millis();
    ssidScrollX += ssidScrollDir;

    if (ssidScrollX < maxX) {
      ssidScrollX = maxX;
      ssidScrollDir = 1;
    }
    if (ssidScrollX > 0) {
      ssidScrollX = 0;
      ssidScrollDir = -1;
    }
  }

  setCursorV(ssidScrollX, 10);
  display.print(ssid);
}

// -----------------------
// Wifi Scan screen shown ONCE per cycle (before first network)
// Uses async scan so radar animates while scanning.
// -----------------------
static void scanNowWithAnimatedScreen() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(30);

  // 1) CLEAR previous results + local list state BEFORE scanning
  WiFi.scanDelete();          // clears old scan results stored by WiFi stack
  networkCount = 0;
  currentIndex = 0;
  for (int i = 0; i < MAX_NETS; i++) sortedIdx[i] = 0;

  // 2) START scan ONCE (async)
  bool scanStarted = false;
  int startResult = WiFi.scanNetworks(true, SHOW_HIDDEN); // async
  scanStarted = (startResult == WIFI_SCAN_RUNNING);

  uint32_t t0 = millis();
  while (true) {
    // Animate scan screen
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    setCursorV(0, 0);
    display.print("Wifi Scan");

    // fake strength to animate while scan runs
    int fakeStrength = 20 + (int)((millis() / 40) % 80);
    drawRadarWithRightBar(fakeStrength);

    display.display();
    delay(40);

    int s = WiFi.scanComplete(); // -1 running, -2 not started, >=0 done
    bool minTimeMet = (millis() - t0) >= SCAN_SCREEN_MIN_MS;

    // If (for any reason) scan wasn't running, start it ONCE here (single retry)
    if (!scanStarted && s == WIFI_SCAN_FAILED) {
      // optional: you can handle failure explicitly if you want
    }
    if (!scanStarted && s == WIFI_SCAN_RUNNING) {
      scanStarted = true; // already running (rare path)
    }
    if (!scanStarted && s == WIFI_SCAN_FAILED) {
      // don't spam retries; just exit with 0
      networkCount = 0;
      break;
    }
    if (!scanStarted && s == WIFI_SCAN_FAILED) {
      networkCount = 0;
      break;
    }
    if (!scanStarted && s == -2) {
      // Not started (some cores return -2 when no scan was initiated)
      // Start ONCE (retry) then mark started so we never restart again.
      int r = WiFi.scanNetworks(true, SHOW_HIDDEN);
      scanStarted = (r == WIFI_SCAN_RUNNING);
      // keep looping to animate
      continue;
    }

    // done condition: scan finished AND min screen time elapsed
    if (s >= 0 && minTimeMet) {
      networkCount = s;
      break;
    }
  }

  if (networkCount < 0) networkCount = 0;

  sortByRSSI();

  // Reset cycle state
  currentIndex = 0;
  lastRotateMs = millis();

  // Reset marquee state for first network
  ssidScrollX = 0;
  ssidScrollDir = -1;
  lastScrollMs = 0;
}


// -----------------------
static void renderNetwork(int idxSorted) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (networkCount <= 0) {
    setCursorV(0, 10);
    display.print("No networks");
    drawRadarWithRightBar(20);
    display.display();
    return;
  }

  int n = min(networkCount, MAX_NETS);
  idxSorted = clampi(idxSorted, 0, n - 1);
  int i = sortedIdx[idxSorted];

  String ssid = WiFi.SSID(i);
  int rssi = WiFi.RSSI(i);
  int ch = WiFi.channel(i);
  int strength = rssiToStrength(rssi);

  // Line 1: index/total + band (e.g., "4/4 2.4Ghz")
  setCursorV(0, 0);
  display.print(idxSorted + 1);
  display.print("/");
  display.print(n);
  display.print(" ");
  display.print(bandFromChannel(ch));

  // Line 2: SSID (marquee if long)
  drawSSIDMarquee(ssid);

  // Line 3: RSSI + channel
  setCursorV(0, 20);
  display.print(rssi);
  display.print("dBm CH");
  display.print(ch);

  // Line 4: ENC
  setCursorV(0, 30);
  display.print(encTypeToStr(WiFi.encryptionType(i)));

  // Radar at bottom
  drawRadarWithRightBar(strength);

  display.display();
}

// -----------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  initDisplay();

  // Start first cycle with scan screen
  scanNowWithAnimatedScreen();
}

// -----------------------
void loop() {
  uint32_t now = millis();

  // Advance networks every 3 seconds
  if (now - lastRotateMs >= ROTATE_INTERVAL_MS) {
    lastRotateMs = now;

    int n = min(networkCount, MAX_NETS);

    if (n <= 0) {
      // If nothing found, scan again after each interval
      scanNowWithAnimatedScreen();
      return;
    }

    currentIndex++;

    // Reset marquee on each new network
    ssidScrollX = 0;
    ssidScrollDir = -1;
    lastScrollMs = 0;

    // If we completed a full cycle, scan again (and show Wifi Scan once)
    if (currentIndex >= n) {
      scanNowWithAnimatedScreen();
      return;
    }
  }

  renderNetwork(currentIndex);
  delay(40);
}
