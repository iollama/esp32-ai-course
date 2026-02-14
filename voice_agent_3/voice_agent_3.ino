//TODO?: show the last three messages



/*
  Voice Assistant (ESP32-S3)
  -------------------------

  REQUIRED LIBRARIES
  ------------------
  This sketch requires ArduinoJson and Adafruit NeoPixel.
  Install via Arduino IDE:
    Sketch -> Include Library -> Manage Libraries...
    Search for "ArduinoJson" by Benoit Blanchon
    Install version 6.x or newer
    Search for "Adafruit NeoPixel" by Adafruit
    Install the latest stable version

  Pipeline:
    1) Long-press button to trigger recording
    2) Record audio from INMP441 over I2S (RX)  [LEGACY I2S DRIVER]
    3) Send WAV to OpenAI transcription (STT), response_format = text
    4) Send transcript to OpenAI Responses API (HTTPClient POST + ArduinoJson)
       - First request has no previous_response_id
       - Subsequent requests include previous_response_id from prior response.id
    5) Send answer to OpenAI TTS (response_format = pcm),
       download into fixed PSRAM buffer, then play over I2S (TX) [LEGACY I2S DRIVER]

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

  Legacy single-color status LED
    - LED -> GPIO 39

  Internal RGB Status LED (addressable WS2812/NeoPixel)
    - Many ESP32-S3 dev boards have a single WS2812 on GPIO 48.
    - This sketch uses Adafruit_NeoPixel (RMT-based) to avoid any I2S driver conflicts.
    - If your board uses a different pin, change STATUS_RGB_LED_PIN below.

  LED color states:
    - until WiFi ready = yellow
    - ready for input = green
    - recording audio = pink
    - STT = purple
    - working with the responses API = blue
    - downloading/playing TTS = orange
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>

// ADDED for WiFi Manager
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
// END ADDED

#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "esp32-hal-psram.h"
#include "driver/i2s.h"

// ADDED for WiFi Manager
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;
boolean ap_mode = false;
// END ADDED

// =====================================================================
// TUNING (KNOBS)
// =====================================================================
//
// RECORD_SECONDS
//   How long (seconds) to record from the microphone per request.
//   Impact:
//     - Longer: higher STT accuracy for long sentences, but bigger upload, more latency, more RAM/PSRAM.
//     - Shorter: faster interaction, smaller buffers, but may cut speech.
//
// SAMPLE_RATE_OUT
//   Playback sample rate (Hz) for TTS PCM output (speaker).
//   Impact:
//     - Higher: better audio quality, more bandwidth, more RAM/PSRAM for buffering.
//     - Lower: less data, faster download, smaller buffer, but lower fidelity.
//
// MAX_TTS_SECONDS
//   Maximum reply audio length we will buffer and play (hard cap).
//   Impact:
//     - Larger: allows longer spoken replies, but increases PSRAM buffer size and download time.
//     - Smaller: reduces memory and latency, but may truncate long replies.
//
// SYS_INSTRUCTION
//   System prompt controlling assistant response style (voice UX).
//   Impact:
//     - More strict/short: keeps TTS small and fast.
//     - More verbose: increases TTS size/latency and buffering needs.
//
// VERBOSE_LOGGING
//   If true: keep the detailed debug prints (raw replies, headers, etc.).
//   If false: print only core operational logs (milestones, errors, key outputs).
//
#define RECORD_SECONDS   3
#define SAMPLE_RATE_OUT  24000
#define MAX_TTS_SECONDS  15

//
// DEFAULT_SYS_INSTRUCTION
//   Default system prompt if not set in NVS. Controls assistant's persona.
//
// DEFAULT_TEMPERATURE
//   Default model temperature (0.0-2.0) if not set in NVS. Higher = more creative.
//
// DEFAULT_PERSIST_CONVO
//   Default conversation persistence (true/false) if not set in NVS.
//
// DEFAULT_VERBOSE_LOGGING
//   Default verbose logging setting if not set in NVS.
//
#define DEFAULT_SYS_INSTRUCTION "You are a helpful assistant. Answer in at most 2 short sentences suitable to be spoken aloud."
#define DEFAULT_TEMPERATURE      0.7f
#define DEFAULT_PERSIST_CONVO    true
#define DEFAULT_VERBOSE_LOGGING  true

// These will be loaded from NVS or set to default by load_agent_config()
String g_sys_instruction;
float  g_temperature;
bool   g_persist_conversation;
bool   g_verbose_logging;

// =====================================================================
// SERIAL OUTPUT CONTROL
// =====================================================================
#define SERIAL_ON true

// Core logging (always shown when SERIAL_ON == true)
#define CPRINT(x) \
  do { \
    if (SERIAL_ON) Serial.print(x); \
  } while (0)
#define CPRINTLN(x) \
  do { \
    if (SERIAL_ON) Serial.println(x); \
  } while (0)
#define CPRINTF(...) \
  do { \
    if (SERIAL_ON) Serial.printf(__VA_ARGS__); \
  } while (0)

// Verbose logging (only when g_verbose_logging == true)
#define VPRINT(x) \
  do { \
    if (SERIAL_ON && g_verbose_logging) Serial.print(x); \
  } while (0)
#define VPRINTLN(x) \
  do { \
    if (SERIAL_ON && g_verbose_logging) Serial.println(x); \
  } while (0)
#define VPRINTF(...) \
  do { \
    if (SERIAL_ON && g_verbose_logging) Serial.printf(__VA_ARGS__); \
  } while (0)

// ------------------------------
// GENERAL PINS
// ------------------------------
const int LED_PIN = 39;  // legacy single-color LED (if present on GPIO39)

// Internal RGB NeoPixel LED (often GPIO 48 on ESP32-S3 dev boards)
#ifndef STATUS_RGB_LED_PIN
#define STATUS_RGB_LED_PIN 48
#endif

#define STATUS_RGB_LED_COUNT 1
Adafruit_NeoPixel statusPixel(STATUS_RGB_LED_COUNT, STATUS_RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// ------------------------------
// STATUS STATE -> RGB COLOR
// ------------------------------
enum AssistantState : uint8_t {
  STATE_WIFI_WAIT = 0,    // until WiFi ready = yellow
  STATE_WIFI_CONFIG,      // in config mode = flashing yellow
  STATE_READY_FOR_INPUT,  // ready for input = green
  STATE_RECORDING,        // recording audio = pink
  STATE_STT,              // STT = purple
  STATE_RESPONSES_API,    // working with the responses API = blue
  STATE_TTS_DOWNLOADING,  // downloading TTS = orange
  STATE_TTS_PLAYING       // playing TTS = cyan
};

static AssistantState g_state = STATE_WIFI_WAIT;

static inline void set_status_led_rgb(uint8_t r, uint8_t g, uint8_t b) {
  statusPixel.setPixelColor(0, statusPixel.Color(r, g, b));
  statusPixel.show();
}

static inline void setAssistantState(AssistantState s) {
  g_state = s;
  switch (s) {
    case STATE_WIFI_WAIT: set_status_led_rgb(255, 255, 0); break;        // yellow
    case STATE_WIFI_CONFIG: /* handled in loop for flashing */ break;    // flashing yellow
    case STATE_READY_FOR_INPUT: set_status_led_rgb(0, 255, 0); break;    // green
    case STATE_RECORDING: set_status_led_rgb(255, 105, 180); break;      // pink
    case STATE_STT: set_status_led_rgb(128, 0, 128); break;              // purple
    case STATE_RESPONSES_API: set_status_led_rgb(0, 0, 255); break;      // blue
    case STATE_TTS_DOWNLOADING: set_status_led_rgb(255, 128, 0); break;  // orange
    case STATE_TTS_PLAYING: set_status_led_rgb(0, 255, 255); break;      // cyan
    default: set_status_led_rgb(0, 0, 0); break;                         // off
  }
}

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
#define NUM_SAMPLES (SAMPLE_RATE_IN * RECORD_SECONDS)
#define I2S_CHUNK_SAMPLES 1024

