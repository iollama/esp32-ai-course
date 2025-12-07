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
#include <time.h>
#include <driver/i2s.h>
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

// Audio library
Audio audio;

// Button state
unsigned long buttonPressTime = 0;
bool buttonWasPressed = false;

// I2S driver state
bool micDriverInstalled = false;

// Conversation memory
String previousAssistantResponse = "";
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
void deinitMicrophone();
void initSpeaker();
void deinitSpeaker();

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
String parseChatResponse(String response);

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
  Serial.println(" Button initialized (GPIO 10)");

  // Initialize speaker shutdown pin (start disabled)
  pinMode(I2S_SPK_SD_PIN, OUTPUT);
  digitalWrite(I2S_SPK_SD_PIN, LOW);
  Serial.println(" Speaker amplifier initialized");

  // Connect to WiFi
  connectWiFi();

  // Sync time (critical for HTTPS)
  syncTime();

  // Allocate PSRAM buffers
  allocatePSRAMBuffers();

  // Initialize microphone
  initMicrophone();

  // Set initial state
  currentState = IDLE;

  Serial.println("\n=================================");
  Serial.println(" Ready - Press button to speak");
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
    Serial.println("\n WiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n WiFi connection failed!");
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
    Serial.println("\n Time synced");
    Serial.print("Current time: ");
    Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
  } else {
    Serial.println("\n Time sync failed!");
    Serial.println("TTS may not work without proper time sync");
  }
}

void allocatePSRAMBuffers() {
  Serial.println("Allocating PSRAM buffers...");

  audioBuffer = (int16_t*)ps_malloc(INPUT_BUFFER_SIZE);
  if (!audioBuffer) {
    Serial.println(" Failed to allocate audio buffer");
    while(1) delay(1000);
  }

  Serial.println(" PSRAM buffers allocated");
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("Free PSRAM: ");
  Serial.println(ESP.getFreePsram());
}

// ===== I2S LIFECYCLE FUNCTIONS =====
void initMicrophone() {
  if (micDriverInstalled) return;

  Serial.println("Initializing microphone...");

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = MIC_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUFFER_COUNT,
    .dma_buf_len = DMA_BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_MIC_SCK_PIN,
    .ws_io_num = I2S_MIC_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD_PIN
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf(" Failed to install I2S driver: %d\n", err);
    while (1) delay(1000);
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf(" Failed to set I2S pins: %d\n", err);
    while (1) delay(1000);
  }

  i2s_start(I2S_PORT);

  micDriverInstalled = true;
  Serial.println(" Microphone driver installed");
}

void deinitMicrophone() {
  if (!micDriverInstalled) return;

  i2s_stop(I2S_PORT);
  i2s_driver_uninstall(I2S_PORT);
  micDriverInstalled = false;

  Serial.println(" Microphone driver uninstalled");
  delay(100);
}

void initSpeaker() {
  Serial.println("Initializing speaker...");

  // Enable amplifier
  digitalWrite(I2S_SPK_SD_PIN, HIGH);
  delay(50);

  // Audio library will claim I2S_NUM_0
  audio.setPinout(I2S_SPK_BCLK_PIN, I2S_SPK_LRC_PIN, I2S_SPK_DIN_PIN);
  audio.setVolume(15);  // 0-21, 15 = ~70%

  Serial.println(" Speaker initialized");
}

void deinitSpeaker() {
  // Disable amplifier
  digitalWrite(I2S_SPK_SD_PIN, LOW);
  delay(100);

  Serial.println(" Speaker disabled");
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
    Serial.println("{ Recovering...");

    currentError = ERROR_NONE;

    // Ensure mic driver is active
    if (!micDriverInstalled) {
      deinitSpeaker();
      delay(200);
      initMicrophone();
    }

    currentState = IDLE;
    Serial.println("\n Recovered - Ready - Press button to speak\n");
  }
}

