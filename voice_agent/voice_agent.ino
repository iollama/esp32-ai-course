/*
 * ESP32-S3 AI Voice Assistant
 *
 * Complete push-to-talk voice assistant that integrates:
 * - INMP441 microphone for audio capture
 * - OpenAI Whisper API for speech-to-text
 * - GPT-4o mini for intelligent responses
 * - OpenAI TTS for voice synthesis
 *
 * Hardware:
 * - ESP32-S3-DevKitC-1 N16R8
 * - INMP441 I2S microphone
 * - MAX98357A I2S amplifier + speaker
 * - Push button on GPIO 10
 */

// ===== INCLUDES =====
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <driver/i2s_std.h>
#include <ArduinoJson.h>
#include "Audio.h"
#include "config.h"

// ===== DEFINES =====
#define I2S_PORT I2S_NUM_0
#define SAMPLES_PER_READ 512
#define DEBOUNCE_MS 50

// ===== STATE MACHINE =====
enum VoiceState {
  IDLE,
  RECORDING,
  PROCESSING_WHISPER,
  PROCESSING_CHAT,
  SPEAKING,
  ERROR_STATE
};

enum ErrorType {
  ERROR_NONE,
  ERROR_WIFI_DISCONNECTED,
  ERROR_WHISPER_FAILED,
  ERROR_CHAT_FAILED,
  ERROR_TTS_FAILED,
  ERROR_RECORDING_FAILED
};

// ===== GLOBAL VARIABLES =====
VoiceState currentState = IDLE;
ErrorType currentError = ERROR_NONE;

// Audio buffers (PSRAM)
int16_t* audioBuffer = nullptr;
uint32_t bufferWritePos = 0;
int32_t i2sSamples[SAMPLES_PER_READ];

// Audio library (using I2S_NUM_1 for speaker)
Audio audio(I2S_NUM_1);

// Button state
unsigned long buttonPressTime = 0;
bool buttonWasPressed = false;

// I2S driver state
i2s_chan_handle_t i2s_mic_handle = nullptr;

// Conversation memory
String previousResponseId = "";  // Store response ID for context
unsigned long lastInteractionTime = 0;

// Error handling
unsigned long errorStartTime = 0;

// ===== WAV HEADER STRUCTURE =====
struct WAVHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t fileSize;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtSize = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = 1;
  uint32_t sampleRate = MIC_SAMPLE_RATE;
  uint32_t byteRate = MIC_SAMPLE_RATE * 2;
  uint16_t blockAlign = 2;
  uint16_t bitsPerSample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize;
};

// ===== FUNCTION PROTOTYPES =====
// Setup functions
void connectWiFi();
void syncTime();
void allocatePSRAMBuffers();

// I2S lifecycle
void initMicrophone();
void initSpeaker();

// State handlers
void handleIdle();
void handleRecording();
void handleError();

// Recording functions
void startRecording();
void captureAudioChunk();
void stopRecording();
void processAudio();

// API functions
String transcribeAudio(int16_t* samples, uint32_t sampleCount);
String getChatResponse(String userMessage);
void speakResponse(String text);

// Audio processing
WAVHeader createWAVHeader(uint32_t sampleCount);
String parseWhisperResponse(String response);

// Network functions
String readHTTPResponse(WiFiClientSecure& client);
String escapeJSON(String str);

// Utility functions
void setError(ErrorType error, const char* message);
void onSpeakingComplete();

// ===== SETUP =====
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  Serial.println("\n\n=================================");
  Serial.println("ESP32-S3 AI Voice Assistant");
  Serial.println("=================================\n");

  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println(" Button initialized (GPIO 10)");

  // Initialize speaker shutdown pin (start disabled)
  pinMode(I2S_SPK_SD_PIN, OUTPUT);
  digitalWrite(I2S_SPK_SD_PIN, LOW);
  Serial.println(" Speaker amplifier initialized");

  // Connect to WiFi
  connectWiFi();

  // Sync time (critical for HTTPS)
  syncTime();

  // Allocate PSRAM buffers
  allocatePSRAMBuffers();

  // Initialize BOTH I2S devices permanently
  initMicrophone();  // I2S_NUM_0
  initSpeaker();     // I2S_NUM_1 (via Audio library)

  // Set initial state
  currentState = IDLE;

  Serial.println("\n=================================");
  Serial.println(" Ready - Press button to speak");
  Serial.println("=================================\n");
}