// INMP441 I2S pinout (ESP32-S3)
static const gpio_num_t I2S_SCK_IN = GPIO_NUM_42;  // BCLK
static const gpio_num_t I2S_WS_IN = GPIO_NUM_41;   // LRCLK
static const gpio_num_t I2S_SD_IN = GPIO_NUM_40;   // DOUT from mic

// ------------------------------
// MAX98357A I2S TX pins
// ------------------------------
static const gpio_num_t I2S_BCLK_OUT = GPIO_NUM_47;
static const gpio_num_t I2S_LRCLK_OUT = GPIO_NUM_21;
static const gpio_num_t I2S_DOUT_OUT = GPIO_NUM_17;

// ------------------------------
// TTS FULL-BUFFER SETTINGS (PSRAM)
// ------------------------------
#define TTS_BYTES_PER_SEC (SAMPLE_RATE_OUT * 2)  // 16-bit mono = 2 bytes/sample
#define MAX_TTS_BYTES (MAX_TTS_SECONDS * TTS_BYTES_PER_SEC)

uint8_t* tts_pcm_buf = nullptr;
size_t tts_pcm_len = 0;

// ------------------------------
// GLOBAL AUDIO BUFFERS (PSRAM)
// ------------------------------
int32_t* raw_buf = nullptr;  // 32-bit I2S mic frames
int16_t* pcm_buf = nullptr;  // 16-bit PCM samples for WAV upload

