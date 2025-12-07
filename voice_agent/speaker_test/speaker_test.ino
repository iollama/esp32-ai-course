/*
 * ESP32-S3 Speaker Test with OpenAI TTS
 *
 * This sketch tests the speaker system using OpenAI's TTS API:
 * - Connects to WiFi and syncs time
 * - Downloads OpenAI TTS audio to FFat filesystem
 * - Plays full TTS sentence to validate complete system
 *
 * Hardware:
 * - ESP32-S3-DevKitC-1 N16R8
 * - MAX98357A I2S Amplifier
 * - 2W 8Ω Speaker
 */

#include <WiFi.h>
#include <time.h>
#include <FFat.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "Audio.h"
#include "../config.h"

// Create audio object
Audio audio;

// State tracking for playback
bool testCompleted = false;
unsigned long playbackStartTime = 0;
bool audioStarted = false;
unsigned long lastStatusUpdate = 0;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  // Disable watchdog for Core 0 (audio processing core)
  esp_task_wdt_deinit();

  Serial.println("\n=================================");
  Serial.println("ESP32-S3 Speaker Test");
  Serial.println("=================================\n");
  Serial.println("✓ Watchdog timer disabled");

  // Initialize FFat
  if (!FFat.begin(true)) {
    Serial.println("✗ FFat Mount Failed");
    while(1) delay(1000);
  }
  Serial.println("✓ FFat initialized");

  // Initialize MAX98357A shutdown pin
  pinMode(I2S_SPK_SD_PIN, OUTPUT);
  digitalWrite(I2S_SPK_SD_PIN, HIGH);  // Enable amplifier
  Serial.println("✓ Amplifier enabled");

  // Initialize I2S audio output
  initAudio();

  // Connect to WiFi and sync time
  connectWiFi();
  syncTime();

  // Play TTS test sentence
  playTestSentence();
}

void loop() {
  // Audio processing loop
  audio.loop();

  // Give CPU time back to system
  delay(1);

  // Track when audio actually starts playing
  if (audio.isRunning() && !audioStarted) {
    audioStarted = true;
    playbackStartTime = millis();
    Serial.println("\n▶ Audio playback started");
  }

  // Periodic status updates during playback
  if (audio.isRunning() && (millis() - lastStatusUpdate > 2000)) {
    lastStatusUpdate = millis();
    unsigned long elapsed = (millis() - playbackStartTime) / 1000;
    Serial.print("  Still playing... (");
    Serial.print(elapsed);
    Serial.println("s elapsed)");
  }

  // Check if playback finished
  if (!audio.isRunning() && audioStarted && !testCompleted) {
    unsigned long playbackDuration = (millis() - playbackStartTime) / 1000;

    // TTS test completed - ensure minimum playback time
    if (playbackDuration >= 5) {
      testCompleted = true;
      Serial.println("\n=================================");
      Serial.println("✓ Test completed successfully!");
      Serial.println("=================================");
      Serial.print("Total playback time: ");
      Serial.print(playbackDuration);
      Serial.println(" seconds");
      Serial.println("\nIf you heard the full test sentence,");
      Serial.println("your speaker system is working!");
      Serial.println("\n=================================");
      Serial.println("TEST PASSED!");
      Serial.println("=================================");
    }
  }
}

void connectWiFi() {
  const int MAX_RETRIES = 1;
  const int ATTEMPTS_PER_TRY = 20;  // 20 × 500ms = 10 seconds per attempt

  for (int retry = 0; retry < MAX_RETRIES; retry++) {
    if (retry > 0) {
      Serial.printf("\n⚠ Connection failed. Retry %d/%d...\n", retry + 1, MAX_RETRIES);
      WiFi.disconnect(true);  // Power cycle WiFi
      delay(1000);
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);  // Initialize WiFi hardware first
    delay(100);  // Give WiFi hardware time to initialize

    // Print MAC address after WiFi initialization (only on first attempt)
    if (retry == 0) {
      Serial.print("ESP32 MAC Address: ");
      Serial.println(WiFi.macAddress());
      Serial.println();
    }

    WiFi.setTxPower(WIFI_POWER_17dBm);  // Maximum TX power (19.5 dBm)
    WiFi.setSleep(false);  // Disable power saving

    // Add delay before first connection attempt
    if (retry == 0) {
      WiFi.disconnect(true);
      delay(1000);
    }

    if (0) {
      Serial.println("This is a static IP, Please remvoe these lines!!!");
      // add static ip 
      IPAddress local_IP(192, 168, 68, 50);
      IPAddress gateway(192, 168, 68, 1);
      IPAddress subnet(255, 255, 255, 0);
      IPAddress primaryDNS(8, 8, 8, 8); 
      WiFi.config(local_IP, gateway, subnet, primaryDNS);
    }
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < ATTEMPTS_PER_TRY) {
      delay(500);
      Serial.print(".");
      attempts++;

      // Show progress every 10 seconds
      if (attempts % 10 == 0) {
        Serial.printf(" %ds ", attempts / 2);
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✓ WiFi connected successfully!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Signal Strength: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      return;  // Success - exit function
    }
  }

  // All retries exhausted
  Serial.println("\n\n✗ WiFi connection failed after 3 attempts!");
  Serial.print("\nFailed. Error code: ");
  Serial.println(WiFi.status()); 
  Serial.println("\nPlease check:");
  Serial.println("  1. SSID and password in config.h");
  Serial.println("  2. Router is 2.4GHz (ESP32 doesn't support 5GHz)");
  Serial.println("  3. Router is powered on and in range");
  Serial.println("  4. No special characters in WiFi password");
  Serial.println("\nSystem halted. Reset to try again.");
  while(1) delay(5000);
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

  // Increase buffer size to prevent jitter/underruns and reduce CPU load
  audio.setConnectionTimeout(10000, 5000);   // 10s timeout, 5s reconnect (longer for large text)
  audio.setInBufferSize(131072);             // 128KB input buffer (increased from 64KB to reduce watchdog pressure)

  Serial.println("✓ Audio initialized");
  Serial.print("Volume: 15/21");
  Serial.println();
}