// ===== MAIN LOOP =====
void loop() {
  switch (currentState) {
    case IDLE:
      handleIdle();
      break;

    case RECORDING:
      handleRecording();
      break;

    case PROCESSING_WHISPER:
    case PROCESSING_CHAT:
      // Blocking API calls, no loop processing needed
      break;

    case SPEAKING:
      audio.loop();  // REQUIRED for ESP32-audioI2S
      if (!audio.isRunning()) {
        onSpeakingComplete();
      }
      break;

    case ERROR_STATE:
      handleError();
      break;
  }

  delay(10);  // Prevent watchdog issues
}

// ===== NETWORK FUNCTIONS =====
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
    Serial.println("\n WiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
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
    Serial.println("\n Time synced");
    Serial.print("Current time: ");
    Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
  } else {
    Serial.println("\nTime sync failed!");
    Serial.println("TTS may not work without proper time sync");
  }
}

void allocatePSRAMBuffers() {
  Serial.println("Allocating PSRAM buffers...");

  audioBuffer = (int16_t*)ps_malloc(INPUT_BUFFER_SIZE);
  if (!audioBuffer) {
    Serial.println("Failed to allocate audio buffer");
    while(1) delay(1000);
  }

  Serial.println("PSRAM buffers allocated");
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("Free PSRAM: ");
  Serial.println(ESP.getFreePsram());
}

// ===== I2S LIFECYCLE FUNCTIONS =====
void initMicrophone() {
  Serial.println("Initializing microphone...");

  // Configure I2S channel for RX (microphone input)
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = DMA_BUFFER_COUNT;      // 8
  chan_cfg.dma_frame_num = DMA_BUFFER_SIZE;      // 1024
  chan_cfg.auto_clear = true;

  // Create RX channel only (NULL for TX)
  esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &i2s_mic_handle);
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %d\n", err);
    while (1) delay(1000);
  }

  // Configure standard I2S mode (Philips format)
  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_MIC_SCK_PIN,
      .ws = (gpio_num_t)I2S_MIC_WS_PIN,
      .dout = I2S_GPIO_UNUSED,
      .din = (gpio_num_t)I2S_MIC_SD_PIN,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };

  // Initialize standard mode
  err = i2s_channel_init_std_mode(i2s_mic_handle, &std_cfg);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
    while (1) delay(1000);
  }

  // Enable channel
  err = i2s_channel_enable(i2s_mic_handle);
  if (err != ESP_OK) {
    Serial.printf(" Failed to enable I2S channel: %d\n", err);
    while (1) delay(1000);
  }

  Serial.println(" Microphone driver installed");
}

void initSpeaker() {
  Serial.println("Initializing speaker...");

  // Enable amplifier
  digitalWrite(I2S_SPK_SD_PIN, HIGH);
  delay(50);

  // Configure Audio library to use I2S_NUM_1
  // Note: Audio object was constructed with I2S_NUM_1 parameter
  audio.setPinout(I2S_SPK_BCLK_PIN, I2S_SPK_LRC_PIN, I2S_SPK_DIN_PIN);
  audio.setVolume(15);  // 0-21, 15 = ~70%

  Serial.println(" Speaker initialized on I2S_NUM_1");
}

// ===== STATE HANDLERS =====
void handleIdle() {
  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && !buttonWasPressed) {
    // Button just pressed
    delay(DEBOUNCE_MS);
    if (digitalRead(BUTTON_PIN) == LOW) {
      buttonWasPressed = true;
      buttonPressTime = millis();
      startRecording();
    }
  }
}