// ------------------------------
// Responses API conversation state
// ------------------------------
String g_prev_response_id = "";

// ------------------------------
// Small helper: inline NOP for short press
// ------------------------------
static inline void do_nop() {
  __asm__ __volatile__("nop" ::
                         : "memory");
}

// =====================================================================
// Helper: JSON escape for TTS "input"
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
    CPRINTF("%s: error body was not valid JSON (or parse failed)\n", tag);
    return;
  }

  const char* msg = doc["error"]["message"] | "";
  const char* type = doc["error"]["type"] | "";
  const char* code = doc["error"]["code"] | "";

  CPRINTF("%s: error.message: %s\n", tag, msg);
  if (type[0]) CPRINTF("%s: error.type: %s\n", tag, type);
  if (code[0]) CPRINTF("%s: error.code: %s\n", tag, code);
}

// =====================================================================
// Helper: Extract assistant text and response.id from Responses API JSON
// =====================================================================
bool parse_responses_api_text_and_id(const String& json,
                                     String& out_text,
                                     String& out_id) {
  out_text = "";
  out_id = "";

  StaticJsonDocument<6144> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    CPRINT("Responses: JSON parse failed: ");
    CPRINTLN(err.c_str());
    return false;
  }

  out_id = (const char*)(doc["id"] | "");

  JsonArray output = doc["output"].as<JsonArray>();
  if (output.isNull()) {
    CPRINTLN("Responses: missing output[]");
    return false;
  }

  for (JsonObject item : output) {
    const char* type = item["type"] | "";
    if (strcmp(type, "message") != 0) continue;

    JsonArray content = item["content"].as<JsonArray>();
    if (content.isNull()) continue;

    for (JsonObject c : content) {
      const char* ctype = c["type"] | "";
      if (strcmp(ctype, "output_text") == 0) {
        const char* txt = c["text"] | "";
        if (txt[0]) out_text += String(txt);
      }
    }
  }

  out_text.trim();
  if (out_text.length() == 0) {
    CPRINTLN("Responses: no output_text found");
    return false;
  }

  return true;
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

  CPRINTF("I2S speaker ready @ %u Hz\n", (unsigned)sampleRate);
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
  write_u16_le(h + 20, 1);  // PCM
  write_u16_le(h + 22, 1);  // mono
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
    CPRINTF("I2S mic: driver_install failed: %d\n", err);
    return;
  }

  err = i2s_set_pin(I2S_NUM_0, &pins);
  if (err != ESP_OK) {
    CPRINTF("I2S mic: set_pin failed: %d\n", err);
    return;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  CPRINTLN("I2S microphone ready (legacy, INMP441).");
}

// =====================================================================
// I2S SPEAKER SETUP (TX)
// =====================================================================
void setup_i2s_speaker() {
  init_i2s_speaker_legacy(SAMPLE_RATE_OUT);
}

// =====================================================================
// RECORD FROM MIC INTO pcm_buf (16-bit PCM)
// =====================================================================
void record_audio() {
  setAssistantState(STATE_RECORDING);

  CPRINTF("\nRecording %d seconds...\n", (int)RECORD_SECONDS);

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
      CPRINT("I2S read error: ");
      CPRINTLN((int)err);
      break;
    }

    int got_samples = (int)(bytes_read / sizeof(int32_t));
    samples_collected += got_samples;
    yield();
  }

  CPRINT("Collected samples: ");
  CPRINTLN(samples_collected);

  if (samples_collected < NUM_SAMPLES) {
    CPRINTLN("Warning: not all samples collected, filling rest with zeros.");
    for (int i = samples_collected; i < NUM_SAMPLES; i++) raw_buf[i] = 0;
  }

  for (int i = 0; i < NUM_SAMPLES; i++) {
    pcm_buf[i] = (int16_t)(raw_buf[i] >> 16);
  }
}

