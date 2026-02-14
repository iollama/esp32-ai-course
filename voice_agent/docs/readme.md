# ESP32-S3 Voice Assistant

A voice assistant built on the ESP32-S3 that records speech, transcribes it using OpenAI's STT API, generates a response via the Responses API, and speaks it back using OpenAI's TTS API.

## How It Works

The voice assistant runs a push-to-talk pipeline triggered by a long button press:

1. **Record** - Captures audio from an INMP441 I2S microphone (16 kHz, 16-bit mono)
2. **Transcribe (STT)** - Uploads a WAV file to OpenAI's `gpt-4o-mini-transcribe` model and receives plain text
3. **Think (LLM)** - Sends the transcript to OpenAI's Responses API (`gpt-4o-mini`) with conversation history chaining via `previous_response_id`
4. **Speak (TTS)** - Sends the assistant's reply to OpenAI's `gpt-4o-mini-tts` model, downloads PCM audio into a PSRAM buffer, and plays it through a MAX98357A amplifier

The system prompt keeps replies short (at most 2 sentences) for a natural voice UX.

## Hardware Requirements

- ESP32-S3 development board (with PSRAM)
- INMP441 I2S MEMS microphone
- MAX98357A I2S amplifier + speaker
- Momentary push button
- LED (status indicator)

### Pin Configuration

| Component | Signal | GPIO |
|-----------|--------|------|
| INMP441 Microphone | SD (data) | 40 |
| | WS (word select) | 41 |
| | SCK (clock) | 42 |
| | L/R | GND (left channel) |
| | VDD | 3.3V |
| | GND | Ground |
| MAX98357A Amplifier | DIN (data) | 17 |
| | BCLK (bit clock) | 47 |
| | LRC (left/right clock) | 21 |
| | SD (shutdown) | 3.3V |
| | VIN | 5V |
| | GND | Ground |
| | GAIN | Not Connected |
| Button | + | 1 |
| | - | GND |
| Status LED | LED | 39 |

## Software Requirements

### Arduino IDE Setup

- **Board**: ESP32-S3 (via the ESP32 board package)
- **PSRAM**: Enabled (required for audio buffers)
- **Partition scheme**: Choose one with enough app space (e.g., "Huge APP")

### Libraries

- **ArduinoJson** (v6.x or newer) - Install via Arduino Library Manager

### Configuration

Create a `config.h` file in the `voice_agent_2` sketch folder (or a shared keys file) with:

```cpp
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"
#define OPENAI_API_KEY  "sk-your-openai-api-key"
```

## Project Structure

```
voice_agent/
  voice_agent_2/        Main voice assistant sketch
    voice_agent_2.ino
    config.h
  speaker_test/         Standalone speaker/amplifier test
    speaker_test.ino
  mic_test/             Standalone microphone test
    mic_test.ino
  docs/                 Documentation
```

## Getting Started

### 1. Test the Speaker

Upload `speaker_test/speaker_test.ino` to verify the MAX98357A amplifier and speaker are working. This sketch generates sine wave tones (440 Hz and 880 Hz) offline - no WiFi required.

Open the Serial Monitor at 115200 baud. You should hear alternating tones and see status messages confirming I2S initialization.

### 2. Test the Microphone

Upload `mic_test/mic_test.ino` to verify the INMP441 microphone. This sketch captures audio and displays real-time audio levels on the Serial Monitor - no WiFi required.

Open the Serial Monitor at 115200 baud. Speak near the microphone and look for:
- Diagnostic raw sample output on startup
- A live level meter showing peak and RMS values
- Status labels: `quiet`, `noise`, or `SOUND`

**Tip**: If you have the amplifier connected while testing the microphone, connect the amplifier's SD (shutdown) pin to GND to disable it.

### 3. Run the Voice Assistant

1. Set up `config.h` with your WiFi credentials and OpenAI API key
2. Upload `voice_agent_2/voice_agent_2.ino`
3. Open Serial Monitor at 115200 baud
4. Wait for WiFi connection and NTP time sync
5. Long-press the button (200 ms+) to start a voice interaction
6. Speak your question, then wait for the assistant to respond through the speaker

The status LED turns on when the system is idle and turns off while processing a request.

## Tuning Parameters

These constants in `voice_agent_2.ino` control behavior:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `RECORD_SECONDS` | 3 | Recording duration per interaction |
| `SAMPLE_RATE_OUT` | 24000 | TTS playback sample rate (Hz) |
| `MAX_TTS_SECONDS` | 15 | Max reply audio length to buffer |
| `SYS_INSTRUCTION` | (see code) | System prompt for the assistant |
| `VERBOSE_LOGGING` | true | Enable detailed debug output |

## License

This project is provided for educational purposes.