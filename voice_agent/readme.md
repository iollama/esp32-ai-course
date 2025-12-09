# ESP32-S3 AI Voice Assistant (Streaming PTT)

A low-latency, push-to-talk voice assistant built on the ESP32-S3. This project integrates OpenAI's **Whisper** (STT), **GPT-4o mini** (LLM), and **TTS-1** (Text-to-Speech) into a single real-time conversation loop.

Unlike typical implementations that download audio files before playing, this project utilizes **real-time audio streaming** to minimize latency, playing the AI's voice response while it is still being generated.

## 🚀 Features

* **Push-to-Talk (PTT):** Physical button control for precise audio capture.
* **Dual I2S Architecture:** Uses `I2S_NUM_0` for the Microphone and `I2S_NUM_1` for the Speaker, allowing independent hardware control.
* **Streaming TTS:** Uses the `ESP32-audioI2S` library to stream MP3 data directly from OpenAI, decoding to PCM in real-time.
* **Conversation Context:** Passes `previous_response_id` to OpenAI to maintain conversation history (the AI remembers what you just said).
* **Robust State Machine:** Clearly defined states (`IDLE`, `RECORDING`, `PROCESSING`, `SPEAKING`) with error recovery.
* **Visual Diagnostics:** Serial plotter output for audio levels and buffer health.

## 🛠 Hardware Required

* **ESP32-S3** (Model N16R8 recommended for PSRAM support).
* **INMP441** Omnidirectional I2S Microphone.
* **MAX98357A** I2S Mono Amplifier.
* **Speaker** (4Ω or 8Ω, 3W).
* **Push Button** (Momentary).

## 🔌 Pinout Configuration

The project is configured for the ESP32-S3 using the following pins defined in `config.h`:

### Microphone (INMP441) - I2S Port 0
| Pin Name | ESP32 Pin | Note |
| :--- | :--- | :--- |
| **SD** | GPIO 4 | Serial Data |
| **WS** | GPIO 17 | Word Select (L/R Clock) |
| **SCK** | GPIO 18 | Serial Clock |
| **VDD** | 3.3V | |
| **GND** | GND | |
| **L/R** | GND | 3.3V |

### Amplifier (MAX98357A) - I2S Port 1
| Pin Name | ESP32 Pin | Note |
| :--- | :--- | :--- |
| **DIN** | GPIO 7 | Data In |
| **BCLK** | GPIO 6 | Bit Clock |
| **LRC** | GPIO 5 | Left/Right Clock |
| **SD** | GPIO 8 | Shutdown (HIGH = Enable) |
| **VIN** | 5V | |
| **GND** | GND | |
| **GAIN** | 5V | |


### Peripherals
| Component | ESP32 Pin | Note |
| :--- | :--- | :--- |
| **Button** | GPIO 10 | Connect between Pin and GND |

## 📦 Software Setup

### 1. Library Dependencies
Install the following libraries via the Arduino Library Manager or PlatformIO:
1.  **ESP32-audioI2S** by Schreibfaul1 (Handles TTS streaming and MP3 decoding).
2.  **ArduinoJson** (Handles API JSON parsing).

### Hardware testing 
1. mic_test - checks the INMP441 I2S microphone
2. speaker_test - checks the MAX98357A I2S Amplifier and OPENAI connection

```cpp
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASS"
#define OPENAI_API_KEY "sk-..."
