/*
  Voice Assistant (ESP32-S3)
  -------------------------
  Pipeline:
    1) Long-press button to trigger recording
    2) Record audio from INMP441 over I2S (RX)
    3) Send WAV to OpenAI transcription (STT)
    4) Send transcript to OpenAI chat completion
    5) Send answer to OpenAI TTS and stream WAV to MAX98357A over I2S (TX)

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
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "config.h"

#include "esp32-hal-psram.h"
#include "driver/i2s.h"  // Legacy I2S for speaker (MAX98357A)

// ------------------------------
// SERIAL OUTPUT CONTROL
// ------------------------------
#define SERIAL_ON true

// Simple prints
#define PRINT(x)       do { if (SERIAL_ON) Serial.print(x); } while (0)
#define PRINTLN(x)     do { if (SERIAL_ON) Serial.println(x); } while (0)

// Formatted prints (printf-style)
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
// Assumption: TTS WAV is 24 kHz, 16-bit mono (we still read header from stream)
#define SAMPLE_RATE_OUT 24000

// MAX98357A I2S TX pins
static const gpio_num_t I2S_BCLK_OUT = GPIO_NUM_47;
static const gpio_num_t I2S_LRCLK_OUT = GPIO_NUM_21;
static const gpio_num_t I2S_DOUT_OUT = GPIO_NUM_17;

// =====================================================================
// LEGACY I2S TX CONFIG for MAX98357A (speaker) on I2S_NUM_1
// =====================================================================
void init_i2s_speaker_legacy(uint32_t sampleRate) {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = (int)sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // MAX98357A: left
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

  PRINTF("I2S speaker (legacy) ready @ %u Hz\n", (unsigned)sampleRate);
}

// ------------------------------
// I2S CHANNEL HANDLES (unused here, kept as-is)
// ------------------------------
i2s_chan_handle_t rx_chan = nullptr;
i2s_chan_handle_t tx_chan = nullptr;

// ------------------------------
// GLOBAL AUDIO BUFFERS (PSRAM)
// ------------------------------
// raw_buf: stores 32-bit I2S mic frames
// pcm_buf: stores converted 16-bit PCM samples for WAV upload
int32_t* raw_buf = nullptr;
int16_t* pcm_buf = nullptr;

// Small helper: inline NOP for "short press"
static inline void do_nop() {
  __asm__ __volatile__("nop" ::
                         : "memory");
}

// =====================================================================
// WAV HEADER (16-bit mono helpers)
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
  write_u32_le(h + 16, 16);  // Subchunk1Size = 16
  write_u16_le(h + 20, 1);   // PCM
  write_u16_le(h + 22, 1);   // mono
  write_u32_le(h + 24, sample_rate);
  write_u32_le(h + 28, sample_rate * 2);  // 16-bit mono -> 2 bytes/sample
  write_u16_le(h + 32, 2);                // BlockAlign
  write_u16_le(h + 34, 16);               // BitsPerSample

  memcpy(h + 36, "data", 4);
  write_u32_le(h + 40, data_size);
}

// =====================================================================
// I2S MIC SETUP (RX) for INMP441 using LEGACY DRIVER (I2S_NUM_0)
// =====================================================================
void setup_i2s_mic() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE_IN,                 // 16000
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // INMP441 -> 32-bit frames
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,   // INMP441 usually on left
    .communication_format =
      (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK_IN,  // GPIO 42
    .ws_io_num = I2S_WS_IN,    // GPIO 41
    .data_out_num = -1,
    .data_in_num = I2S_SD_IN   // GPIO 40
  };

  esp_err_t err;

  err = i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
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
    if (samples_to_read > I2S_CHUNK_SAMPLES) {
      samples_to_read = I2S_CHUNK_SAMPLES;
    }

    size_t bytes_to_read = samples_to_read * sizeof(int32_t);
    size_t bytes_read = 0;

    esp_err_t err = i2s_read(
      I2S_NUM_0,
      &raw_buf[samples_collected],
      bytes_to_read,
      &bytes_read,
      portMAX_DELAY
    );

    if (err != ESP_OK) {
      PRINT("I2S read error: ");
      PRINTLN((int)err);
      break;
    }

    int got_samples = bytes_read / sizeof(int32_t);
    samples_collected += got_samples;

    yield();
  }

  PRINT("Collected samples: ");
  PRINTLN(samples_collected);

  if (samples_collected < NUM_SAMPLES) {
    PRINTLN("Warning: not all samples collected, filling rest with zeros.");
    for (int i = samples_collected; i < NUM_SAMPLES; i++) {
      raw_buf[i] = 0;
    }
  }

  // Convert 32-bit mic frames to 16-bit PCM (simple shift)
  for (int i = 0; i < NUM_SAMPLES; i++) {
    pcm_buf[i] = raw_buf[i] >> 16;
  }
}

// =====================================================================
// DIRECT HTTPS: STT (Whisper) -> returns transcribed question
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
  String part_header =
    "--" + boundary + "\r\n"
                      "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
                      "Content-Type: audio/wav\r\n\r\n";

  String part_footer =
    "\r\n--" + boundary + "\r\n"
                          "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
                          "gpt-4o-mini-transcribe\r\n"
                          "--"
    + boundary + "--\r\n";

  uint8_t wav_header[44];
  create_wav_header(wav_header, NUM_SAMPLES * 2, SAMPLE_RATE_IN);

  int wav_header_size = 44;
  int pcm_size = NUM_SAMPLES * 2;

  int content_len =
    part_header.length() + wav_header_size + pcm_size + part_footer.length();

  // HTTP request
  client.print("POST /v1/audio/transcriptions HTTP/1.1\r\n");
  client.print("Host: api.openai.com\r\n");
  client.print("Authorization: Bearer " + String(OPENAI_API_KEY) + "\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(content_len) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  // Body
  client.write((uint8_t*)part_header.c_str(), part_header.length());
  client.write(wav_header, wav_header_size);

  const uint8_t* p = (const uint8_t*)pcm_buf;
  int remaining = pcm_size;
  while (remaining > 0) {
    int chunk = remaining > 1024 ? 1024 : remaining;
    client.write(p, chunk);
    p += chunk;
    remaining -= chunk;
  }

  client.write((uint8_t*)part_footer.c_str(), part_footer.length());

  PRINTLN("STT: Sent. Reading reply...");

  String reply;
  while (client.connected() || client.available()) {
    if (client.available()) {
      reply += (char)client.read();
    }
  }

  PRINTLN("=== RAW STT REPLY ===");
  PRINTLN(reply);

  int idx = reply.indexOf("\"text\"");
  if (idx == -1) {
    PRINTLN("STT: No 'text' field found");
    return "";
  }

  int start = reply.indexOf(":", idx);
  if (start < 0) return "";
  start++;
  while (start < reply.length() && (reply[start] == ' ' || reply[start] == '\"')) {
    start++;
  }
  int end = reply.indexOf("\"", start);
  if (end < 0) end = reply.length();

  String question = reply.substring(start, end);
  PRINT("Transcription: ");
  PRINTLN(question);
  return question;
}

// =====================================================================
// DIRECT HTTPS: Chat completion -> short spoken-style answer
// =====================================================================
String openai_answer(const String& question) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.openai.com", 443)) {
    PRINTLN("Chat: HTTPS connection failed");
    return "";
  }

  PRINTLN("Chat: Connected to OpenAI.");

  // Build JSON request body
  String body = "{";
  body += "\"model\":\"gpt-4o-mini\",";
  body += "\"messages\":[";
  body += "{\"role\":\"system\",\"content\":\"You are a helpful assistant. Answer in at most 2 short sentences suitable to be spoken aloud.\"},";
  body += "{\"role\":\"user\",\"content\":\"";

  // Escape quotes and backslashes in question
  for (size_t i = 0; i < question.length(); i++) {
    char c = question[i];
    if (c == '\"' || c == '\\') body += '\\';
    body += c;
  }

  body += "\"}";
  body += "]";
  body += "}";

  client.println("POST /v1/chat/completions HTTP/1.1");
  client.println("Host: api.openai.com");
  client.println("Content-Type: application/json");
  client.println("Authorization: Bearer " + String(OPENAI_API_KEY));
  client.println("Content-Length: " + String(body.length()));
  client.println("Connection: close");
  client.println();
  client.print(body);

  PRINTLN("Chat: Sent. Reading reply...");

  String reply;
  while (client.connected() || client.available()) {
    if (client.available()) {
      reply += (char)client.read();
    }
  }

  PRINTLN("=== RAW CHAT REPLY ===");
  PRINTLN(reply);

  int idx = reply.indexOf("\"content\"");
  if (idx == -1) {
    PRINTLN("Chat: No 'content' field found");
    return "";
  }
  int colon = reply.indexOf(":", idx);
  if (colon < 0) return "";
  int start = reply.indexOf("\"", colon);
  if (start < 0) return "";
  start++;
  int end = reply.indexOf("\"", start);
  if (end < 0) end = reply.length();

  String answer = reply.substring(start, end);

  // Unescape a minimal set of sequences
  answer.replace("\\\"", "\"");
  answer.replace("\\n", " ");
  answer.replace("\\r", " ");
  answer.replace("\\\\", "\\");

  PRINT("Assistant answer: ");
  PRINTLN(answer);
  return answer;
}

// =====================================================================
// Helper: JSON escape (for TTS input string)
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
// Helper: robust TCP chunk read (no spurious "Error reading chunk")
// =====================================================================
int readChunkBytes(WiFiClientSecure& client, uint8_t* buf, int want) {
  int waitCount = 0;

  while (true) {
    int r = client.read(buf, want);
    if (r > 0) {
      return r;  // good data
    }

    // No data yet
    delay(1);
    waitCount++;
    if (waitCount > 5000) {  // ~5 seconds
      return -1;             // timeout
    }
  }
}

// =====================================================================
// DIRECT HTTPS: TTS (WAV) -> stream to I2S speaker (legacy I2S_NUM_1)
// =====================================================================
void openai_tts_play(const String& text) {

  // Reset speaker DMA (required on ESP32-S3)
  i2s_stop(I2S_NUM_1);
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_start(I2S_NUM_1);

  WiFiClientSecure client;
  client.setInsecure();

  PRINTLN("TTS: Connecting to OpenAI...");
  if (!client.connect("api.openai.com", 443)) {
    PRINTLN("TTS: HTTPS connection failed");
    return;
  }
  PRINTLN("TTS: Connected.");

  // JSON body
  String body = "{";
  body += "\"model\":\"gpt-4o-mini-tts\",";
  body += "\"voice\":\"alloy\",";
  body += "\"response_format\":\"wav\",";
  body += "\"input\":" + jsonEscape(text);
  body += "}";

  // HTTP request
  client.print("POST /v1/audio/speech HTTP/1.1\r\n");
  client.print("Host: api.openai.com\r\n");
  client.print("Authorization: Bearer " + String(OPENAI_API_KEY) + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: " + String(body.length()) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  // Read and discard headers
  PRINTLN("TTS: Reading headers...");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "") break;
  }

  PRINTLN("TTS: Starting chunk stream...");

  uint8_t wavHeader[44];
  bool headerReceived = false;
  int headerPtr = 0;

  while (client.connected() || client.available()) {

    // Read chunk size line (hex)
    String sizeLine;
    do {
      sizeLine = client.readStringUntil('\n');
      sizeLine.trim();
    } while (sizeLine.length() == 0 && (client.connected() || client.available()));

    if (sizeLine.length() == 0) break;

    int chunkSize = strtol(sizeLine.c_str(), NULL, 16);
    PRINTF("TTS chunk size = %d\n", chunkSize);

    if (chunkSize <= 0) break;  // end of stream

    int remaining = chunkSize;
    while (remaining > 0) {

      uint8_t buf[1024];
      int toRead = min(remaining, (int)sizeof(buf));

      int len = client.read(buf, toRead);
      if (len <= 0) continue;

      remaining -= len;

      // First 44 bytes of the total stream are WAV header
      if (!headerReceived) {
        int needed = 44 - headerPtr;
        int copyLen = min(needed, len);
        memcpy(wavHeader + headerPtr, buf, copyLen);
        headerPtr += copyLen;

        if (headerPtr == 44) {
          headerReceived = true;

          uint32_t sampleRate = *(uint32_t*)&wavHeader[24];
          uint16_t bits       = *(uint16_t*)&wavHeader[34];
          uint16_t channels   = *(uint16_t*)&wavHeader[22];

          PRINTF("TTS WAV header: %lu Hz, %u-bit, %u channels\n",
                 (unsigned long)sampleRate, bits, channels);

          // Playback continues below
        }

        // If we consumed part/all header, skip to next data
        if (len <= copyLen) continue;

        // Remaining part becomes audio payload
        uint8_t* audioPtr = buf + copyLen;
        int audioLen = len - copyLen;

        size_t written;
        i2s_write(I2S_NUM_1, audioPtr, audioLen, &written, portMAX_DELAY);
        continue;
      }

      // Normal audio frames
      size_t written;
      i2s_write(I2S_NUM_1, buf, len, &written, portMAX_DELAY);
    }

    // Consume CRLF after each chunk
    client.readStringUntil('\n');
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
  raw_buf = (int32_t*)ps_malloc(NUM_SAMPLES * sizeof(int32_t));
  pcm_buf = (int16_t*)ps_malloc(NUM_SAMPLES * sizeof(int16_t));
  if (!raw_buf || !pcm_buf) {
    PRINTLN("PSRAM allocation failed!");
    while (true) delay(1000);
  }
  PRINTLN("PSRAM buffers allocated.");

  init_wifi_and_time();
  setup_i2s_mic();
  setup_i2s_speaker();

  PRINTLN("Ready. Long press button to ask a question.");
}

// =====================================================================
// LOOP: wait for long button press, then run full pipeline
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
