/*
 * ESP32-S3 Speaker Test with OpenAI TTS
 *
 * This sketch tests the MAX98357A amplifier and speaker by:
 * 1. Connecting to WiFi
 * 2. Syncing time via NTP (required for HTTPS)
 * 3. Playing a test sentence using OpenAI TTS
 *
 * Hardware:
 * - ESP32-S3-DevKitC-1 N16R8
 * - MAX98357A I2S Amplifier
 * - 2W 8Ω Speaker
 */

#include <WiFi.h>
#include <time.h>
#include "Audio.h"
#include "../config.h"

// Create audio object
Audio audio;

// State tracking
bool testCompleted = false;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("ESP32-S3 Speaker Test");
  Serial.println("=================================\n");

  // Initialize MAX98357A shutdown pin
  pinMode(I2S_SPK_SD_PIN, OUTPUT);
  digitalWrite(I2S_SPK_SD_PIN, HIGH);  // Enable amplifier
  Serial.println("✓ Amplifier enabled");

  // Connect to WiFi
  connectWiFi();

  // Sync time (required for HTTPS)
  syncTime();

  // Initialize I2S audio output
  initAudio();

  // Play test sentence
  playTestSentence();
}

void loop() {
  // Audio processing loop
  audio.loop();

  // Check if playback finished
  if (!audio.isRunning() && !testCompleted) {
    testCompleted = true;
    Serial.println("\n=================================");
    Serial.println("✓ Test completed successfully!");
    Serial.println("=================================");
    Serial.println("\nIf you heard the test sentence,");
    Serial.println("your speaker system is working!");
  }
}

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  #if WIFI_SLEEP_DISABLED
  WiFi.setSleep(false);
  #endif

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n✗ WiFi connection failed!");
    Serial.println("Please check your credentials in config.h");
    while(1) delay(1000);
  }
}

void syncTime() {
  Serial.print("Syncing time with NTP...");

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (getLocalTime(&timeinfo)) {
    Serial.println("\n✓ Time synced");
    Serial.print("Current time: ");
    Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
  } else {
    Serial.println("\n✗ Time sync failed!");
    Serial.println("TTS may not work without proper time sync");
  }
}

void initAudio() {
  Serial.println("Initializing I2S audio...");

  // Set I2S pins for MAX98357A
  audio.setPinout(I2S_SPK_BCLK_PIN, I2S_SPK_LRC_PIN, I2S_SPK_DIN_PIN);

  // Set volume (0-21, default 12)
  audio.setVolume(15);  // ~70% volume

  Serial.println("✓ Audio initialized");
  Serial.print("Volume: 15/21");
  Serial.println();
}

void playTestSentence() {
  Serial.println("\n=================================");
  Serial.println("Playing test sentence...");
  Serial.println("=================================\n");

  Serial.println("Model: " + String(TTS_MODEL));
  Serial.println("Voice: " + String(TTS_VOICE));
  Serial.println("Speed: " + String(TTS_SPEED));
  Serial.println();

  // The test sentence
  const char* testText = "Hello! This is a test of the speaker system. If you can hear this message clearly, your hardware is working correctly.";

  Serial.print("Speaking: ");
  Serial.println(testText);
  Serial.println();

  // Play using OpenAI TTS
  // Parameters: api_key, model, input, instructions, voice, response_format, speed
  bool success = audio.openai_speech(
    OPENAI_API_KEY,
    TTS_MODEL,
    testText,
    "",         // instructions (empty for basic TTS)
    TTS_VOICE,
    "mp3",      // format: mp3, opus, aac, flac
    String(TTS_SPEED)
  );

  if (success) {
    Serial.println("✓ TTS request sent successfully");
    Serial.println("Audio streaming...");
  } else {
    Serial.println("✗ TTS request failed!");
    Serial.println("\nPossible issues:");
    Serial.println("- Check API key in config.h");
    Serial.println("- Verify internet connection");
    Serial.println("- Check OpenAI API quota/credits");
  }
}

// Optional: Audio library callbacks for debugging
void audio_info(const char *info) {
  Serial.print("Audio info: ");
  Serial.println(info);
}

void audio_eof_mp3(const char *info) {
  Serial.print("End of file: ");
  Serial.println(info);
}