bool downloadTTSToFile(const char* text, const char* filename) {
  Serial.println("Downloading TTS audio to FFat...");

  WiFiClientSecure client;  
  client.setInsecure();  // Skip certificate validation for simplicity

  HTTPClient https;

  String url = "https://api.openai.com/v1/audio/speech";

  if (!https.begin(client, url)) {
    Serial.println("✗ HTTPS connection failed");
    return false;
  }

  // Set headers
  https.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
  https.addHeader("Content-Type", "application/json");

  // Create JSON payload
  String payload = "{\"model\":\"" + String(TTS_MODEL) +
                   "\",\"input\":\"" + String(text) +
                   "\",\"voice\":\"" + String(TTS_VOICE) +
                   "\",\"response_format\":\"mp3\"," +
                   "\"speed\":" + String(TTS_SPEED) + "}";

  Serial.println("Sending POST request...");
  int httpCode = https.POST(payload);

  if (httpCode != 200) {
    Serial.printf("✗ HTTP error: %d\n", httpCode);
    https.end();
    return false;
  }

  // Get content length
  int len = https.getSize();
  Serial.printf("Response size: %d bytes\n", len);

  // Add diagnostic
  Serial.printf("Connected: %d\n", https.connected());
  Serial.printf("Stream available: %d\n", https.getStreamPtr()->available());

  // Open file for writing
  File file = FFat.open(filename, "w");
  if (!file) {
    Serial.println("✗ Failed to open file for writing");
    https.end();
    return false;
  }

  // Download and save to file
  WiFiClient* stream = https.getStreamPtr();
  uint8_t buffer[1024];
  int bytesWritten = 0;
  int loopCount = 0;
  int noDataCount = 0;  // Track consecutive loops with no data

  Serial.println("Starting download loop...");

  while (https.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();

    if (loopCount % 100 == 0) {  // Debug every 100 iterations
      Serial.printf("Loop %d: connected=%d, available=%d, written=%d\n",
                    loopCount, https.connected(), size, bytesWritten);
    }

    if (size) {
      int c = stream->readBytes(buffer, min((size_t)sizeof(buffer), size));
      file.write(buffer, c);
      bytesWritten += c;
      noDataCount = 0;  // Reset no-data counter when we receive data

      if (len > 0) {
        len -= c;
      }

      // Progress indicator
      if (bytesWritten % 10240 == 0) {
        Serial.print(".");
      }
    } else {
      noDataCount++;

      // For chunked encoding (len == -1), exit if no data for 3 seconds
      if (len == -1 && noDataCount > 1000 && bytesWritten > 0) {
        Serial.println("\n✓ Download complete (no more data)");
        break;
      }
    }

    delay(1);
    loopCount++;

    // Timeout after 30 seconds of no data at all
    if (loopCount > 30000 && bytesWritten == 0) {
      Serial.println("\n✗ Download timeout - no data received");
      break;
    }
  }

  Serial.printf("\nDownload loop exited after %d iterations\n", loopCount);

  Serial.println();
  Serial.printf("✓ Downloaded %d bytes to %s\n", bytesWritten, filename);

  file.close();
  https.end();

  return true;
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
  const char* testText = "Hello! This is a test of the speaker system. If you can hear this message clearly, your hardware is working correctly. Hello! This is a test of the speaker system. If you can hear this message clearly, your hardware is working correctly. Hello! This is a test of the speaker system. If you can hear this message clearly, your hardware is working correctly.";

  Serial.print("Speaking: ");
  Serial.println(testText);
  Serial.print("Text length: ");
  Serial.print(strlen(testText));
  Serial.println(" characters");
  Serial.println();

  // Download TTS to file
  const char* audioFile = "/tts_test.mp3";

  if (!downloadTTSToFile(testText, audioFile)) {
    Serial.println("✗ Failed to download TTS audio");
    return;
  }

  // Verify file exists
  File file = FFat.open(audioFile, "r");
  if (!file) {
    Serial.println("✗ Could not open downloaded file");
    return;
  }
  size_t fileSize = file.size();
  file.close();

  Serial.printf("✓ Audio file ready (%d bytes)\n", fileSize);
  Serial.println("Starting playback from FFat...\n");

  // Play from file
  bool success = audio.connecttoFS(FFat, audioFile);

  if (success) {
    Serial.println("✓ Playback started");
    Serial.println("(Check loop() for real-time status updates)\n");
  } else {
    Serial.println("✗ Failed to start playback");
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