void handleRecording() {
  int buttonState = digitalRead(BUTTON_PIN);

  // Check if button released
  if (buttonState == HIGH && buttonWasPressed) {
    delay(DEBOUNCE_MS);
    if (digitalRead(BUTTON_PIN) == HIGH) {
      buttonWasPressed = false;
      unsigned long recordDuration = millis() - buttonPressTime;

      if (recordDuration >= MIN_RECORDING_TIME_MS) {
        stopRecording();
        processAudio();
      } else {
        Serial.println("Recording too short, ignored");
        stopRecording();
        currentState = IDLE;
        Serial.println("\nReady - Press button to speak");
      }
      return;
    }
  }

  // Check max recording time
  if (millis() - buttonPressTime >= MAX_RECORDING_TIME_MS) {
    Serial.println("Max recording time reached");
    buttonWasPressed = false;
    stopRecording();
    processAudio();
    return;
  }

  // Continue capturing audio
  captureAudioChunk();
}

void handleError() {
  if (millis() - errorStartTime > 3000) {
    // Recovery after 3 seconds
    Serial.println("Recovering...");

    currentError = ERROR_NONE;
    currentState = IDLE;

    Serial.println("\nRecovered - Ready - Press button to speak\n");
  }
}

// ===== RECORDING FUNCTIONS =====
void startRecording() {
  bufferWritePos = 0;
  memset(audioBuffer, 0, INPUT_BUFFER_SIZE);

  // Clear I2S buffer of stale data
  size_t bytes_read_dummy;
  i2s_channel_read(i2s_mic_handle, i2sSamples, sizeof(i2sSamples), &bytes_read_dummy, 0);

  currentState = RECORDING;
  Serial.println("\nRecording... (release button when done)");

  // === DIAGNOSTIC: Display raw audio samples ===
  size_t bytes_read = 0;
  i2s_channel_read(i2s_mic_handle, i2sSamples, sizeof(i2sSamples), &bytes_read, 100);

  Serial.println("\n=== AUDIO DIAGNOSTIC ===");
  Serial.printf("Bytes read: %d samples\n", bytes_read / 4);
  Serial.println("First 10 raw I2S samples:");
  for (int i = 0; i < 10; i++) {
    Serial.printf("[%d] Raw=0x%08X  >>8=%d  >>14=%d  >>16=%d\n",
                  i,
                  i2sSamples[i],
                  i2sSamples[i] >> 8,
                  i2sSamples[i] >> 14,
                  i2sSamples[i] >> 16);
  }
  Serial.println("========================\n");
}

void captureAudioChunk() {
  size_t bytes_read = 0;

  esp_err_t result = i2s_channel_read(
    i2s_mic_handle,
    i2sSamples,
    sizeof(i2sSamples),
    &bytes_read,
    100  // 100ms timeout
  );

  if (result != ESP_OK || bytes_read == 0) return;

  // In STEREO mode: LEFT sample, then RIGHT sample alternating
  // INMP441 L/R=GND means LEFT channel has data, RIGHT is noise/duplicate
  int samples_count = bytes_read / sizeof(int32_t);
  uint32_t maxSamples = INPUT_BUFFER_SIZE / sizeof(int16_t);

  // STEREO mode: Skip zeros (even samples), use only odd samples where INMP441 data is
  for (int i = 1; i < samples_count && bufferWritePos < maxSamples; i += 2) {
    // Based on diagnostics: odd samples have audio data, even samples are zeros
    // INMP441 outputs 24-bit in 32-bit frames
    // Need to shift by 16 total to properly convert 24-bit to 16-bit without overflow
    int16_t sample16 = (int16_t)(i2sSamples[i] >> 16);
    audioBuffer[bufferWritePos++] = sample16;
  }

  // === DIAGNOSTIC: Audio level monitoring ===
  static unsigned long lastMonitor = 0;
  if (millis() - lastMonitor >= 200 && bufferWritePos > 320) {
    lastMonitor = millis();

    // Calculate RMS and peak of last 160 samples (10ms at 16kHz)
    int64_t sum_squares = 0;
    int32_t peak = 0;
    for (int j = 0; j < 160; j++) {
      int16_t sample = audioBuffer[bufferWritePos - 1 - j];
      int32_t abs_val = abs(sample);
      if (abs_val > peak) peak = abs_val;
      sum_squares += (int64_t)sample * (int64_t)sample;
    }
    float rms = sqrt((float)sum_squares / 160.0);

    // Display audio level meter
    int bars = (int)(rms / 100.0);  // Scale for visibility
    if (bars > 40) bars = 40;
    Serial.print("Audio [");
    for (int b = 0; b < 40; b++) {
      Serial.print(b < bars ? "=" : " ");
    }
    Serial.printf("] RMS=%.0f Peak=%d\n", rms, peak);
  }
}