// ===== RECORDING FUNCTIONS =====
void startRecording() {
  bufferWritePos = 0;
  memset(audioBuffer, 0, INPUT_BUFFER_SIZE);

  // Clear I2S buffer of stale data
  size_t bytes_read;
  i2s_read(I2S_PORT, i2sSamples, sizeof(i2sSamples), &bytes_read, 0);

  currentState = RECORDING;
  Serial.println("\n<� Recording... (release button when done)");
}

void captureAudioChunk() {
  size_t bytes_read = 0;

  esp_err_t result = i2s_read(
    I2S_PORT,
    i2sSamples,
    sizeof(i2sSamples),
    &bytes_read,
    100  // 100ms timeout
  );

  if (result != ESP_OK || bytes_read == 0) return;

  int samples_count = bytes_read / sizeof(int32_t);
  uint32_t maxSamples = INPUT_BUFFER_SIZE / sizeof(int16_t);

  for (int i = 0; i < samples_count && bufferWritePos < maxSamples; i++) {
    // CRITICAL: INMP441 outputs 24-bit in 32-bit frames
    // Shift right by 16 to get 16-bit sample
    int16_t sample16 = (int16_t)(i2sSamples[i] >> 16);
    audioBuffer[bufferWritePos++] = sample16;
  }
}

void stopRecording() {
  uint32_t samplesRecorded = bufferWritePos;
  float durationSeconds = (float)samplesRecorded / MIC_SAMPLE_RATE;

  Serial.print(" Recorded ");
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
  Serial.println("= Transcribing audio...");

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.openai.com", 443)) {
    Serial.println(" Connection failed");
    return "";
  }

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
    uint32_t chunkSize = min(CHUNK_SIZE, bytesToSend - bytesSent);
    client.write((uint8_t*)samples + bytesSent, chunkSize);
    bytesSent += chunkSize;
  }

  // Send body end
  client.print(bodyMiddle);

  // Read response
  String response = readHTTPResponse(client);
  client.stop();

  // Parse JSON response
  return parseWhisperResponse(response);
}

String getChatResponse(String userMessage) {
  currentState = PROCESSING_CHAT;
  Serial.println("> Getting AI response...");

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.openai.com", 443)) {
    Serial.println(" Connection failed");
    return "";
  }

  // Build JSON payload with conversation context
  String payload = "{";
  payload += "\"model\":\"" + String(CHAT_MODEL) + "\",";
  payload += "\"messages\":[";

  // System message
  payload += "{\"role\":\"system\",\"content\":\"You are a helpful voice assistant. Keep responses concise and natural for speech.\"}";

  // Previous assistant response for context (if exists)
  if (previousAssistantResponse.length() > 0) {
    payload += ",{\"role\":\"assistant\",\"content\":\"";
    payload += escapeJSON(previousAssistantResponse);
    payload += "\"}";
  }

  // Current user message
  payload += ",{\"role\":\"user\",\"content\":\"";
  payload += escapeJSON(userMessage);
  payload += "\"}";

  payload += "],";
  payload += "\"temperature\":" + String(CHAT_TEMPERATURE) + ",";
  payload += "\"max_tokens\":" + String(CHAT_MAX_TOKENS);
  payload += "}";

  // Send HTTP request
  client.print("POST /v1/chat/completions HTTP/1.1\r\n");
  client.print("Host: api.openai.com\r\n");
  client.print("Authorization: Bearer ");
  client.print(OPENAI_API_KEY);
  client.print("\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: ");
  client.print(payload.length());
  client.print("\r\n\r\n");
  client.print(payload);

  // Read response
  String response = readHTTPResponse(client);
  client.stop();

  // Parse and store response
  String aiResponse = parseChatResponse(response);

  if (aiResponse.length() > 0) {
    previousAssistantResponse = aiResponse;  // Store for next turn
    lastInteractionTime = millis();
  }

  return aiResponse;
}

