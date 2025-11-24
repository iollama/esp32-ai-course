#include <FastLED.h>

// ---------- CONFIGURE YOUR PINS HERE ----------
#define LED_PIN        4      // WS2812 data pin
#define BUTTON_PIN     2      // Button to GND, use INPUT_PULLUP
#define NUM_LEDS       9
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB

CRGB leds[NUM_LEDS];

// Middle “hit” LED (0-based)
const int MIDDLE_INDEX = NUM_LEDS / 2;  // 9/2 = 4

// Movement / speed
int currentIndex = 0;
int direction    = 1;                 // 1 = right, -1 = left

const unsigned long INITIAL_STEP_INTERVAL = 200;  // starting speed (ms per step)
unsigned long stepInterval = INITIAL_STEP_INTERVAL;
unsigned long lastStepTime = 0;

// Scoring
int wins = 0;

// Button state / debounce
bool lastButtonState = HIGH;          // because of INPUT_PULLUP
bool buttonLatched   = false;
unsigned long lastButtonChangeTime = 0;
const unsigned long DEBOUNCE_MS = 30;

void setup() {
  Serial.begin(115200);
  delay(500);

  // LEDs
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear(true);

  // Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("ESP32-C3 LED Game Start (9 LEDs)");
}

// Simple debounced button edge detection (returns true once per press)
bool buttonPressedEdge() {
  bool raw = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if (raw != lastButtonState) {
    // state changed: reset debounce timer
    lastButtonChangeTime = now;
    lastButtonState = raw;
  }

  // After stable for DEBOUNCE_MS, check for press
  if ((now - lastButtonChangeTime) > DEBOUNCE_MS) {
    // Active LOW
    bool pressed = (raw == LOW);
    if (pressed && !buttonLatched) {
      buttonLatched = true;
      return true;  // press edge
    }
    if (!pressed) {
      buttonLatched = false; // reset latch when released
    }
  }
  return false;
}

void showRunnerFrame() {
  // Base background: all off
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // Optionally mark middle zone faintly
  leds[MIDDLE_INDEX] = CRGB(0, 0, 16);

  // Runner color: white normally, yellow in middle zone
  bool inMiddle = (currentIndex == MIDDLE_INDEX);
  CRGB runnerColor = inMiddle ? CRGB::Yellow : CRGB::White;

  leds[currentIndex] = runnerColor;

  FastLED.show();
}

void stepRunnerIfNeeded() {
  unsigned long now = millis();
  if (now - lastStepTime >= stepInterval) {
    lastStepTime = now;

    currentIndex += direction;

    // Bounce at edges
    if (currentIndex <= 0) {
      currentIndex = 0;
      direction = 1;
    } else if (currentIndex >= NUM_LEDS - 1) {
      currentIndex = NUM_LEDS - 1;
      direction = -1;
    }
  }
}

void flashColor(int times, const CRGB& color, int onMs = 200, int offMs = 200) {
  for (int i = 0; i < times; i++) {
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
    delay(onMs);

    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(offMs);
  }
}

void handleWin() {
  wins++;
  Serial.print("WIN! Total wins: ");
  Serial.println(wins);

  // Flash wins count in green
  flashColor(wins, CRGB::Green, 200, 200);

  // Speed up (shorter interval), no top limit
  stepInterval = (unsigned long)(stepInterval * 0.9);

  // Reset runner
  currentIndex = 0;
  direction = 1;
  lastStepTime = millis();
}

void handleLoss() {
  Serial.println("LOSS!");

  // Flash red a few times
  flashColor(3, CRGB::Red, 200, 200);

  // Reset wins and speed
  wins = 0;
  stepInterval = INITIAL_STEP_INTERVAL;

  // Reset runner
  currentIndex = 0;
  direction = 1;
  lastStepTime = millis();
}

void loop() {
  // Move the runner according to the current speed
  stepRunnerIfNeeded();

  // Render LEDs
  showRunnerFrame();

  // Check button press
  if (buttonPressedEdge()) {
    bool inMiddle = (currentIndex == MIDDLE_INDEX);
    if (inMiddle) {
      handleWin();
    } else {
      handleLoss();
    }
  }

  // Small delay to avoid busy loop; timing is controlled by stepInterval
  delay(5);
}