void stopRecording() {
  uint32_t samplesRecorded = bufferWritePos;
  float durationSeconds = (float)samplesRecorded / MIC_SAMPLE_RATE;

  Serial.print(" Recorded ");
  Serial.print(samplesRecorded);
  Serial.print(" samples (");
  Serial.print(durationSeconds, 2);
  Serial.println(" seconds)");
}

void processAudio() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    setError(ERROR_WIFI_DISCONNECTED, "WiFi disconnected");
    return;
  }

  // 1. Transcribe audio
  String transcription = transcribeAudio(audioBuffer, bufferWritePos);

  if (transcription.length() == 0) {
    setError(ERROR_WHISPER_FAILED, "Transcription failed");
    return;
  }

  // 2. Get AI response
  String aiResponse = getChatResponse(transcription);

  if (aiResponse.length() == 0) {
    setError(ERROR_CHAT_FAILED, "Chat API failed");
    return;
  }

  // 3. Speak response
  speakResponse(aiResponse);
}

// ===== API FUNCTIONS =====
String transcribeAudio(int16_t* samples, uint32_t sampleCount) {
  currentState = PROCESSING_WHISPER;
  Serial.println("Transcribing audio...");

  // Validate input
  if (sampleCount == 0) {
    Serial.println("Error: No audio samples to transcribe");
    return "";
  }

  if (sampleCount > (INPUT_BUFFER_SIZE / sizeof(int16_t))) {
    Serial.println("Error: Sample count exceeds buffer size");
    return "";
  }

  Serial.printf("Audio: %lu samples (%.2f seconds)\n",
                sampleCount,
                (float)sampleCount / MIC_SAMPLE_RATE);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(API_TIMEOUT_MS / 1000);  // Convert ms to seconds

  Serial.print("Connecting to api.openai.com:443...");
  if (!client.connect("api.openai.com", 443)) {
    Serial.println(" FAILED");
    Serial.printf("WiFi status: %d\n", WiFi.status());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    return "";
  }
  Serial.println(" Connected");

  // Generate WAV header
  WAVHeader header = createWAVHeader(sampleCount);
  uint32_t totalWAVSize = sizeof(WAVHeader) + (sampleCount * 2);

  // Multipart boundary
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

  // Build multipart body structure
  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  bodyStart += "Content-Type: audio/wav\r\n\r\n";

  String bodyMiddle = "\r\n--" + boundary + "\r\n";
  bodyMiddle += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
  bodyMiddle += WHISPER_MODEL;
  bodyMiddle += "\r\n--" + boundary + "\r\n";
  bodyMiddle += "Content-Disposition: form-data; name=\"language\"\r\n\r\n";
  bodyMiddle += WHISPER_LANGUAGE;
  bodyMiddle += "\r\n--" + boundary + "--\r\n";

  uint32_t contentLength = bodyStart.length() + totalWAVSize + bodyMiddle.length();

  // Send HTTP request
  client.print("POST /v1/audio/transcriptions HTTP/1.1\r\n");
  client.print("Host: api.openai.com\r\n");
  client.print("Authorization: Bearer ");
  client.print(OPENAI_API_KEY);
  client.print("\r\n");
  client.print("Content-Type: multipart/form-data; boundary=");
  client.print(boundary);
  client.print("\r\n");
  client.print("Content-Length: ");
  client.print(contentLength);
  client.print("\r\n\r\n");

  // Send body start
  client.print(bodyStart);

  // Send WAV header
  client.write((uint8_t*)&header, sizeof(WAVHeader));

  // Send audio data in chunks
  uint32_t bytesToSend = sampleCount * 2;
  uint32_t bytesSent = 0;
  const uint32_t CHUNK_SIZE = 2048;

  while (bytesSent < bytesToSend) {
    // Check if connection is still active
    if (!client.connected()) {
      Serial.println("Connection lost during upload!");
      return "";
    }

    uint32_t chunkSize = min(CHUNK_SIZE, bytesToSend - bytesSent);
    size_t written = client.write((uint8_t*)samples + bytesSent, chunkSize);

    if (written == 0) {
      Serial.println("Failed to write data!");
      return "";
    }

    bytesSent += written;

    // Progress update every 8KB
    if (bytesSent % 8192 == 0 || bytesSent == bytesToSend) {
      Serial.printf("Uploaded %lu/%lu bytes (%.0f%%)\n",
                    bytesSent, bytesToSend,
                    (float)bytesSent * 100.0 / bytesToSend);
    }
  }

  // Send body end
  client.print(bodyMiddle);
  Serial.println("Request sent, waiting for response...");

  // Read response
  String response = readHTTPResponse(client);
  client.stop();

  if (response.length() == 0) {
    Serial.println("Empty response from server");
    return "";
  }

  // Parse JSON response
  return parseWhisperResponse(response);
}