// =====================================================================
// STT: Upload WAV; request response_format=text; return plain transcript
// =====================================================================
String openai_transcribe() {
  setAssistantState(STATE_STT);

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.openai.com", 443)) {
    CPRINTLN("STT: HTTPS connection failed");
    return "";
  }

  CPRINTLN("STT: Connected.");

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
    (int)part_file_header.length() + wav_header_size + pcm_size + (int)part_model.length() + (int)part_respfmt.length() + (int)part_end.length();

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

  CPRINTLN("STT: Sent. Reading reply...");

  // Read status line
  String statusLine;
  while (client.connected()) {
    statusLine = client.readStringUntil('\n');
    statusLine.trim();
    if (statusLine.length() == 0) continue;
    if (statusLine.startsWith("HTTP/")) break;
  }
  VPRINT("STT: Status: ");
  VPRINTLN(statusLine);

  int httpStatus = -1;
  int sp1 = statusLine.indexOf(' ');
  if (sp1 >= 0) {
    int sp2 = statusLine.indexOf(' ', sp1 + 1);
    if (sp2 >= 0) httpStatus = statusLine.substring(sp1 + 1, sp2).toInt();
    else httpStatus = statusLine.substring(sp1 + 1).toInt();
  }

  // Read headers (discard)
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "") break;
  }

  // Read body
  String body;
  while (client.connected() || client.available()) {
    while (client.available()) body += (char)client.read();
    delay(1);
  }

  if (httpStatus != 200) {
    CPRINTF("STT: Non-200 (%d)\n", httpStatus);
    print_openai_json_error_if_any(body, "STT");
    VPRINTLN("STT raw body:");
    VPRINTLN(body);
    return "";
  }

  body.trim();
  CPRINT("Transcription: ");
  CPRINTLN(body);
  return body;
}

// =====================================================================
// Responses API: short answer with previous_response_id chaining
// =====================================================================
String openai_answer_responses(const String& question) {
  setAssistantState(STATE_RESPONSES_API);

  CPRINTLN("Responses: POST /v1/responses");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  StaticJsonDocument<2048> req; // Increased size for longer prompt
  req["model"] = "gpt-4o-mini";
  req["temperature"] = g_temperature;

  JsonArray input = req.createNestedArray("input");

  JsonObject sys = input.createNestedObject();
  sys["role"] = "system";
  sys["content"] = g_sys_instruction;

  JsonObject usr = input.createNestedObject();
  usr["role"] = "user";
  usr["content"] = question;

  if (g_persist_conversation && g_prev_response_id.length() > 0) {
    req["previous_response_id"] = g_prev_response_id;
  }

  String body;
  serializeJson(req, body);

  VPRINTLN("--- RESPONSES API REQUEST ---");
  VPRINTLN(body);
  VPRINTLN("---------------------------");

  if (!http.begin(client, "https://api.openai.com/v1/responses")) {
    CPRINTLN("Responses: http.begin failed");
    http.end();
    return "";
  }

  http.setTimeout(20000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(OPENAI_API_KEY));

  int httpCode = http.POST((uint8_t*)body.c_str(), body.length());
  String reply = http.getString();
  http.end();

  CPRINTF("Responses: HTTP status = %d\n", httpCode);

  VPRINTLN("=== RAW RESPONSES REPLY ===");
  VPRINTLN(reply);

  if (httpCode != 200) {
    print_openai_json_error_if_any(reply, "Responses");
    return "";
  }

  String answerText;
  String responseId;
  if (!parse_responses_api_text_and_id(reply, answerText, responseId)) {
    CPRINTLN("Responses: failed to extract text/id");
    return "";
  }

  if (responseId.length() > 0) {
    g_prev_response_id = responseId;
    VPRINT("Responses: stored previous_response_id = ");
    VPRINTLN(g_prev_response_id);
  } else {
    VPRINTLN("Responses: warning - missing response.id");
  }

  CPRINT("Assistant answer: ");
  CPRINTLN(answerText);

  return answerText;
}

