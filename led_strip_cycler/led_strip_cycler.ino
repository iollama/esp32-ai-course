#include <FastLED.h>

// LED strip pin definitions
#define LED_PIN_1 4  // D4 - 9 LEDs
#define LED_PIN_2 5  // D5 - 5 LEDs
#define LED_PIN_3 6  // D6 - 20 LEDs
#define LED_PIN_4 7  // D7 - 9 LEDs
#define LED_PIN_5 8  // D8 - 9 LEDs

// Number of LEDs in each strip
#define NUM_LEDS_1 9
#define NUM_LEDS_2 5
#define NUM_LEDS_3 20
#define NUM_LEDS_4 9
#define NUM_LEDS_5 9

// Button pin
#define BUTTON_PIN 2  // D2

// LED arrays
CRGB leds1[NUM_LEDS_1];
CRGB leds2[NUM_LEDS_2];
CRGB leds3[NUM_LEDS_3];
CRGB leds4[NUM_LEDS_4];
CRGB leds5[NUM_LEDS_5];

// Global variables
int currentStrip = 0;  // Currently active strip (0-4)
int totalStrips = 5;   // Total number of strips
uint8_t rainbowHue = 0;  // Starting hue for rainbow effect

// Button debouncing variables
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup() {
  // Initialize Serial for debugging (optional)
  Serial.begin(115200);
  Serial.println("LED Strip Cycler Starting...");
  
  // Setup button with internal pull-up
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize all LED strips
  // Using WS2812B as the LED type - adjust if you have different strips
  FastLED.addLeds<WS2812B, LED_PIN_1, GRB>(leds1, NUM_LEDS_1);
  FastLED.addLeds<WS2812B, LED_PIN_2, GRB>(leds2, NUM_LEDS_2);
  FastLED.addLeds<WS2812B, LED_PIN_3, GRB>(leds3, NUM_LEDS_3);
  FastLED.addLeds<WS2812B, LED_PIN_4, GRB>(leds4, NUM_LEDS_4);
  FastLED.addLeds<WS2812B, LED_PIN_5, GRB>(leds5, NUM_LEDS_5);
  
  // Set brightness (adjust as needed, 0-255)
  FastLED.setBrightness(128);
  
  // Clear all LEDs at startup
  clearAllStrips();
  FastLED.show();
  
  Serial.println("Setup complete. Press button to cycle strips.");
}

void loop() {
  // Check for button press
  checkButton();
  
  // Update rainbow effect on current strip
  updateRainbow();
  
  // Show the LEDs
  FastLED.show();
  
  // Small delay for animation speed
  delay(20);
  
  // Increment rainbow hue for next frame
  rainbowHue += 1;
}

void checkButton() {
  // Read button state (LOW when pressed due to pull-up)
  int reading = digitalRead(BUTTON_PIN);
  
  // Check if button state has changed
  if (reading != lastButtonState) {
    // Reset debounce timer
    lastDebounceTime = millis();
  }
  
  // If enough time has passed, check if state is stable
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If button state has changed
    if (reading != buttonState) {
      buttonState = reading;
      
      // Only act on button press (transition from HIGH to LOW)
      if (buttonState == LOW) {
        // Cycle to next strip
        cycleStrip();
      }
    }
  }
  
  // Save reading for next iteration
  lastButtonState = reading;
}

void cycleStrip() {
  // Clear current strip before switching
  clearStrip(currentStrip);
  
  // Move to next strip
  currentStrip++;
  if (currentStrip >= totalStrips) {
    currentStrip = 0;
  }
  
  Serial.print("Switched to strip: ");
  Serial.println(currentStrip + 1);
}

void updateRainbow() {
  // Clear all strips first
  clearAllStrips();
  
  // Apply rainbow to current strip only
  switch(currentStrip) {
    case 0:
      fill_rainbow(leds1, NUM_LEDS_1, rainbowHue, 255 / NUM_LEDS_1);
      break;
    case 1:
      fill_rainbow(leds2, NUM_LEDS_2, rainbowHue, 255 / NUM_LEDS_2);
      break;
    case 2:
      fill_rainbow(leds3, NUM_LEDS_3, rainbowHue, 255 / NUM_LEDS_3);
      break;
    case 3:
      fill_rainbow(leds4, NUM_LEDS_4, rainbowHue, 255 / NUM_LEDS_4);
      break;
    case 4:
      fill_rainbow(leds5, NUM_LEDS_5, rainbowHue, 255 / NUM_LEDS_5);
      break;
  }
}

void clearStrip(int stripNum) {
  // Clear specific strip
  switch(stripNum) {
    case 0:
      fill_solid(leds1, NUM_LEDS_1, CRGB::Black);
      break;
    case 1:
      fill_solid(leds2, NUM_LEDS_2, CRGB::Black);
      break;
    case 2:
      fill_solid(leds3, NUM_LEDS_3, CRGB::Black);
      break;
    case 3:
      fill_solid(leds4, NUM_LEDS_4, CRGB::Black);
      break;
    case 4:
      fill_solid(leds5, NUM_LEDS_5, CRGB::Black);
      break;
  }
}

void clearAllStrips() {
  // Clear all LED strips
  fill_solid(leds1, NUM_LEDS_1, CRGB::Black);
  fill_solid(leds2, NUM_LEDS_2, CRGB::Black);
  fill_solid(leds3, NUM_LEDS_3, CRGB::Black);
  fill_solid(leds4, NUM_LEDS_4, CRGB::Black);
  fill_solid(leds5, NUM_LEDS_5, CRGB::Black);
}

// Optional: Add more effects that can be called instead of rainbow
void sparkleEffect(CRGB* leds, int numLeds) {
  fadeToBlackBy(leds, numLeds, 10);
  int pos = random16(numLeds);
  leds[pos] += CHSV(rainbowHue + random8(64), 200, 255);
}

void cometEffect(CRGB* leds, int numLeds) {
  fadeToBlackBy(leds, numLeds, 20);
  int pos = beatsin16(13, 0, numLeds - 1);
  leds[pos] += CHSV(rainbowHue, 255, 192);
}