String getChatResponse(String userMessage) {
  currentState = PROCESSING_CHAT;
  Serial.println("Getting AI response...");

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  // Build API URL
  String url = "https://" + String(OPENAI_API_HOST) + CHAT_ENDPOINT;
  http.begin(client, url);

  // Set headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));

  // Build JSON request using ArduinoJson
  // Need larger document for nested structure
  StaticJsonDocument<2048> requestDoc;
  requestDoc["model"] = CHAT_MODEL;
  requestDoc["store"] = true;
  requestDoc["max_output_tokens"] = CHAT_MAX_OUTPUT_TOKENS;

  // Add previous_response_id if we have conversation context
  if (previousResponseId.length() > 0) {
    requestDoc["previous_response_id"] = previousResponseId;
  }

  // Build input array with system and user messages
  JsonArray input = requestDoc.createNestedArray("input");

  // System message
  JsonObject systemMsg = input.createNestedObject();
  systemMsg["role"] = "system";
  JsonArray systemContent = systemMsg.createNestedArray("content");
  JsonObject systemText = systemContent.createNestedObject();
  systemText["type"] = "input_text";
  systemText["text"] = CHAT_INSTRUCTIONS;

  // User message
  JsonObject userMsg = input.createNestedObject();
  userMsg["role"] = "user";
  JsonArray userContent = userMsg.createNestedArray("content");
  JsonObject userText = userContent.createNestedObject();
  userText["type"] = "input_text";
  userText["text"] = userMessage;

  // Serialize JSON to string
  String payload;
  serializeJson(requestDoc, payload);

  Serial.print("Request payload: ");
  Serial.println(payload);

  // Send POST request
  int httpCode = http.POST(payload);

  if (httpCode != HTTP_CODE_OK && httpCode != 200) {
    Serial.printf("HTTP Error: %d\n", httpCode);
    http.end();
    return "";
  }

  // Get response
  String response = http.getString();
  http.end();

  Serial.print("Response: ");
  Serial.println(response);

  // Parse JSON response using ArduinoJson
  StaticJsonDocument<4096> responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return "";
  }

  // Extract response ID for context
  if (responseDoc.containsKey("id")) {
    previousResponseId = responseDoc["id"].as<String>();
  }

  // Extract output text from: output[0].content[0].text
  if (!responseDoc.containsKey("output")) {
    Serial.println("No output field in response");
    return "";
  }

  JsonArray output = responseDoc["output"];
  if (output.size() == 0) {
    Serial.println("Empty output array");
    return "";
  }

  JsonObject firstOutput = output[0];
  if (!firstOutput.containsKey("content")) {
    Serial.println("No content in first output");
    return "";
  }

  JsonArray content = firstOutput["content"];
  if (content.size() == 0) {
    Serial.println("Empty content array");
    return "";
  }

  JsonObject firstContent = content[0];
  if (!firstContent.containsKey("text")) {
    Serial.println("No text in first content");
    return "";
  }

  String aiResponse = firstContent["text"].as<String>();

  Serial.print("AI: \"");
  Serial.print(aiResponse);
  Serial.println("\"");

  return aiResponse;
}