// =====================================================================
// TTS (PCM): Download full PCM into PSRAM buffer, then play
// =====================================================================
void openai_tts_play(const String& text) {
  setAssistantState(STATE_TTS_DOWNLOADING);

  i2s_stop(I2S_NUM_1);
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_start(I2S_NUM_1);

  if (!tts_pcm_buf) {
    CPRINTLN("TTS: PSRAM buffer not allocated");
    return;
  }
  tts_pcm_len = 0;

  WiFiClientSecure client;
  client.setInsecure();

  VPRINTF("TTS: Free heap before connect: %u\n", (unsigned)ESP.getFreeHeap());
  CPRINTLN("TTS: Connecting...");
  if (!client.connect("api.openai.com", 443)) {
    CPRINTLN("TTS: HTTPS connection failed");
    return;
  }
  CPRINTLN("TTS: Connected.");
  VPRINTF("TTS: Free heap after connect: %u\n", (unsigned)ESP.getFreeHeap());

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
  VPRINTLN("TTS: Reading status line...");
  String statusLine;
  while (client.connected()) {
    statusLine = client.readStringUntil('\n');
    statusLine.trim();
    if (statusLine.length() == 0) continue;
    if (statusLine.startsWith("HTTP/")) break;
  }
  VPRINT("TTS: Status: ");
  VPRINTLN(statusLine);

  int httpStatus = -1;
  int sp1 = statusLine.indexOf(' ');
  if (sp1 >= 0) {
    int sp2 = statusLine.indexOf(' ', sp1 + 1);
    if (sp2 >= 0) httpStatus = statusLine.substring(sp1 + 1, sp2).toInt();
    else httpStatus = statusLine.substring(sp1 + 1).toInt();
  }

  // Read headers (verbose)
  VPRINTLN("TTS: Reading headers...");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "") break;
    line.trim();
    VPRINT("TTS HDR: ");
    VPRINTLN(line);
  }

  if (httpStatus != 200) {
    CPRINTF("TTS: Non-200 (%d). Reading body as text...\n", httpStatus);
    String errBody;
    unsigned long t0 = millis();
    while ((client.connected() || client.available()) && (millis() - t0 < 7000)) {
      while (client.available()) errBody += (char)client.read();
      delay(1);
    }
    print_openai_json_error_if_any(errBody, "TTS");
    VPRINTLN("TTS raw body:");
    VPRINTLN(errBody);
    return;
  }

  CPRINTLN("TTS: Downloading and buffering PCM...");

  // Chunked transfer decoding
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

  CPRINTF("TTS: Buffered %u bytes (max %u). Playing...\n",
          (unsigned)tts_pcm_len, (unsigned)MAX_TTS_BYTES);
  setAssistantState(STATE_TTS_PLAYING);

  size_t played = 0;
  while (played < tts_pcm_len) {
    size_t toWrite = tts_pcm_len - played;
    if (toWrite > 2048) toWrite = 2048;

    size_t written = 0;
    i2s_write(I2S_NUM_1, tts_pcm_buf + played, toWrite, &written, portMAX_DELAY);
    played += written;
    yield();
  }

  CPRINTLN("TTS: Playback complete.");
}

// =====================================================================
// AGENT CONFIG (NVS)
// =====================================================================
void load_agent_config() {
  preferences.begin("agentConfig", true); // read-only

  g_sys_instruction = preferences.getString("sysPrompt", DEFAULT_SYS_INSTRUCTION);
  g_temperature = preferences.getFloat("temp", DEFAULT_TEMPERATURE);
  g_persist_conversation = preferences.getBool("persist", DEFAULT_PERSIST_CONVO);
  g_verbose_logging = preferences.getBool("verbose", DEFAULT_VERBOSE_LOGGING);

  preferences.end();

  CPRINTLN("--- Agent Config Loaded ---");
  CPRINTF("System Prompt: %.60s...\n", g_sys_instruction.c_str());
  CPRINTF("Temperature: %.2f\n", g_temperature);
  CPRINTF("Persist Convo: %s\n", g_persist_conversation ? "true" : "false");
  CPRINTF("Verbose Logs: %s\n", g_verbose_logging ? "true" : "false");
  CPRINTLN("---------------------------");
}

// =====================================================================
// WIFI / SoftAP / NVS
// =====================================================================

