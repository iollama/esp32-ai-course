//TODO: move to responses API and save last respose
//TODO: Add LED status
//TODO: start with sofAP
//TODO: create a configuration screen for system prompt + show last 3 messages
/*
  Voice Assistant (ESP32-S3)
  -------------------------

  REQUIRED LIBRARIES
  ------------------
  This sketch requires ArduinoJson.
  Install via Arduino IDE:
    Sketch -> Include Library -> Manage Libraries...
    Search for "ArduinoJson" by Benoit Blanchon
    Install version 6.x or newer

  Pipeline:
    1) Long-press button to trigger recording
    2) Record audio from INMP441 over I2S (RX)
    3) Send WAV to OpenAI transcription (STT), response_format = text
    4) Send transcript to OpenAI chat completion (HTTPClient POST + ArduinoJson)
    5) Send answer to OpenAI TTS (response_format = pcm),
       download into fixed PSRAM buffer, then play over I2S (TX)

  Pin configuration
  -----------------

  INMP441 Microphone (I2S RX)
    - SD   -> GPIO 40
    - VDD  -> 3V
    - GND  -> GND
    - L/R  -> GND (left channel)
    - WS   -> GPIO 41
    - SCK  -> GPIO 42

  MAX98357A Amplifier (I2S TX)
    - VIN  -> 5V
    - GND  -> GND
    - SD   -> 3V
    - GAIN -> not connected
    - DIN  -> GPIO 17
    - BCLK -> GPIO 47
    - LRC  -> GPIO 21

  Button
    - + -> GPIO 1
    - - -> GND

  Status LED
    - GPIO 39
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>

#include "config.h"
#include "esp32-hal-psram.h"
#include "driver/i2s.h"

// ------------------------------
// SERIAL OUTPUT CONTROL
// ------------------------------
#define SERIAL_ON true
#define PRINT(x)       do { if (SERIAL_ON) Serial.print(x); } while (0)
#define PRINTLN(x)     do { if (SERIAL_ON) Serial.println(x); } while (0)
#define PRINTF(...)    do { if (SERIAL_ON) Serial.printf(__VA_ARGS__); } while (0)

// ------------------------------
// GENERAL PINS
// ------------------------------
const int LED_PIN = 39;

// ------------------------------
// BUTTON SETTINGS
// ------------------------------
const int BUTTON_PIN = 1;
const unsigned long LONG_PRESS_TIME = 200;  // ms

bool buttonPressed = false;
unsigned long pressStart = 0;

// ------------------------------
// NTP
// ------------------------------
const char* ntpServer = "pool.ntp.org";

// ------------------------------
// AUDIO SETTINGS (MIC)
// ------------------------------
#define SAMPLE_RATE_IN 16000
#define RECORD_SECONDS 3
#define NUM_SAMPLES (SAMPLE_RATE_IN * RECORD_SECONDS)
#define I2S_CHUNK_SAMPLES 1024

// INMP441 I2S pinout (ESP32-S3)
static const gpio_num_t I2S_SCK_IN = GPIO_NUM_42;  // BCLK
static const gpio_num_t I2S_WS_IN  = GPIO_NUM_41;  // LRCLK
static const gpio_num_t I2S_SD_IN  = GPIO_NUM_40;  // DOUT from mic

// ------------------------------
// AUDIO SETTINGS (SPEAKER / TTS)
// ------------------------------
// Playback configuration for PCM output.
// We assume the returned PCM is 16-bit mono little-endian.
#define SAMPLE_RATE_OUT 24000

// MAX98357A I2S TX pins
static const gpio_num_t I2S_BCLK_OUT = GPIO_NUM_47;
static const gpio_num_t I2S_LRCLK_OUT = GPIO_NUM_21;
static const gpio_num_t I2S_DOUT_OUT = GPIO_NUM_17;

// ------------------------------
// TTS FULL-BUFFER SETTINGS (PSRAM)
// ------------------------------
#define MAX_TTS_SECONDS 15
#define TTS_BYTES_PER_SEC (SAMPLE_RATE_OUT * 2)   // 16-bit mono = 2 bytes/sample
#define MAX_TTS_BYTES (MAX_TTS_SECONDS * TTS_BYTES_PER_SEC)

uint8_t* tts_pcm_buf = nullptr;
size_t   tts_pcm_len = 0;

// ------------------------------
// GLOBAL AUDIO BUFFERS (PSRAM)
// ------------------------------
int32_t* raw_buf = nullptr;  // 32-bit I2S mic frames
int16_t* pcm_buf = nullptr;  // 16-bit PCM samples for WAV upload

// ------------------------------
// Small helper: inline NOP for short press
// ------------------------------
static inline void do_nop() {
  __asm__ __volatile__("nop" ::: "memory");
}

// =====================================================================
// Helper: JSON escape for TTS "input" (kept small and deterministic)
// =====================================================================
String jsonEscape(const String& s) {
  String out = "\"";
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          sprintf(buf, "\\u%04x", (uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += "\"";
  return out;
}

// =====================================================================
// Helper: Parse and print OpenAI-style JSON error body if present
// =====================================================================
void print_openai_json_error_if_any(const String& body, const char* tag) {
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    PRINTF("%s: non-JSON error body (or parse failed)\n", tag);
    return;
  }

  const char* msg  = doc["error"]["message"] | "";
  const char* type = doc["error"]["type"] | "";
  const char* code = doc["error"]["code"] | "";

  PRINTF("%s: error.message: %s\n", tag, msg);
  if (type[0]) PRINTF("%s: error.type: %s\n", tag, type);
  if (code[0]) PRINTF("%s: error.code: %s\n", tag, code);
}

// =====================================================================
// LEGACY I2S TX CONFIG for MAX98357A (speaker) on I2S_NUM_1
// =====================================================================
void init_i2s_speaker_legacy(uint32_t sampleRate) {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = (int)sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK_OUT,
    .ws_io_num = I2S_LRCLK_OUT,
    .data_out_num = I2S_DOUT_OUT,
    .data_in_num = -1
  };

  i2s_driver_install(I2S_NUM_1, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pins);
  i2s_zero_dma_buffer(I2S_NUM_1);

  PRINTF("I2S speaker ready @ %u Hz\n", (unsigned)sampleRate);
}

// =====================================================================
// WAV HEADER (16-bit mono helpers) for STT upload
// =====================================================================
void write_u32_le(uint8_t* buf, uint32_t v) {
  buf[0] = v & 0xff;
  buf[1] = (v >> 8) & 0xff;
  buf[2] = (v >> 16) & 0xff;
  buf[3] = (v >> 24) & 0xff;
}

void write_u16_le(uint8_t* buf, uint16_t v) {
  buf[0] = v & 0xff;
  buf[1] = (v >> 8) & 0xff;
}

void create_wav_header(uint8_t* h, uint32_t data_size, uint32_t sample_rate) {
  memset(h, 0, 44);

  memcpy(h, "RIFF", 4);
  write_u32_le(h + 4, 36 + data_size);
  memcpy(h + 8, "WAVE", 4);

  memcpy(h + 12, "fmt ", 4);
  write_u32_le(h + 16, 16);
  write_u16_le(h + 20, 1);   // PCM
  write_u16_le(h + 22, 1);   // mono
  write_u32_le(h + 24, sample_rate);
  write_u32_le(h + 28, sample_rate * 2);
  write_u16_le(h + 32, 2);
  write_u16_le(h + 34, 16);

  memcpy(h + 36, "data", 4);
  write_u32_le(h + 40, data_size);
}

// =====================================================================
// I2S MIC SETUP (RX) for INMP441 using LEGACY DRIVER (I2S_NUM_0)
// =====================================================================
void setup_i2s_mic() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE_IN,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK_IN,
    .ws_io_num = I2S_WS_IN,
    .data_out_num = -1,
    .data_in_num = I2S_SD_IN
  };

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  if (err != ESP_OK) {
    PRINTF("I2S mic: driver_install failed: %d\n", err);
    return;
  }

  err = i2s_set_pin(I2S_NUM_0, &pins);
  if (err != ESP_OK) {
    PRINTF("I2S mic: set_pin failed: %d\n", err);
    return;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  PRINTLN("I2S microphone ready (legacy, INMP441).");
}

// =====================================================================
// I2S SPEAKER SETUP (TX) for MAX98357A (legacy driver on I2S_NUM_1)
// =====================================================================
void setup_i2s_speaker() {
  init_i2s_speaker_legacy(SAMPLE_RATE_OUT);
}

// =====================================================================
// RECORD FROM MIC INTO pcm_buf (16-bit PCM)
// =====================================================================
void record_audio() {
  PRINTLN("\nRecording 3 seconds...");

  int samples_collected = 0;

  while (samples_collected < NUM_SAMPLES) {
    int samples_to_read = NUM_SAMPLES - samples_collected;
    if (samples_to_read > I2S_CHUNK_SAMPLES) samples_to_read = I2S_CHUNK_SAMPLES;

    size_t bytes_to_read = (size_t)samples_to_read * sizeof(int32_t);
    size_t bytes_read = 0;

    esp_err_t err = i2s_read(I2S_NUM_0,
                             &raw_buf[samples_collected],
                             bytes_to_read,
                             &bytes_read,
                             portMAX_DELAY);

    if (err != ESP_OK) {
      PRINT("I2S read error: ");
      PRINTLN((int)err);
      break;
    }

    int got_samples = (int)(bytes_read / sizeof(int32_t));
    samples_collected += got_samples;
    yield();
  }

  PRINT("Collected samples: ");
  PRINTLN(samples_collected);

  if (samples_collected < NUM_SAMPLES) {
    PRINTLN("Warning: not all samples collected, filling rest with zeros.");
    for (int i = samples_collected; i < NUM_SAMPLES; i++) raw_buf[i] = 0;
  }

  // Convert 32-bit mic frames to 16-bit PCM (simple shift)
  for (int i = 0; i < NUM_SAMPLES; i++) {
    pcm_buf[i] = (int16_t)(raw_buf[i] >> 16);
  }
}

// =====================================================================
// STT: Upload WAV; request response_format=text; return plain transcript
// =====================================================================
String openai_transcribe() {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.openai.com", 443)) {
    PRINTLN("STT: HTTPS connection failed");
    return "";
  }

  PRINTLN("STT: Connected to OpenAI.");

  String boundary = "----ESP32BOUNDARY";

  String part_file_header =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
    "Content-Type: audio/wav\r\n\r\n";

  String part_model =
    "\r\n--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
    "gpt-4o-mini-transcribe\r\n";

  String part_respfmt =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"response_format\"\r\n\r\n"
    "text\r\n";

  String part_end =
    "--" + boundary + "--\r\n";

  uint8_t wav_header[44];
  create_wav_header(wav_header, (uint32_t)(NUM_SAMPLES * 2), SAMPLE_RATE_IN);

  const int wav_header_size = 44;
  const int pcm_size = NUM_SAMPLES * 2;

  int content_len =
    (int)part_file_header.length() +
    wav_header_size +
    pcm_size +
    (int)part_model.length() +
    (int)part_respfmt.length() +
    (int)part_end.length();

  client.print("POST /v1/audio/transcriptions HTTP/1.1\r\n");
  client.print("Host: api.openai.com\r\n");
  client.print("Authorization: Bearer " + String(OPENAI_API_KEY) + "\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(content_len) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  client.write((const uint8_t*)part_file_header.c_str(), part_file_header.length());
  client.write(wav_header, wav_header_size);

  const uint8_t* p = (const uint8_t*)pcm_buf;
  int remaining = pcm_size;
  while (remaining > 0) {
    int chunk = (remaining > 1024) ? 1024 : remaining;
    client.write(p, chunk);
    p += chunk;
    remaining -= chunk;
  }

  client.write((const uint8_t*)part_model.c_str(), part_model.length());
  client.write((const uint8_t*)part_respfmt.c_str(), part_respfmt.length());
  client.write((const uint8_t*)part_end.c_str(), part_end.length());

  PRINTLN("STT: Sent. Reading reply...");

  // Read status line
  String statusLine;
  while (client.connected()) {
    statusLine = client.readStringUntil('\n');
    statusLine.trim();
    if (statusLine.length() == 0) continue;
    if (statusLine.startsWith("HTTP/")) break;
  }
  PRINT("STT: Status: ");
  PRINTLN(statusLine);

  int httpStatus = -1;
  int sp1 = statusLine.indexOf(' ');
  if (sp1 >= 0) {
    int sp2 = statusLine.indexOf(' ', sp1 + 1);
    if (sp2 >= 0) httpStatus = statusLine.substring(sp1 + 1, sp2).toInt();
    else          httpStatus = statusLine.substring(sp1 + 1).toInt();
  }

  // Read headers (discard)
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "") break;
  }

  // Read body (text transcript on success; JSON on error sometimes)
  String body;
  while (client.connected() || client.available()) {
    while (client.available()) body += (char)client.read();
    delay(1);
  }

  if (httpStatus != 200) {
    PRINTF("STT: Non-200 (%d)\n", httpStatus);
    print_openai_json_error_if_any(body, "STT");
    PRINTLN("STT raw body:");
    PRINTLN(body);
    return "";
  }

  body.trim();
  PRINT("Transcription: ");
  PRINTLN(body);
  return body;
}

// =====================================================================
// Chat completion -> short spoken-style answer (HTTPClient + ArduinoJson)
// =====================================================================
String openai_answer(const String& question) {
  PRINTLN("Chat: POST /v1/chat/completions");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  // Build request JSON with ArduinoJson
  StaticJsonDocument<768> req;
  req["model"] = "gpt-4o-mini";

  JsonArray messages = req.createNestedArray("messages");

  JsonObject sys = messages.createNestedObject();
  sys["role"] = "system";
  sys["content"] = "You are a helpful assistant. Answer in at most 2 short sentences suitable to be spoken aloud.";

  JsonObject usr = messages.createNestedObject();
  usr["role"] = "user";
  usr["content"] = question;

  String body;
  serializeJson(req, body);

  if (!http.begin(client, "https://api.openai.com/v1/chat/completions")) {
    PRINTLN("Chat: http.begin failed");
    http.end();
    return "";
  }

  http.setTimeout(15000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));

  int httpCode = http.POST((uint8_t*)body.c_str(), body.length());
  String reply = http.getString();
  http.end();

  PRINTF("Chat: HTTP status = %d\n", httpCode);
  PRINTLN("=== RAW CHAT REPLY ===");
  PRINTLN(reply);

  if (httpCode != 200) {
    print_openai_json_error_if_any(reply, "Chat");
    return "";
  }

  // Parse response JSON with ArduinoJson
  StaticJsonDocument<4096> resp;
  DeserializationError err = deserializeJson(resp, reply);
  if (err) {
    PRINT("Chat: JSON parse failed: ");
    PRINTLN(err.c_str());
    return "";
  }

  const char* content = resp["choices"][0]["message"]["content"] | "";
  if (content[0] == '\0') {
    PRINTLN("Chat: empty content");
    return "";
  }

  PRINT("Assistant answer: ");
  PRINTLN(content);

  return String(content);
}

// =====================================================================
// TTS (PCM): Download full PCM into PSRAM buffer, then play
// =====================================================================
void openai_tts_play(const String& text) {

  // Reset speaker DMA
  i2s_stop(I2S_NUM_1);
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_start(I2S_NUM_1);

  if (!tts_pcm_buf) {
    PRINTLN("TTS: PSRAM buffer not allocated");
    return;
  }
  tts_pcm_len = 0;

  WiFiClientSecure client;
  client.setInsecure();

  PRINTF("TTS: Free heap before connect: %u\n", (unsigned)ESP.getFreeHeap());
  PRINTLN("TTS: Connecting to OpenAI...");
  if (!client.connect("api.openai.com", 443)) {
    PRINTLN("TTS: HTTPS connection failed");
    return;
  }
  PRINTLN("TTS: Connected.");
  PRINTF("TTS: Free heap after connect: %u\n", (unsigned)ESP.getFreeHeap());

  // JSON body: request raw PCM output
  String body = "{";
  body += "\"model\":\"gpt-4o-mini-tts\",";
  body += "\"voice\":\"alloy\",";
  body += "\"response_format\":\"pcm\",";
  body += "\"input\":" + jsonEscape(text);
  body += "}";

  client.print("POST /v1/audio/speech HTTP/1.1\r\n");
  client.print("Host: api.openai.com\r\n");
  client.print("Authorization: Bearer " + String(OPENAI_API_KEY) + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: " + String(body.length()) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  // Read status line
  PRINTLN("TTS: Reading status line...");
  String statusLine;
  while (client.connected()) {
    statusLine = client.readStringUntil('\n');
    statusLine.trim();
    if (statusLine.length() == 0) continue;
    if (statusLine.startsWith("HTTP/")) break;
  }
  PRINT("TTS: Status: ");
  PRINTLN(statusLine);

  int httpStatus = -1;
  int sp1 = statusLine.indexOf(' ');
  if (sp1 >= 0) {
    int sp2 = statusLine.indexOf(' ', sp1 + 1);
    if (sp2 >= 0) httpStatus = statusLine.substring(sp1 + 1, sp2).toInt();
    else          httpStatus = statusLine.substring(sp1 + 1).toInt();
  }

  // Read headers (print for debug)
  PRINTLN("TTS: Reading headers...");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "") break;
    line.trim();
    PRINT("TTS HDR: ");
    PRINTLN(line);
  }

  if (httpStatus != 200) {
    PRINTF("TTS: Non-200 (%d). Reading body as text...\n", httpStatus);
    String errBody;
    unsigned long t0 = millis();
    while ((client.connected() || client.available()) && (millis() - t0 < 7000)) {
      while (client.available()) errBody += (char)client.read();
      delay(1);
    }
    print_openai_json_error_if_any(errBody, "TTS");
    PRINTLN("TTS raw body:");
    PRINTLN(errBody);
    return;
  }

  PRINTLN("TTS: Downloading and buffering PCM...");

  // Decode chunked transfer encoding
  while (client.connected() || client.available()) {

    String sizeLine;
    do {
      sizeLine = client.readStringUntil('\n');
      sizeLine.trim();
    } while (sizeLine.length() == 0 && (client.connected() || client.available()));

    if (sizeLine.length() == 0) break;

    int chunkSize = strtol(sizeLine.c_str(), NULL, 16);
    if (chunkSize <= 0) break;

    int remaining = chunkSize;
    while (remaining > 0) {
      uint8_t buf[1024];
      int toRead = min(remaining, (int)sizeof(buf));
      int len = client.read(buf, toRead);
      if (len <= 0) continue;
      remaining -= len;

      size_t space = (tts_pcm_len < MAX_TTS_BYTES) ? (MAX_TTS_BYTES - tts_pcm_len) : 0;
      if (space == 0) continue;

      size_t copyLen = ((size_t)len <= space) ? (size_t)len : space;
      memcpy(tts_pcm_buf + tts_pcm_len, buf, copyLen);
      tts_pcm_len += copyLen;
    }

    // Consume CRLF after chunk
    client.readStringUntil('\n');

    if (tts_pcm_len >= MAX_TTS_BYTES) break;
  }

  PRINTF("TTS: Buffered %u bytes (max %u). Playing...\n",
         (unsigned)tts_pcm_len, (unsigned)MAX_TTS_BYTES);

  // Play buffered PCM to I2S
  size_t played = 0;
  while (played < tts_pcm_len) {
    size_t toWrite = tts_pcm_len - played;
    if (toWrite > 2048) toWrite = 2048;

    size_t written = 0;
    i2s_write(I2S_NUM_1, tts_pcm_buf + played, toWrite, &written, portMAX_DELAY);
    played += written;

    yield();
  }

  PRINTLN("TTS: Playback complete.");
}

// =====================================================================
// WiFi + NTP init
// =====================================================================
void init_wifi_and_time() {
  PRINTLN("=== WiFi INIT ===");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(300);

  PRINT("Connecting to: ");
  PRINTLN(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    PRINT(".");
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    PRINTLN("\nWiFi FAILED on first attempt. Retrying...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      PRINT("+");
      delay(500);
    }
  }

  PRINTLN("\nWiFi OK!");
  PRINT("IP: ");
  PRINTLN(WiFi.localIP());
  PRINTLN("=====================");

  PRINTLN("Syncing time via NTP...");
  configTime(0, 0, ntpServer, "time.nist.gov", "time.google.com");

  time_t now = 0;
  uint8_t retries = 0;
  while (now < 100000 && retries < 20) {
    retries++;
    delay(500);
    now = time(nullptr);
  }

  if (now < 100000) {
    PRINTLN("Time sync failed.");
  } else {
    PRINTF("Time synced: %s\n", ctime(&now));
  }
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  PRINTLN("Allocating PSRAM buffers...");

  raw_buf = (int32_t*)ps_malloc((size_t)NUM_SAMPLES * sizeof(int32_t));
  pcm_buf = (int16_t*)ps_malloc((size_t)NUM_SAMPLES * sizeof(int16_t));
  tts_pcm_buf = (uint8_t*)ps_malloc((size_t)MAX_TTS_BYTES);

  if (!raw_buf || !pcm_buf || !tts_pcm_buf) {
    PRINTLN("PSRAM allocation failed!");
    while (true) delay(1000);
  }

  PRINTF("PSRAM buffers allocated. TTS max = %u bytes (%u sec)\n",
         (unsigned)MAX_TTS_BYTES, (unsigned)MAX_TTS_SECONDS);

  init_wifi_and_time();
  setup_i2s_mic();
  setup_i2s_speaker();

  PRINTLN("Ready. Long press button to ask a question.");
}

// =====================================================================
// LOOP: wait for long press, then run full pipeline
// =====================================================================
void loop() {
  digitalWrite(LED_PIN, HIGH);

  unsigned long now = millis();
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  // Button edge: press start
  if (pressed && !buttonPressed) {
    buttonPressed = true;
    pressStart = now;
  }

  // Button edge: release
  if (!pressed && buttonPressed) {
    buttonPressed = false;

    unsigned long dt = now - pressStart;

    if (dt >= LONG_PRESS_TIME) {
      digitalWrite(LED_PIN, LOW);
      PRINTLN("\n=== BUTTON TRIGGERED ===");

      record_audio();

      String question = openai_transcribe();
      if (question.length() == 0) {
        PRINTLN("No question received from STT.");
        return;
      }

      String answer = openai_answer(question);
      if (answer.length() == 0) {
        PRINTLN("No answer from Chat.");
        return;
      }

      openai_tts_play(answer);

    } else {
      // Short press ignored
      do_nop();
    }
  }
}
