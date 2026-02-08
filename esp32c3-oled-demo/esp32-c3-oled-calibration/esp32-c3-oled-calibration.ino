#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -----------------------
// I2C pins from your board pinout:
static const int I2C_SDA = 5;
static const int I2C_SCL = 6;

// Common SSD1306 I2C addresses
static const uint8_t OLED_ADDR_1 = 0x3C;
static const uint8_t OLED_ADDR_2 = 0x3D;

// Controller framebuffer size
#define OLED_W 128
#define OLED_H 64

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);

// -----------------------
// Visible area assumption (typical for 0.42" variants).
// If your glass is slightly different, you can change these.
// These only affect the "window frame" we draw.
static int VISIBLE_W = 72;
static int VISIBLE_H = 40;

// Start with what you already found "almost works"
static int offsetX = 16;
static int offsetY = 14;

// Crosshair animation (in visible window coords)
static int cx = 0;
static int cy = 0;
static int vx = 1;
static int vy = 1;

static uint32_t lastFrameMs = 0;

static int clampi(int v, int lo, int hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

// Map "visible coordinates" -> controller coordinates by adding offset
static inline int Xv(int x) { return offsetX + x; }
static inline int Yv(int y) { return offsetY + y; }

static bool initDisplay() {
  Wire.begin(I2C_SDA, I2C_SCL);
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_1)) return true;
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_2)) return true;
  return false;
}

static void printHelp() {
  Serial.println();
  Serial.println("=== OLED OFFSET CALIBRATION ===");
  Serial.println("Controls:");
  Serial.println("  a/d : offsetX -/+ 1");
  Serial.println("  w/s : offsetY -/+ 1");
  Serial.println("  A/D : offsetX -/+ 10");
  Serial.println("  W/S : offsetY -/+ 10");
  Serial.println("  r   : reset offsets to (16,14)");
  Serial.println("  p   : print current offsets");
  Serial.println();
}

static void printOffsets() {
  Serial.print("offsetX=");
  Serial.print(offsetX);
  Serial.print("  offsetY=");
  Serial.print(offsetY);
  Serial.print("  visible=");
  Serial.print(VISIBLE_W);
  Serial.print("x");
  Serial.println(VISIBLE_H);
}

static void drawCalibrationScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // 1) Draw a "visible window" frame at (offsetX, offsetY) of size VISIBLE_W x VISIBLE_H
  display.drawRect(Xv(0), Yv(0), VISIBLE_W, VISIBLE_H, SSD1306_WHITE);

  // 2) Mark the four corners (makes clipping obvious)
  display.drawPixel(Xv(0),             Yv(0),              SSD1306_WHITE); // TL
  display.drawPixel(Xv(VISIBLE_W-1),   Yv(0),              SSD1306_WHITE); // TR
  display.drawPixel(Xv(0),             Yv(VISIBLE_H-1),    SSD1306_WHITE); // BL
  display.drawPixel(Xv(VISIBLE_W-1),   Yv(VISIBLE_H-1),    SSD1306_WHITE); // BR

  // 3) Corner labels inside the visible frame (should not be clipped)
  display.setCursor(Xv(2), Yv(2));
  display.print("TL(0,0)");

  // Put TR label near top-right
  display.setCursor(Xv(max(0, VISIBLE_W - 6 * 7)), Yv(2)); // ~7 chars width
  display.print("TR");

  // Put BL label near bottom-left
  display.setCursor(Xv(2), Yv(max(0, VISIBLE_H - 10)));
  display.print("BL");

  // Put BR label near bottom-right
  display.setCursor(Xv(max(0, VISIBLE_W - 6 * 2)), Yv(max(0, VISIBLE_H - 10)));
  display.print("BR");

  // 4) A 10px grid inside visible area (helps see drift)
  for (int x = 10; x < VISIBLE_W; x += 10) {
    display.drawFastVLine(Xv(x), Yv(0), VISIBLE_H, SSD1306_WHITE);
  }
  for (int y = 10; y < VISIBLE_H; y += 10) {
    display.drawFastHLine(Xv(0), Yv(y), VISIBLE_W, SSD1306_WHITE);
  }

  // 5) Draw a moving crosshair inside the visible area
  display.drawLine(Xv(cx), Yv(0), Xv(cx), Yv(VISIBLE_H-1), SSD1306_WHITE);
  display.drawLine(Xv(0),  Yv(cy), Xv(VISIBLE_W-1), Yv(cy), SSD1306_WHITE);
  display.fillCircle(Xv(cx), Yv(cy), 2, SSD1306_WHITE);

  // 6) Print current offsets (inside visible window)
  char buf[32];
  snprintf(buf, sizeof(buf), "X:%d Y:%d", offsetX, offsetY);
  display.setCursor(Xv(2), Yv(12));
  display.print(buf);

  // 7) A test string that is long enough to reveal clipping
  // If you see missing chars on the left/right, adjust offsetX.
  display.setCursor(Xv(2), Yv(22));
  display.print("Tirosh-G");

  // 8) Also show controller origin marker (0,0) as a small cross,
  // so you can see where the real framebuffer origin falls.
  display.drawLine(0, 0, 6, 0, SSD1306_WHITE);
  display.drawLine(0, 0, 0, 6, SSD1306_WHITE);
  display.setCursor(8, 0);
  display.print("(0,0)");

  display.display();
}

static void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == 'a') offsetX -= 1;
    if (c == 'd') offsetX += 1;
    if (c == 'w') offsetY -= 1;
    if (c == 's') offsetY += 1;

    if (c == 'A') offsetX -= 10;
    if (c == 'D') offsetX += 10;
    if (c == 'W') offsetY -= 10;
    if (c == 'S') offsetY += 10;

    if (c == 'r') { offsetX = 16; offsetY = 14; }
    if (c == 'p') { printOffsets(); }

    // Keep offsets sane so we don't draw outside too much
    offsetX = clampi(offsetX, 0, 127);
    offsetY = clampi(offsetY, 0, 63);

    printOffsets();
  }
}

static void animateCrosshair() {
  // Bounce within visible area (keep inside the frame)
  cx += vx;
  cy += vy;
  if (cx <= 1) { cx = 1; vx = 1; }
  if (cx >= VISIBLE_W - 2) { cx = VISIBLE_W - 2; vx = -1; }
  if (cy <= 1) { cy = 1; vy = 1; }
  if (cy >= VISIBLE_H - 2) { cy = VISIBLE_H - 2; vy = -1; }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!initDisplay()) {
    Serial.println("OLED init failed. Check I2C address/wiring.");
    while (true) delay(1000);
  }

  printHelp();
  printOffsets();

  // Initialize crosshair
  cx = VISIBLE_W / 2;
  cy = VISIBLE_H / 2;
}

void loop() {
  handleSerial();

  uint32_t now = millis();
  if (now - lastFrameMs >= 60) { // ~16 FPS
    lastFrameMs = now;
    animateCrosshair();
    drawCalibrationScreen();
  }
}