void speakResponse(String text) {
  // Prepare for speaker (uninstall mic driver)
  Serial.println("=
 Preparing to speak...");

  deinitMicrophone();
  delay(200);  // CRITICAL: Let hardware settle

  initSpeaker();

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
    Serial.println(" TTS request failed!");
    setError(ERROR_TTS_FAILED, "TTS failed");

    // Recovery: reinstall mic driver
    deinitSpeaker();
    delay(200);
    initMicrophone();
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
  // Find JSON body (after headers)
  int bodyStart = response.indexOf("\r\n\r\n");
  if (bodyStart == -1) {
    Serial.println(" Invalid response format");
    return "";
  }

  String jsonBody = response.substring(bodyStart + 4);

  // Simple JSON parsing for "text" field
  int textStart = jsonBody.indexOf("\"text\"");
  if (textStart == -1) {
    Serial.println(" No text field in response");
    return "";
  }

  int quoteStart = jsonBody.indexOf("\"", textStart + 7);
  int quoteEnd = jsonBody.indexOf("\"", quoteStart + 1);

  if (quoteStart == -1 || quoteEnd == -1) {
    Serial.println(" Failed to parse text field");
    return "";
  }

  String transcription = jsonBody.substring(quoteStart + 1, quoteEnd);

  Serial.print("=� Transcription: \"");
  Serial.print(transcription);
  Serial.println("\"");

  return transcription;
}

String parseChatResponse(String response) {
  int bodyStart = response.indexOf("\r\n\r\n");
  if (bodyStart == -1) {
    Serial.println(" Invalid response format");
    return "";
  }

  String jsonBody = response.substring(bodyStart + 4);

  // Find content field in choices[0].message.content
  int contentStart = jsonBody.indexOf("\"content\"");
  if (contentStart == -1) {
    Serial.println(" No content field in response");
    return "";
  }

  int quoteStart = jsonBody.indexOf("\"", contentStart + 10);
  int quoteEnd = -1;

  // Find matching quote (handle escaped quotes)
  for (int i = quoteStart + 1; i < jsonBody.length(); i++) {
    if (jsonBody[i] == '\"' && jsonBody[i-1] != '\\') {
      quoteEnd = i;
      break;
    }
  }

  if (quoteStart == -1 || quoteEnd == -1) {
    Serial.println(" Failed to parse content field");
    return "";
  }

  String content = jsonBody.substring(quoteStart + 1, quoteEnd);

  // Unescape JSON
  content.replace("\\n", " ");
  content.replace("\\\"", "\"");
  content.replace("\\\\", "\\");

  Serial.print("=� AI: \"");
  Serial.print(content);
  Serial.println("\"");

  return content;
}

// ===== NETWORK FUNCTIONS =====
String readHTTPResponse(WiFiClientSecure& client) {
  unsigned long timeout = millis() + API_TIMEOUT_MS;
  String response = "";

  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      char c = client.read();
      response += c;
    }

    // Check if we've received enough data (simple heuristic)
    if (response.length() > 100 && response.indexOf("\r\n\r\n") != -1) {
      // Wait a bit more for body
      delay(100);
      while (client.available()) {
        response += (char)client.read();
      }
      break;
    }

    delay(1);
  }

  if (millis() >= timeout) {
    Serial.println("� API timeout");
    return "";
  }

  // Check HTTP status code
  if (response.indexOf("HTTP/1.1 200") == -1) {
    Serial.println("� Non-200 response:");
    Serial.println(response.substring(0, 300));
    return "";
  }

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
  Serial.println(" Audio playback finished");

  // Cleanup speaker and restore microphone
  deinitSpeaker();
  delay(200);  // CRITICAL: Let hardware settle

  initMicrophone();

  currentState = IDLE;
  Serial.println("\n=================================");
  Serial.println("Ready - Press button to speak");
  Serial.println("=================================\n");
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