void speakResponse(String text) {
  Serial.println("Preparing to speak...");

  currentState = SPEAKING;
  Serial.print("Speaking: ");
  Serial.println(text);

  // Use Audio library's TTS streaming
  bool success = audio.openai_speech(
    OPENAI_API_KEY,
    TTS_MODEL,
    text.c_str(),
    "",
    TTS_VOICE,
    "mp3",
    String(TTS_SPEED)
  );

  if (!success) {
    Serial.println("TTS request failed!");
    setError(ERROR_TTS_FAILED, "TTS failed");
    return;
  }

  Serial.println("Audio streaming...");
}

// ===== AUDIO PROCESSING =====
WAVHeader createWAVHeader(uint32_t sampleCount) {
  WAVHeader header;
  header.dataSize = sampleCount * 2;  // 2 bytes per sample
  header.fileSize = header.dataSize + 36;  // 36 = header size - 8
  return header;
}

String parseWhisperResponse(String response) {
  // Find JSON body (after HTTP headers)
  int bodyStart = response.indexOf("\r\n\r\n");
  if (bodyStart == -1) {
    Serial.println("Invalid response format");
    return "";
  }

  String jsonBody = response.substring(bodyStart + 4);

  // Parse JSON with ArduinoJson
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonBody);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    Serial.println("Response body:");
    Serial.println(jsonBody.substring(0, 200));
    return "";
  }

  // Extract text field
  if (!doc.containsKey("text")) {
    Serial.println("No text field in response");
    return "";
  }

  String transcription = doc["text"].as<String>();

  Serial.print("Transcription: \"");
  Serial.print(transcription);
  Serial.println("\"");

  return transcription;
}

// ===== NETWORK FUNCTIONS =====
String readHTTPResponse(WiFiClientSecure& client) {
  unsigned long timeout = millis() + API_TIMEOUT_MS;
  unsigned long startTime = millis();
  String response = "";
  int bytesReceived = 0;

  Serial.print("Reading response");

  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      char c = client.read();
      response += c;
      bytesReceived++;

      // Print progress every 1000 bytes
      if (bytesReceived % 1000 == 0) {
        Serial.print(".");
      }
    }

    // Check if we've received enough data (simple heuristic)
    if (response.length() > 100 && response.indexOf("\r\n\r\n") != -1) {
      // Wait a bit more for body
      delay(100);
      while (client.available()) {
        response += (char)client.read();
        bytesReceived++;
      }
      break;
    }

    delay(1);
  }

  Serial.println();
  Serial.printf("Received %d bytes in %lu ms\n", bytesReceived, millis() - startTime);

  if (millis() >= timeout) {
    Serial.println("API timeout - no response received");
    Serial.printf("Connection still active: %d\n", client.connected());
    return "";
  }

  // Check HTTP status code
  if (response.indexOf("HTTP/1.1 200") == -1) {
    Serial.println("Non-200 response:");
    Serial.println(response.substring(0, 500));
    return "";
  }

  Serial.println("HTTP 200 OK received");
  return response;
}

String escapeJSON(String str) {
  String result = str;
  result.replace("\\", "\\\\");
  result.replace("\"", "\\\"");
  result.replace("\n", "\\n");
  result.replace("\r", "");
  result.replace("\t", "\\t");
  return result;
}

// ===== UTILITY FUNCTIONS =====
void setError(ErrorType error, const char* message) {
  currentError = error;
  errorStartTime = millis();
  currentState = ERROR_STATE;

  Serial.print("L ERROR: ");
  Serial.println(message);
}

void onSpeakingComplete() {
  Serial.println("Audio playback finished");

  currentState = IDLE;
  Serial.println("\nReady - Press button to speak\n");
}

// ===== OPTIONAL AUDIO LIBRARY CALLBACKS =====
void audio_info(const char *info) {
  Serial.print("Audio info: ");
  Serial.println(info);
}

void audio_eof_mp3(const char *info) {
  Serial.print("End of file: ");
  Serial.println(info);
}
