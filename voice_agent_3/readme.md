# Voice Agent 3 - ESP32-S3 Voice Assistant (Advanced)

An enhanced voice assistant for the ESP32-S3 with WiFi captive portal configuration, NVS-persisted agent settings, and RGB LED status indicators. Built on top of the core voice assistant pipeline (STT, LLM, TTS) using OpenAI APIs.

## What's New in Voice Agent 3

Compared to the base voice assistant (`voice_agent_2`), this version adds:

- **SoftAP WiFi Configuration** - No hardcoded WiFi credentials. On first boot (or when credentials fail), the device starts a captive portal (`VOICE-AGENT-XXXX`) where you can enter your WiFi SSID and password via a web browser.
- **mDNS Support** - Once connected, the device is accessible at `http://voice-agent-XXXX.local` (where XXXX is derived from the MAC address).
- **Web-Based Agent Configuration** - A settings page to configure the system prompt, temperature, conversation persistence, and verbose logging -- all saved to NVS (non-volatile storage) and persisted across reboots.
- **RGB NeoPixel Status LED** - Uses the built-in WS2812 LED (GPIO 48) on most ESP32-S3 dev boards to show the current pipeline state with distinct colors.
- **Configurable Conversation Persistence** - Toggle whether conversation history is maintained across button presses via `previous_response_id` chaining.

## How It Works

The voice assistant runs a push-to-talk pipeline triggered by a long button press (200 ms+):

1. **Record** - Captures audio from an INMP441 I2S microphone (16 kHz, 16-bit mono)
2. **Transcribe (STT)** - Uploads a WAV file to OpenAI's `gpt-4o-mini-transcribe` model
3. **Think (LLM)** - Sends the transcript to OpenAI's Responses API (`gpt-4o-mini`) with optional conversation chaining
4. **Speak (TTS)** - Downloads PCM audio from OpenAI's `gpt-4o-mini-tts` into a PSRAM buffer and plays it through the speaker

## RGB LED Status Colors

| Color | State |
|-------|-------|
| Yellow (solid) | Waiting for WiFi connection |
| Yellow (flashing) | SoftAP configuration mode |
| Green | Ready for input |
| Pink | Recording audio |
| Purple | Speech-to-text (STT) |
| Blue | Querying Responses API (LLM) |
| Orange | Downloading TTS audio |
| Cyan | Playing TTS audio |
| Red | PSRAM allocation failure |

## Hardware Requirements

- ESP32-S3 development board (with PSRAM and built-in WS2812 NeoPixel LED)
- INMP441 I2S MEMS microphone
- MAX98357A I2S amplifier + speaker
- Momentary push button
- (Optional) External LED for legacy status indicator

### Pin Configuration

| Component | Signal | GPIO |
|-----------|--------|------|
| INMP441 Microphone | SD (data) | 40 |
| | WS (word select) | 41 |
| | SCK (clock) | 42 |
| | L/R | GND (left channel) |
| | VDD | 3.3V |
| MAX98357A Amplifier | DIN (data) | 17 |
| | BCLK (bit clock) | 47 |
| | LRC (left/right clock) | 21 |
| | SD (shutdown) | 3.3V |
| | VIN | 5V |
| | GAIN | Not Connected |
| Button | + | 1 |
| | - | GND |
| NeoPixel RGB LED | Data | 48 (built-in) |
| Legacy LED (optional) | LED | 39 |

## Software Requirements

### Arduino IDE Setup

- **Board**: ESP32-S3 (via the ESP32 board package)
- **PSRAM**: Enabled (required for audio buffers)
- **Partition scheme**: Choose one with enough app space (e.g., "Huge APP")

### Libraries

Install via Arduino Library Manager (`Sketch > Include Library > Manage Libraries...`):

- **ArduinoJson** (v6.x or newer) by Benoit Blanchon
- **Adafruit NeoPixel** (latest stable) by Adafruit

### Configuration

Create a `config.h` file with your OpenAI API key:

```cpp
#define OPENAI_API_KEY "sk-your-openai-api-key"
```

WiFi credentials are **not** needed in `config.h` -- they are configured at runtime via the captive portal and stored in NVS.

## Getting Started

### 1. Upload the Sketch

1. Open `voice_agent_3.ino` in Arduino IDE
2. Ensure `config.h` contains your OpenAI API key
3. Select your ESP32-S3 board with PSRAM enabled
4. Upload

### 2. First-Time WiFi Setup

1. On first boot, the device creates a WiFi access point named `VOICE-AGENT-XXXX`
2. The RGB LED flashes yellow to indicate configuration mode
3. Connect to this network with your phone or computer
4. A captive portal page opens automatically (or navigate to `192.168.4.1`)
5. Enter your WiFi SSID and password, then click **Save & Restart**
6. The device restarts and connects to your WiFi network
7. The RGB LED turns green when ready

### 3. Configure Agent Settings

Once connected to WiFi, open a browser and navigate to:

```
http://voice-agent-XXXX.local
```

From the settings page you can configure:

- **System Prompt** - Control the assistant's persona and response style
- **Temperature** (0.0 - 2.0) - Higher values produce more creative responses
- **Persist Conversation** - When enabled, the assistant remembers previous exchanges within a session
- **Verbose Logging** - Enable detailed debug output on the Serial Monitor

Click **Save Agent Settings** to persist changes to NVS. Use **Restore Defaults** to reset all agent settings.

### 4. Use the Voice Assistant

1. Long-press the button (200 ms+) to start recording
2. Speak your question during the recording window (default: 3 seconds)
3. Watch the RGB LED cycle through the pipeline stages
4. Listen for the assistant's spoken response

## Tuning Parameters

These constants in `voice_agent_3.ino` control core behavior:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `RECORD_SECONDS` | 3 | Recording duration per interaction |
| `SAMPLE_RATE_OUT` | 24000 | TTS playback sample rate (Hz) |
| `MAX_TTS_SECONDS` | 15 | Max reply audio length to buffer |

These defaults are configurable at runtime via the web interface:

| Setting | Default | Description |
|---------|---------|-------------|
| System Prompt | "You are a helpful assistant..." | Controls assistant persona |
| Temperature | 0.7 | Model creativity (0.0 - 2.0) |
| Persist Conversation | true | Chain responses across interactions |
| Verbose Logging | true | Detailed serial debug output |

## Web Server Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Settings page |
| POST | `/save` | Save WiFi credentials and restart |
| POST | `/delete` | Forget WiFi credentials and restart into AP mode |
| POST | `/saveAgent` | Save agent configuration to NVS |
| POST | `/restoreAgent` | Restore default agent settings |

## Troubleshooting

- **LED stays yellow**: WiFi connection failed. The device will fall back to SoftAP mode after 15 seconds. Check that your WiFi credentials are correct.
- **LED turns red**: PSRAM allocation failed. Make sure PSRAM is enabled in your board settings.
- **No sound from speaker**: Verify the MAX98357A SD pin is connected to 3.3V (not GND). Run the `speaker_test` sketch to isolate the issue.
- **Microphone not picking up audio**: Check INMP441 wiring (especially L/R to GND for left channel). Run the `mic_test` sketch to verify levels.
- **Can't reach `.local` address**: mDNS may not work on all networks/devices. Use the IP address printed to Serial Monitor instead.

## License

This project is provided for educational purposes.