//TODO?: show the last three messages

/*
  Voice Assistant (ESP32-S3) - WebSockets S2S Edition
  ---------------------------------------------------

  REQUIRED LIBRARIES
  ------------------
  This sketch requires the following libraries. Install via Arduino IDE:
    - "ArduinoJson" by Benoit Blanchon
    - "Adafruit NeoPixel" by Adafruit
    - "ArduinoWebsockets" by Gil Maimon (v0.5.3 or newer)

  Pipeline:
    1) Dual-core FreeRTOS architecture.
    2) Core 0 handles Protocol: WiFi, Secure WebSocket to OpenAI Realtime API, Base64 encode/decode.
    3) Core 1 handles Hardware: I2S DMA for Mic/Speaker, PTT Button, Ring Buffers.
    4) Push-to-Talk (PTT) sends audio streams in real-time, instantly interrupting prior audio (Barge-In).
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include "src/ArduinoWebsockets.h"

#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "esp32-hal-psram.h"
#include "driver/i2s.h"
#include "mbedtls/base64.h"

// =====================================================================
// GLOBAL WEBSERVER & NVS
// =====================================================================
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;
boolean ap_mode = false;

// =====================================================================
// TUNING (KNOBS)
// =====================================================================
#define SAMPLE_RATE_IN   16000
#define SAMPLE_RATE_OUT  24000
#define JITTER_BUFFER_MS 300
#define JITTER_BUFFER_BYTES (SAMPLE_RATE_OUT * 2 * JITTER_BUFFER_MS / 1000)
#define LOW_WATER_BYTES    (1024 * 3)  // ~64ms at 24kHz — rebuffer only when nearly empty

#define DEFAULT_SYS_INSTRUCTION "You are a helpful voice assistant. Answer concisely and conversationally."
#define DEFAULT_TEMPERATURE      0.7f
#define DEFAULT_PERSIST_CONVO    true
#define DEFAULT_VERBOSE_LOGGING  true

String g_sys_instruction;
float  g_temperature;
bool   g_persist_conversation;
bool   g_verbose_logging;
String g_api_key;
int    g_volume = 100;

// =====================================================================
// SERIAL OUTPUT CONTROL
// =====================================================================
#define SERIAL_ON true
#define CPRINT(x)   do { if (SERIAL_ON) Serial.print(x); } while (0)
#define CPRINTLN(x) do { if (SERIAL_ON) Serial.println(x); } while (0)
#define CPRINTF(...) do { if (SERIAL_ON) Serial.printf(__VA_ARGS__); } while (0)

#define VPRINT(x)   do { if (SERIAL_ON && g_verbose_logging) Serial.print(x); } while (0)
#define VPRINTLN(x) do { if (SERIAL_ON && g_verbose_logging) Serial.println(x); } while (0)
#define VPRINTF(...) do { if (SERIAL_ON && g_verbose_logging) Serial.printf(__VA_ARGS__); } while (0)

// =====================================================================
// PINS & STATUS LED
// =====================================================================
const int LED_PIN = 39;  // legacy single-color LED
#ifndef STATUS_RGB_LED_PIN
#define STATUS_RGB_LED_PIN 48
#endif

Adafruit_NeoPixel statusPixel(1, STATUS_RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

enum AssistantState : uint8_t {
  STATE_WIFI_WAIT = 0,
  STATE_WIFI_CONFIG,
  STATE_READY_FOR_INPUT,
  STATE_RECORDING,
  STATE_THINKING,
  STATE_SPEAKING
};

volatile AssistantState g_state = STATE_WIFI_WAIT;

static inline void set_status_led_rgb(uint8_t r, uint8_t g, uint8_t b) {
  statusPixel.setPixelColor(0, statusPixel.Color(r, g, b));
  statusPixel.show();
}

static inline void setAssistantState(AssistantState s) {
  g_state = s;
  switch (s) {
    case STATE_WIFI_WAIT:       set_status_led_rgb(255, 255, 0); break;   // yellow
    case STATE_WIFI_CONFIG:     break;                                    // flashing yellow handled in loop
    case STATE_READY_FOR_INPUT: set_status_led_rgb(0, 255, 0); break;     // green
    case STATE_RECORDING:       set_status_led_rgb(255, 105, 180); break; // pink
    case STATE_THINKING:        set_status_led_rgb(0, 0, 255); break;     // blue
    case STATE_SPEAKING:        set_status_led_rgb(0, 255, 255); break;   // cyan
    default:                    set_status_led_rgb(0, 0, 0); break;
  }
}

// =====================================================================
// AUDIO PINS
// =====================================================================
const int BUTTON_PIN = 1;

static const gpio_num_t I2S_SCK_IN = GPIO_NUM_42;
static const gpio_num_t I2S_WS_IN = GPIO_NUM_41;
static const gpio_num_t I2S_SD_IN = GPIO_NUM_40;

static const gpio_num_t I2S_BCLK_OUT = GPIO_NUM_47;
static const gpio_num_t I2S_LRCLK_OUT = GPIO_NUM_21;
static const gpio_num_t I2S_DOUT_OUT = GPIO_NUM_17;

// =====================================================================
// PSRAM THREAD-SAFE RING BUFFER
// =====================================================================
class PSRAMRingBuffer {
private:
    uint8_t* buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;
    SemaphoreHandle_t mutex;
public:
    PSRAMRingBuffer(size_t capacity) {
        size = capacity;
        buffer = (uint8_t*)ps_malloc(size);
        head = 0; tail = 0; count = 0;
        mutex = xSemaphoreCreateMutex();
    }
    bool write(const uint8_t* data, size_t len) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        if (size - count < len) { 
            xSemaphoreGive(mutex); 
            return false; 
        }
        for(size_t i=0; i<len; i++) {
            buffer[head] = data[i];
            head = (head + 1) % size;
        }
        count += len;
        xSemaphoreGive(mutex);
        return true;
    }
    size_t read(uint8_t* data, size_t len) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        size_t available = (count < len) ? count : len;
        for(size_t i=0; i<available; i++) {
            data[i] = buffer[tail];
            tail = (tail + 1) % size;
        }
        count -= available;
        xSemaphoreGive(mutex);
        return available;
    }
    size_t available() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        size_t c = count;
        xSemaphoreGive(mutex);
        return c;
    }
    void clear() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        head = 0; tail = 0; count = 0;
        xSemaphoreGive(mutex);
    }
};

PSRAMRingBuffer* in_ring_buf = nullptr;
PSRAMRingBuffer* out_ring_buf = nullptr;

// =====================================================================
// PRE-ALLOCATED AUDIO BUFFERS (avoids heap fragmentation)
// =====================================================================
// Encode: ceil(2048 * 4/3) + 4 = 2736 bytes
#define AUDIO_CHUNK_SIZE    2048
#define B64_CHUNK_SIZE      2736
#define WS_FRAME_SIZE       2800   // prefix(46) + b64(2732) + suffix+null(3)
#define AUDIO_DECODE_SIZE   49152  // 48KB — covers largest expected audio delta

static uint8_t  s_mic_chunk[AUDIO_CHUNK_SIZE];
static char     s_b64_buf[B64_CHUNK_SIZE];
static char     s_ws_frame[WS_FRAME_SIZE];
static uint8_t* s_decode_buf = nullptr;  // PSRAM, allocated in setup()

// Inter-Task Communication Flags
volatile bool cmd_cancel = false;
volatile bool cmd_commit = false;
volatile bool ws_response_done = false;

// =====================================================================
// HELPERS
// =====================================================================
String get_api_key() {
  return (g_api_key.length() > 0) ? g_api_key : String(OPENAI_API_KEY);
}

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

// Encode audio chunk into s_b64_buf/s_ws_frame and send via wsClient.
// Uses only pre-allocated static buffers — no heap allocation.
static void sendAudioChunk(websockets::WebsocketsClient& ws, const uint8_t* data, size_t len) {
    size_t b64_len = 0;
    mbedtls_base64_encode((unsigned char*)s_b64_buf, B64_CHUNK_SIZE, &b64_len, data, len);
    s_b64_buf[b64_len] = '\0';
    snprintf(s_ws_frame, WS_FRAME_SIZE,
             "{\"type\":\"input_audio_buffer.append\",\"audio\":\"%s\"}", s_b64_buf);
    ws.send(s_ws_frame);
}

// =====================================================================
// I2S HARDWARE SETUP
// =====================================================================
void setup_i2s_mic() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE_IN,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 is 32-bit/24-bit
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
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
  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
  CPRINTLN("I2S Mic Ready");
}

void setup_i2s_speaker() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE_OUT,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
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
  CPRINTLN("I2S Speaker Ready");
}

// =====================================================================
// PROTOCOL TASK (CORE 0) - OpenAI Realtime WebSockets
// =====================================================================
using namespace websockets;

void manage_websockets() {
    WebsocketsClient wsClient;
    wsClient.setInsecure();

    wsClient.onMessage([](WebsocketsMessage message) {
        String payload = message.data();
        
        // Fast manual parse for latency optimization
        if (payload.indexOf("\"response.audio.delta\"") > 0) {
            int audioStart = payload.indexOf("\"delta\":\"") + 9;
            int audioEnd   = (audioStart > 8) ? payload.indexOf("\"", audioStart) : -1;
            if (audioStart > 8 && audioEnd > audioStart) {
                String b64 = payload.substring(audioStart, audioEnd);
                size_t olen = 0;
                mbedtls_base64_decode(NULL, 0, &olen, (const unsigned char*)b64.c_str(), b64.length());
                if (olen > 0 && olen <= AUDIO_DECODE_SIZE && s_decode_buf) {
                    mbedtls_base64_decode(s_decode_buf, AUDIO_DECODE_SIZE, &olen,
                                         (const unsigned char*)b64.c_str(), b64.length());
                    out_ring_buf->write(s_decode_buf, olen);
                    VPRINTF("WS: Decoded and buffered %d bytes of audio\n", olen);
                }
            }
        } else if (payload.indexOf("\"response.done\"") > 0 || payload.indexOf("\"response.audio.done\"") > 0) {
            CPRINTLN("WS: response done");
            ws_response_done = true;
        } else if (payload.indexOf("\"error\"") > 0) {
            CPRINTLN("WS Error:");
            CPRINTLN(payload);
        } else {
            // For verbose debugging, log other events (like session.created, etc.)
            if (payload.length() < 500) {
                 VPRINTLN("WS Event: " + payload);
            } else {
                 VPRINTLN("WS Event: [Large Payload]");
            }
        }
    });

    wsClient.onEvent([](WebsocketsEvent event, String data) {
        if(event == WebsocketsEvent::ConnectionOpened) {
            CPRINTLN("WS Event: Connection Opened");
        } else if(event == WebsocketsEvent::ConnectionClosed) {
            CPRINT("WS Event: Connection Closed. Reason: ");
            CPRINTLN(data);
        } else if(event == WebsocketsEvent::GotPing) {
            VPRINTLN("WS Event: Got Ping");
        } else if(event == WebsocketsEvent::GotPong) {
            VPRINTLN("WS Event: Got Pong");
        }
    });

    CPRINTF("Connecting to OpenAI Realtime API... Free Heap: %d\n", ESP.getFreeHeap());
    String key = get_api_key();
    key.trim(); // Ensure no hidden newlines break the HTTP headers!
    if (key.length() < 10) {
         CPRINTLN("WARNING: API Key seems invalid or empty!");
    }
    

    wsClient.addHeader("Authorization", "Bearer " + key);
    wsClient.addHeader("OpenAI-Beta", "realtime=v1");

    if (!wsClient.connectSecure("api.openai.com", 443, "/v1/realtime?model=gpt-4o-realtime-preview")) {
        CPRINTLN("WS Connection failed!");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }
    CPRINTLN("WS Connected!");
    setAssistantState(STATE_READY_FOR_INPUT);

    String sessionUpdate = "{\"type\":\"session.update\",\"session\":{\"modalities\":[\"audio\",\"text\"],\"instructions\":";
    sessionUpdate += jsonEscape(g_sys_instruction);
    sessionUpdate += ",\"voice\":\"alloy\",\"input_audio_format\":\"pcm16\",\"output_audio_format\":\"pcm16\",\"turn_detection\":null,\"temperature\":";
    sessionUpdate += String(g_temperature);
    sessionUpdate += "}}";
    CPRINTLN("WS: Sending session.update...");
    wsClient.send(sessionUpdate);

    while(wsClient.available() && WiFi.status() == WL_CONNECTED) {
        wsClient.poll();

        if (cmd_cancel) {
            cmd_cancel = false;
            CPRINTLN("WS: Sending response.cancel (Barge-in)");
            wsClient.send("{\"type\":\"response.cancel\"}");
            ws_response_done = false;
        }

        if (g_state == STATE_RECORDING) {
            if (in_ring_buf->available() >= AUDIO_CHUNK_SIZE) {
                in_ring_buf->read(s_mic_chunk, AUDIO_CHUNK_SIZE);
                sendAudioChunk(wsClient, s_mic_chunk, AUDIO_CHUNK_SIZE);
            }
        }

        if (cmd_commit) {
            cmd_commit = false;
            // Flush remaining mic buffer in AUDIO_CHUNK_SIZE chunks — no malloc needed
            while (in_ring_buf->available() > 0) {
                size_t to_read = min((size_t)AUDIO_CHUNK_SIZE, in_ring_buf->available());
                in_ring_buf->read(s_mic_chunk, to_read);
                sendAudioChunk(wsClient, s_mic_chunk, to_read);
            }
            CPRINTLN("WS: Sending input_audio_buffer.commit");
            wsClient.send("{\"type\":\"input_audio_buffer.commit\"}");
            CPRINTLN("WS: Sending response.create");
            wsClient.send("{\"type\":\"response.create\"}");
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    CPRINTLN("WS Disconnected.");
    setAssistantState(STATE_WIFI_WAIT);
}

void protocol_task(void* pvParameters) {
    // Wait until Wi-Fi is connected before trying to establish a WebSocket connection
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    while(true) {
        if (WiFi.status() == WL_CONNECTED && !ap_mode) {
            manage_websockets();
        } else {
             vTaskDelay(pdMS_TO_TICKS(1000));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// =====================================================================
// NVS & WEBSERVER CONFIG
// =====================================================================
void load_agent_config() {
  preferences.begin("agentConfig", true);
  g_sys_instruction = preferences.getString("sysPrompt", DEFAULT_SYS_INSTRUCTION);
  g_temperature = preferences.getFloat("temp", DEFAULT_TEMPERATURE);
  g_persist_conversation = preferences.getBool("persist", DEFAULT_PERSIST_CONVO);
  g_verbose_logging = preferences.getBool("verbose", DEFAULT_VERBOSE_LOGGING);
  g_api_key = preferences.getString("apiKey", "");
  g_volume = preferences.getInt("volume", 100);
  if (g_volume < 0) g_volume = 0;
  if (g_volume > 100) g_volume = 100;
  preferences.end();
}

void start_soft_ap() {
  ap_mode = true;
  setAssistantState(STATE_WIFI_CONFIG);
  uint8_t mac[6];
  char ap_ssid[18];
  WiFi.macAddress(mac);
  snprintf(ap_ssid, sizeof(ap_ssid), "VOICE-AGENT-%02X%02X", mac[4], mac[5]);
  WiFi.softAP(ap_ssid);
  IPAddress ap_ip = WiFi.softAPIP();
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
  
  html += R"rawliteral(
        <div class="section">
            <h3>WiFi Network</h3>
            <form action="/save" method="POST">
                <div class="form-group"><label for="ssid">SSID</label><input type="text" id="ssid" name="ssid" required></div>
                <div class="form-group"><label for="pass">Password</label><input type="password" id="pass" name="pass"></div>
                <button type="submit" class="btn">Save & Restart</button>
            </form>
            <hr><form action="/delete" method="POST"><button type="submit" class="btn btn-danger">Forget Network</button></form>
        </div>
)rawliteral";

  html += "<div class='section'><h3>Agent Management</h3><form action='/saveAgent' method='POST'>";
  html += "<div class='form-group'><label for='sysPrompt'>System Prompt</label><textarea id='sysPrompt' name='sysPrompt' maxlength='2000' required>" + g_sys_instruction + "</textarea></div>";
  html += "<div class='form-group'><label for='temp'>Temperature</label><input type='number' id='temp' name='temp' min='0.0' max='2.0' step='0.1' value='" + String(g_temperature, 2) + "' required></div>";
  String persist_checked = g_persist_conversation ? "checked" : "";
  html += "<div class='form-group-inline'><input type='checkbox' id='persist' name='persist' value='true' " + persist_checked + "><label for='persist'>Persist Conversation</label></div>";
  String verbose_checked = g_verbose_logging ? "checked" : "";
  html += "<div class='form-group-inline'><input type='checkbox' id='verbose' name='verbose' value='true' " + verbose_checked + "><label for='verbose'>Verbose Logging</label></div>";
  html += "<br><button type='submit' class='btn'>Save Agent Settings</button></form><hr>";
  html += "<form action='/restoreAgent' method='POST' onsubmit=\"return confirm('Restore default agent settings?');\"><button type='submit' class='btn btn-secondary'>Restore Defaults</button></form></div>";

  html += "<div class='section'><h3>OpenAI API Key</h3><form action='/saveApiKey' method='POST'>";
  html += "<p>Status: <b>" + String(g_api_key.length() > 0 ? "Custom key is set" : "Using default key") + "</b></p>";
  html += "<div class='form-group'><label for='apiKey'>New API Key</label><input type='password' id='apiKey' name='apiKey' placeholder='sk-...' required></div>";
  html += "<button type='submit' class='btn'>Save API Key</button></form><hr>";
  html += "<form action='/clearApiKey' method='POST' onsubmit=\"return confirm('Clear saved key and revert to compiled default?');\"><button type='submit' class='btn btn-secondary'>Clear Saved Key</button></form></div>";

  html += "<div class='section'><h3>Volume</h3><form action='/saveVolume' method='POST'>";
  html += "<div class='form-group'><label for='volume'>Playback Volume: <span id='volVal'>" + String(g_volume) + "</span>%</label>";
  html += "<input type='range' id='volume' name='volume' min='0' max='100' value='" + String(g_volume) + "' oninput=\"document.getElementById('volVal').textContent=this.value\"></div>";
  html += "<button type='submit' class='btn'>Save Volume</button></form></div>";

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handle_save() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (ssid.length() > 0) {
    preferences.begin("wifi-creds", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.end();
    server.send(200, "text/plain", "Credentials Saved! Restarting...");
    delay(2000); ESP.restart();
  }
}
void handle_delete() {
    preferences.begin("wifi-creds", false);
    preferences.clear();
    preferences.end();
    server.send(200, "text/plain", "Deleted! Restarting...");
    delay(2000); ESP.restart();
}
void handle_save_agent() {
  preferences.begin("agentConfig", false);
  g_sys_instruction = server.arg("sysPrompt");
  g_temperature = server.arg("temp").toFloat();
  g_persist_conversation = server.hasArg("persist");
  g_verbose_logging = server.hasArg("verbose");
  preferences.putString("sysPrompt", g_sys_instruction);
  preferences.putFloat("temp", g_temperature);
  preferences.putBool("persist", g_persist_conversation);
  preferences.putBool("verbose", g_verbose_logging);
  preferences.end();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
void handle_restore_agent() {
  preferences.begin("agentConfig", false);
  preferences.clear();
  preferences.end();
  load_agent_config();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
void handle_save_api_key() {
  String key = server.arg("apiKey");
  if (key.length() > 0) {
    preferences.begin("agentConfig", false);
    preferences.putString("apiKey", key);
    preferences.end();
    g_api_key = key;
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
void handle_clear_api_key() {
  preferences.begin("agentConfig", false);
  preferences.remove("apiKey");
  preferences.end();
  g_api_key = "";
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
void handle_save_volume() {
  g_volume = server.arg("volume").toInt();
  preferences.begin("agentConfig", false);
  preferences.putInt("volume", g_volume);
  preferences.end();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void setup_web_server() {
  server.on("/", HTTP_GET, handle_root);
  server.on("/save", HTTP_POST, handle_save);
  server.on("/delete", HTTP_POST, handle_delete);
  server.on("/saveAgent", HTTP_POST, handle_save_agent);
  server.on("/restoreAgent", HTTP_POST, handle_restore_agent);
  server.on("/saveApiKey", HTTP_POST, handle_save_api_key);
  server.on("/clearApiKey", HTTP_POST, handle_clear_api_key);
  server.on("/saveVolume", HTTP_POST, handle_save_volume);

  server.onNotFound([]() {
    if (ap_mode) {
      server.send(302, "text/plain", "http://" + WiFi.softAPIP().toString());
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
}

void init_wifi_and_time() {
  setAssistantState(STATE_WIFI_WAIT);
  CPRINTLN("=== WiFi INIT ===");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  preferences.begin("wifi-creds", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid.length() == 0) {
    CPRINTLN("NVS empty... Starting AP Config mode.");
  } else {
    CPRINTLN("Found credentials in NVS. Trying to connect to: " + ssid);
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) { 
      CPRINT("."); 
      delay(500); 
  }

  if (WiFi.status() != WL_CONNECTED) {
    CPRINTLN("\nConnection failed or timed out.");
    if (preferences.isKey("ssid")) {
      CPRINTLN("Clearing invalid credentials from NVS.");
      preferences.begin("wifi-creds", false);
      preferences.clear();
      preferences.end();
    }
    CPRINTLN("Falling back to AP Config Mode.");
    WiFi.mode(WIFI_AP_STA);
    start_soft_ap();
    setup_web_server();
    return;
  }
  
  CPRINTLN("\nWiFi Connected!");
  CPRINT("IP Address: ");
  CPRINTLN(WiFi.localIP());

  uint8_t mac[6];
  char hostname[18];
  WiFi.macAddress(mac);
  snprintf(hostname, sizeof(hostname), "voice-agent-%02x%02x", mac[4], mac[5]);

  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
    CPRINT("mDNS started. Web config at: http://");
    CPRINT(hostname);
    CPRINTLN(".local");
  } else {
    CPRINTLN("Error setting up mDNS responder!");
  }

  // Sync Time for SSL/TLS
  CPRINTLN("\nSyncing time via NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  time_t now = 0;
  uint8_t retries = 0;
  while (now < 100000 && retries < 20) {
    retries++;
    delay(500);
    now = time(nullptr);
  }
  if (now > 100000) {
    CPRINTLN("Time synced!");
  } else {
    CPRINTLN("Time sync failed, TLS might have issues.");
  }
  
  // HIGH PERFORMANCE MODE (Disable PS for low latency)
  esp_wifi_set_ps(WIFI_PS_NONE);
  setup_web_server();
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  load_agent_config();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  statusPixel.begin();
  statusPixel.setBrightness(64);
  set_status_led_rgb(0, 0, 0);

  // Allocate large buffers in PSRAM
  in_ring_buf   = new PSRAMRingBuffer(16000 * 2 * 2);  // 2s @ 16kHz 16-bit
  out_ring_buf  = new PSRAMRingBuffer(24000 * 2 * 30);  // 20s @ 24kHz 16-bit
  s_decode_buf  = (uint8_t*)ps_malloc(AUDIO_DECODE_SIZE);

  init_wifi_and_time();
  setup_i2s_mic();
  setup_i2s_speaker();

  // Create Protocol Task on Core 0
  xTaskCreatePinnedToCore(
      protocol_task, 
      "ProtocolTask", 
      16384, 
      NULL, 
      1, 
      NULL, 
      0
  );
}

// =====================================================================
// HARDWARE LOOP (CORE 1) - Polled quickly
// =====================================================================
bool buttonState = false;
bool lastButtonReading = false;
unsigned long lastDebounceTime = 0;

void loop() {
  if (ap_mode) {
    dnsServer.processNextRequest();
    if ((millis() / 500) % 2 == 0) set_status_led_rgb(255, 255, 0);
    else set_status_led_rgb(0, 0, 0);
    server.handleClient();
    return;
  }

  server.handleClient();

  // 1. Button PTT Debounce & State Management
  bool reading = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long now = millis();
  if (reading != lastButtonReading) {
      lastDebounceTime = now;
  }
  if ((now - lastDebounceTime) > 50) {
      if (reading != buttonState) {
          buttonState = reading;
          if (buttonState) {
              CPRINTLN("PTT PRESSED");
              out_ring_buf->clear();
              in_ring_buf->clear();
              cmd_cancel = true;
              setAssistantState(STATE_RECORDING);
          } else {
              CPRINTLN("PTT RELEASED");
              cmd_commit = true;
              setAssistantState(STATE_THINKING);
          }
      }
  }
  lastButtonReading = reading;

  // 2. Microphone Capture
  if (g_state == STATE_RECORDING) {
      size_t bytes_read = 0;
      uint8_t mic_buf[1024];
      i2s_read(I2S_NUM_0, mic_buf, 1024, &bytes_read, 0);
      if (bytes_read > 0) {
          int samples = bytes_read / 4;
          int32_t* raw32 = (int32_t*)mic_buf;
          int16_t* pcm16 = (int16_t*)mic_buf;
          for(int i=0; i<samples; i++) {
              pcm16[i] = (int16_t)(raw32[i] >> 16);
          }
          in_ring_buf->write((uint8_t*)pcm16, samples * 2);
      }
  } else {
      // Keep DMA drained to prevent stale audio
      size_t bytes_read = 0;
      uint8_t mic_buf[1024];
      i2s_read(I2S_NUM_0, mic_buf, 1024, &bytes_read, 0);
  }

  // 3. Speaker Playback & Jitter Management
  if (g_state == STATE_THINKING || g_state == STATE_SPEAKING || g_state == STATE_READY_FOR_INPUT) {
      size_t avail = out_ring_buf->available();
      
      // Start speaking if we hit jitter threshold, or if response is done and any data is waiting
      if (g_state != STATE_SPEAKING && (avail >= JITTER_BUFFER_BYTES || (ws_response_done && avail > 0))) {
          CPRINTF("Speaker: Starting playback (%d bytes, response_done=%d)\n", avail, ws_response_done);
          setAssistantState(STATE_SPEAKING);
      }

      if (g_state == STATE_SPEAKING) {
          if (avail < LOW_WATER_BYTES && !ws_response_done) {
              // Low water — rebuffer to avoid choppy underrun gaps mid-stream
              CPRINTF("Speaker: Rebuffering (only %d bytes left)\n", avail);
              setAssistantState(STATE_THINKING);
              uint8_t zero[1024] = {0};
              size_t bw = 0;
              i2s_write(I2S_NUM_1, zero, 1024, &bw, 0);
          } else if (avail > 0) {
              size_t to_read = (avail > 1024) ? 1024 : avail;
              uint8_t spk_buf[1024];
              out_ring_buf->read(spk_buf, to_read);

              // Apply Volume
              if (g_volume < 100) {
                  int16_t* samples = (int16_t*)spk_buf;
                  for(size_t i=0; i<to_read/2; i++) {
                      samples[i] = (int16_t)(((int32_t)samples[i] * g_volume) / 100);
                  }
              }
              size_t bw = 0;
              i2s_write(I2S_NUM_1, spk_buf, to_read, &bw, portMAX_DELAY);
          } else {
              // True underrun (avail == 0)
              uint8_t zero[1024] = {0};
              size_t bw = 0;
              i2s_write(I2S_NUM_1, zero, 1024, &bw, 0);

              if (ws_response_done) {
                  CPRINTLN("Speaker: Playback finished (response.done).");
                  setAssistantState(STATE_READY_FOR_INPUT);
                  ws_response_done = false;
              }
          }
      } else {
          // Play silence to maintain DMA pace
          uint8_t zero[1024] = {0};
          size_t bw = 0;
          i2s_write(I2S_NUM_1, zero, 1024, &bw, portMAX_DELAY); 
      }
  } else {
      // During recording, pace loop via delays or non-blocking
      vTaskDelay(pdMS_TO_TICKS(1));
  }
}