void start_soft_ap() {
  ap_mode = true;
  setAssistantState(STATE_WIFI_CONFIG);

  uint8_t mac[6];
  char ap_ssid[18];
  WiFi.macAddress(mac);
  snprintf(ap_ssid, sizeof(ap_ssid), "VOICE-AGENT-%02X%02X", mac[4], mac[5]);

  CPRINTLN("Starting SoftAP with SSID: " + String(ap_ssid));
  WiFi.softAP(ap_ssid);
  delay(100);
  IPAddress ap_ip = WiFi.softAPIP();
  CPRINTLN("AP IP address: " + ap_ip.toString());

  dnsServer.start(53, "*", ap_ip);
}

void handle_root() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Voice Agent - Config</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 20px; }
        .container { background-color: #fff; max-width: 600px; margin: auto; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h2, h3 { color: #333; }
        .form-group { margin-bottom: 15px; }
        .form-group-inline { display: flex; align-items: center; }
        .form-group-inline label { margin-right: 10px; font-weight: normal; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input[type="text"], input[type="password"], input[type="number"], textarea { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        textarea { resize: vertical; min-height: 100px; }
        .btn { background-color: #007bff; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%; box-sizing: border-box; }
        .btn:hover { background-color: #0056b3; }
        .btn-danger { background-color: #dc3545; }
        .btn-danger:hover { background-color: #c82333; }
        .btn-secondary { background-color: #6c757d; }
        .btn-secondary:hover { background-color: #5a6268; }
        hr { border: 0; border-top: 1px solid #eee; margin: 20px 0; }
        .section { border: 1px solid #ddd; border-radius: 8px; padding: 20px; margin-bottom: 20px; }
    </style>
</head>
<body>
    <div class="container">
)rawliteral";

  html += "<h2>Voice Agent Settings</h2>";
  if (ap_mode) {
    html += "<p>First, connect this device to your WiFi network.</p>";
  } else {
    html += "<p>Device is connected to <b>" + WiFi.SSID() + "</b>.</p>";
  }
  
  // --- WiFi Section ---
  html += R"rawliteral(
        <div class="section">
            <h3>WiFi Network</h3>
            <form action="/save" method="POST">
                <div class="form-group">
                    <label for="ssid">SSID</label>
                    <input type="text" id="ssid" name="ssid" required>
                </div>
                <div class="form-group">
                    <label for="pass">Password</label>
                    <input type="password" id="pass" name="pass">
                </div>
                <button type="submit" class="btn">Save & Restart</button>
            </form>
            <hr>
            <form action="/delete" method="POST" onsubmit="return confirm('Are you sure you want to delete saved credentials and restart into AP mode?');">
                <button type="submit" class="btn btn-danger">Forget Current Network</button>
            </form>
        </div>
)rawliteral";

  // --- Agent Management Section ---
  html += "<div class='section'><h3>Agent Management</h3><form action='/saveAgent' method='POST'>";

  html += "<div class='form-group'><label for='sysPrompt'>System Prompt</label><textarea id='sysPrompt' name='sysPrompt' maxlength='2000' required>" + g_sys_instruction + "</textarea></div>";
  html += "<div class='form-group'><label for='temp'>Temperature</label><input type='number' id='temp' name='temp' min='0.0' max='2.0' step='0.1' value='" + String(g_temperature, 2) + "' required></div>";
  
  String persist_checked = g_persist_conversation ? "checked" : "";
  html += "<div class='form-group-inline'><input type='checkbox' id='persist' name='persist' value='true' " + persist_checked + "><label for='persist'>Persist Conversation</label></div>";
  
  String verbose_checked = g_verbose_logging ? "checked" : "";
  html += "<div class='form-group-inline'><input type='checkbox' id='verbose' name='verbose' value='true' " + verbose_checked + "><label for='verbose'>Verbose Logging</label></div>";

  html += "<br><button type='submit' class='btn'>Save Agent Settings</button>";
  html += "</form><hr>";

  html += R"rawliteral(
        <form action="/restoreAgent" method="POST" onsubmit="return confirm('Are you sure you want to restore default agent settings?');">
            <button type="submit" class="btn btn-secondary">Restore Defaults</button>
        </form>
    </div>
)rawliteral";

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handle_save() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  if (ssid.length() > 0) {
    CPRINTLN("Saving credentials to NVS...");
    preferences.begin("wifi-creds", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.end();

    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>WiFi Config</title><meta http-equiv="refresh" content="3;url=/" /><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:Arial,sans-serif;text-align:center;padding:40px;}.container{background-color:#fff;max-width:500px;margin:auto;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}</style></head>
<body><div class="container"><h2>Credentials Saved!</h2><p>The device will now restart and try to connect. You will be redirected shortly.</p></div></body></html>
)rawliteral";
    server.send(200, "text/html", html);
    
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Bad Request: SSID cannot be empty.");
  }
}

void handle_delete() {
    CPRINTLN("Clearing credentials from NVS...");
    preferences.begin("wifi-creds", false);
    preferences.clear();
    preferences.end();

    String html = R"rawliteral(
<!DOCTYPE html><html><head><title>Credentials Deleted</title><meta http-equiv="refresh" content="3;url=/" /><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:Arial,sans-serif;text-align:center;padding:40px;}.container{background-color:#fff;max-width:500px;margin:auto;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}</style></head>
<body><div class="container"><h2>Credentials Deleted</h2><p>The device will now restart in Access Point mode. You will be redirected shortly.</p></div></body></html>
)rawliteral";
    server.send(200, "text/html", html);
    
    delay(2000);
    ESP.restart();
}

void handle_save_agent() {
  CPRINTLN("Saving agent configuration...");
  preferences.begin("agentConfig", false);

  g_sys_instruction = server.arg("sysPrompt");
  g_temperature = server.arg("temp").toFloat();
  g_persist_conversation = server.hasArg("persist");
  g_verbose_logging = server.hasArg("verbose");

  // Clamp values just in case
  if (g_temperature < 0.0f) g_temperature = 0.0f;
  if (g_temperature > 2.0f) g_temperature = 2.0f;
  if (g_sys_instruction.length() > 2000) g_sys_instruction = g_sys_instruction.substring(0, 2000);

  preferences.putString("sysPrompt", g_sys_instruction);
  preferences.putFloat("temp", g_temperature);
  preferences.putBool("persist", g_persist_conversation);
  preferences.putBool("verbose", g_verbose_logging);
  
  preferences.end();
  
  CPRINTLN("Agent configuration saved.");
  
  // Redirect back to the root page
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handle_restore_agent() {
  CPRINTLN("Restoring default agent configuration...");
  preferences.begin("agentConfig", false);
  preferences.clear();
  preferences.end();

  // Reload the default values into the global variables
  load_agent_config();

  // Redirect back to the root page
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void setup_web_server() {
  server.on("/", HTTP_GET, handle_root);
  server.on("/save", HTTP_POST, handle_save);
  server.on("/delete", HTTP_POST, handle_delete);
  server.on("/saveAgent", HTTP_POST, handle_save_agent);
  server.on("/restoreAgent", HTTP_POST, handle_restore_agent);
  
  server.onNotFound([]() {
    if (ap_mode) {
      server.send(302, "text/plain", "http://" + WiFi.softAPIP().toString());
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });
  server.begin();
}

// =====================================================================
// WiFi + NTP init
// =====================================================================
void init_wifi_and_time() {
  setAssistantState(STATE_WIFI_WAIT);
  CPRINTLN("=== WiFi INIT ===");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  preferences.begin("wifi-creds", true);  // read-only
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  bool connected = false;
  if (ssid.length() == 0) {
    CPRINTLN("NVS empty...");
  } else {
    CPRINTLN("Found credentials in NVS. Trying to connect...");
  }

  CPRINT("SSID: ");
  CPRINTLN(ssid);

  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    CPRINT(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    connected = true;
  } else {
    CPRINTLN("\nConnection failed.");
    // If we failed with NVS credentials, clear them.
    if (preferences.isKey("ssid")) {
      CPRINTLN("Clearing invalid credentials from NVS.");
      preferences.begin("wifi-creds", false);
      preferences.clear();
      preferences.end();
    }
  }

  if (!connected) {
    WiFi.mode(WIFI_AP_STA);
    start_soft_ap();
    setup_web_server();
    // Loop will handle server clients. Execution will pause here until configured.
    return;
  }

  CPRINTLN("\nWiFi OK!");
  CPRINT("IP: ");
  CPRINTLN(WiFi.localIP());

  // ADDED for mDNS
  uint8_t mac[6];
  char hostname[18];
  WiFi.macAddress(mac);
  snprintf(hostname, sizeof(hostname), "voice-agent-%02x%02x", mac[4], mac[5]);

  if (MDNS.begin(hostname)) {
    CPRINT("mDNS responder started. Hostname: http://");
    CPRINT(hostname);
    CPRINTLN(".local");
  } else {
    CPRINTLN("Error setting up MDNS responder!");
  }
  // END ADDED

  CPRINTLN("=====================");

  setup_web_server();  // Also start server in STA mode

  CPRINTLN("Syncing time via NTP...");
  configTime(0, 0, ntpServer, "time.nist.gov", "time.google.com");

  time_t now = 0;
  uint8_t retries = 0;
  while (now < 100000 && retries < 20) {
    retries++;
    delay(500);
    now = time(nullptr);
  }

  if (now < 100000) {
    CPRINTLN("Time sync failed.");
  } else {
    VPRINTF("Time synced: %s\n", ctime(&now));
  }

  setAssistantState(STATE_READY_FOR_INPUT);
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  load_agent_config(); // Load agent settings from NVS

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Legacy single-color LED (if present on GPIO39)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // NeoPixel internal RGB LED
  statusPixel.begin();
  statusPixel.setBrightness(64);
  set_status_led_rgb(0, 0, 0);  // off
  setAssistantState(STATE_WIFI_WAIT);

  CPRINTLN("Allocating PSRAM buffers...");

  raw_buf = (int32_t*)ps_malloc((size_t)NUM_SAMPLES * sizeof(int32_t));
  pcm_buf = (int16_t*)ps_malloc((size_t)NUM_SAMPLES * sizeof(int16_t));
  tts_pcm_buf = (uint8_t*)ps_malloc((size_t)MAX_TTS_BYTES);

  if (!raw_buf || !pcm_buf || !tts_pcm_buf) {
    CPRINTLN("PSRAM allocation failed!");
    set_status_led_rgb(255, 0, 0);  // red
    while (true) delay(1000);
  }

  CPRINTF("PSRAM buffers allocated. TTS max = %u bytes (%u sec)\n",
          (unsigned)MAX_TTS_BYTES, (unsigned)MAX_TTS_SECONDS);

  init_wifi_and_time();
  setup_i2s_mic();
  setup_i2s_speaker();

  if (false == ap_mode) {
    CPRINTLN("Ready. Long press button to ask a question.");
    setAssistantState(STATE_READY_FOR_INPUT);
  }
}

// =====================================================================
// LOOP: wait for long press, then run full pipeline
// =====================================================================
void loop() {
  if (ap_mode) {
    dnsServer.processNextRequest();

    // Flash yellow LED in config mode
    if ((millis() / 500) % 2 == 0) {
      set_status_led_rgb(255, 255, 0);  // yellow
    } else {
      set_status_led_rgb(0, 0, 0);  // off
    }
  }

  server.handleClient();  // Always run the web server

  if (ap_mode) {
    return;  // Block assistant logic while in AP mode
  }

  // Keep legacy single-color LED as a simple "idle indicator":
  // ON when ready for input, OFF otherwise.
  if (g_state == STATE_READY_FOR_INPUT) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  unsigned long now = millis();
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  // If WiFi drops, show yellow. (No auto-reconnect loop here beyond WiFi stack defaults.)
  if (WiFi.status() != WL_CONNECTED) {
    if (g_state != STATE_WIFI_WAIT) setAssistantState(STATE_WIFI_WAIT);
  } else {
    if (g_state == STATE_WIFI_WAIT) setAssistantState(STATE_READY_FOR_INPUT);
  }

  if (pressed && !buttonPressed) {
    buttonPressed = true;
    pressStart = now;
  }

  if (!pressed && buttonPressed) {
    buttonPressed = false;

    unsigned long dt = now - pressStart;

    if (dt >= LONG_PRESS_TIME) {
      CPRINTLN("\n=== BUTTON TRIGGERED ===");

      if (!g_persist_conversation) {
        CPRINTLN("Conversation persistence is off. Starting fresh.");
        g_prev_response_id = "";
      }

      record_audio();  // pink

      String question = openai_transcribe();  // purple
      if (question.length() == 0) {
        CPRINTLN("No question received from STT.");
        setAssistantState(STATE_READY_FOR_INPUT);
        return;
      }

      String answer = openai_answer_responses(question);  // blue
      if (answer.length() == 0) {
        CPRINTLN("No answer from Responses API.");
        setAssistantState(STATE_READY_FOR_INPUT);
        return;
      }

      openai_tts_play(answer);  // orange

      setAssistantState(STATE_READY_FOR_INPUT);  // green
    } else {
      do_nop();
    }
  }
}
