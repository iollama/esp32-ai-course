# ESP32-S3 AI Voice Assistant

A push-to-talk AI voice assistant built with ESP32-S3 that integrates OpenAI's Whisper, GPT-4o mini, and TTS APIs for natural voice conversations.

## 📋 Features

- **Push-to-Talk Interface**: Press and hold button to record voice commands
- **Speech Recognition**: OpenAI Whisper API converts speech to text accurately
- **AI Responses**: GPT-4o mini generates intelligent, context-aware responses
- **Voice Synthesis**: Natural-sounding speech output via OpenAI TTS
- **Conversation Memory**: Maintains context between interactions using response api previous response field 
- **Real-Time Processing**: Optimized for minimal latency

## 🛠 Hardware Components

| Component | Model | Purpose |
|-----------|-------|---------|
| **Microcontroller** | ESP32-S3-DevKitC-1 N16R8 | Main processor with WiFi and 8MB PSRAM |
| **Microphone** | INMP441 MEMS | Digital I2S microphone for voice capture |
| **Amplifier** | MAX98357A | I2S audio amplifier for speaker output |
| **Speaker** | 2W 8Ω Speaker | Audio output |
| **Button** | Momentary Push Button | Push-to-talk activation |
| **Power Supply** | 5V USB | System power |

## 📐 Pin Connections

### INMP441 Microphone Connections

| INMP441 Pin | ESP32-S3 Pin | Function |
|-------------|--------------|----------|
| VDD | 3.3V | Power supply |
| GND | GND | Ground |
| SD | GPIO 4 | I2S Data |
| WS | GPIO 17 | Word Select |
| SCK | GPIO 18 | Serial Clock |
| L/R | GND | Left channel select |

### MAX98357A Amplifier Connections

| MAX98357A Pin | ESP32-S3 Pin | Function |
|---------------|--------------|----------|
| VIN | 5V | Power supply |
| GND | GND | Ground |
| DIN | GPIO 7 | I2S Data |
| BCLK | GPIO 6 | Bit Clock |
| LRC | GPIO 5 | Word Select |
| SD | GPIO 8 | Shutdown control (HIGH=enabled) |
| GAIN | Not connected | 15dB default gain |

### Push Button Connection

Connect a momentary push button between **GPIO 10** and **GND**. The internal pull-up resistor will be enabled in software.

## 💻 Software Requirements

### Development Environment
- **Arduino IDE** 2.x or **PlatformIO**
- **ESP32 Arduino Core** 2.0.14 or later

### Required Libraries

**Built-in Libraries (included with ESP32 Arduino Core):**
- WiFi & WiFiClientSecure - Network connectivity and HTTPS support
- HTTPClient - API communication
- driver/i2s_std.h - ESP-IDF I2S driver for audio
- Wire - I2C communication

**External Libraries to Install:**

1. **ESP32-audioI2S** by Schreibfaul1
   - Provides OpenAI TTS streaming support
   - Handles MP3/AAC/FLAC decoding automatically
   - Manages audio buffering and playback
   - Installation: Arduino Library Manager → Search "ESP32-audioI2S"
   - GitHub: https://github.com/schreibfaul1/ESP32-audioI2S

2. **ArduinoJson** (v6.x or v7.x)
   - JSON parsing for API responses
   - Efficient memory management
   - Installation: Arduino Library Manager → Search "ArduinoJson"

### OpenAI API Requirements

You'll need an OpenAI API key with access to:
- **Whisper API** - Speech-to-text transcription
- **GPT-4o mini** - Chat completions
- **TTS-1** - Text-to-speech synthesis
- **TTS-1-HD** - High-quality TTS (optional)

## 🚀 Installation

### Step 1: Hardware Assembly
1. Connect all components according to the pin diagrams above
2. Ensure proper power distribution (5V to amplifier, 3.3V to microphone)
3. Keep I2S wires short to minimize noise
4. Verify speaker impedance matches amplifier specs (8Ω recommended)

### Step 2: Arduino IDE Setup
1. Install ESP32 board support:
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board Manager → Search "ESP32" → Install

2. Select board and configure:
   - Board: "ESP32S3 Dev Module"
   - USB CDC On Boot: Enabled
   - USB Mode: Hardware CDC and JTAG
   - Flash Size: 16MB
   - Partition Scheme: 16MB Flash (3MB APP/9.9MB FATFS)
   - PSRAM: OPI PSRAM
   - Arduino Runs On: Core 1
   - Events Run On: Core 1

3. Install required libraries:
   - Tools → Manage Libraries
   - Search and install: "ESP32-audioI2S" and "ArduinoJson"

### Step 3: Project Configuration
Create a `config.h` file in your project folder with:
- WiFi network credentials (SSID and password)
- OpenAI API key
- Pin definitions for your hardware setup
- API endpoints and model configurations

### Step 4: Upload and Test
1. Connect ESP32-S3 via USB
2. Select correct COM port in Tools menu
3. Upload the sketch
4. Open Serial Monitor (115200 baud) to view debug output
5. Wait for "Ready" message
6. Press button to test recording and playback

## 📁 Project Structure

The project uses modular architecture with these components:
- **speaker_test/speaker_test.ino** - Test sketch for TTS playback
- **config.h** - Credentials and hardware configuration
- **FFat filesystem** - Local storage for downloaded TTS audio files

## 🎮 Usage

### Operation Flow (Speaker Test)

1. **Initialization**: Device connects to WiFi, syncs time via NTP, initializes FFat and audio
2. **TTS Request**: Sends test sentence to OpenAI TTS API
3. **Download**: TTS audio (MP3) downloads to FFat filesystem (~350KB)
4. **Playback**: Audio plays from local file through speaker
5. **Monitoring**: Loop tracks playback progress with periodic status updates

### LED Status Indicators (optional)

| State | Indicator |
|-------|-----------|
| Initializing | Rapid blink |
| Ready | Solid on |
| Recording | Slow pulse |
| Processing | Fast pulse |
| Playing | Solid on |
| Error | Triple blink |

## 💡 Implementation Details

### Why ESP32-audioI2S Library?

This library dramatically simplifies the project by providing:
- File-based audio playback via `audio.connecttoFS()` method
- Automatic format detection and decoding (MP3, AAC, FLAC, WAV)
- Built-in buffer management for smooth playback
- No manual PCM data handling required
- Dedicated audio task for smooth playback

**Note**: The speaker test uses file-based playback instead of streaming (`audio.openai_speech()`) for more reliable playback of large audio files. TTS audio is downloaded to FFat filesystem first, then played from the local file.

### Multi-Core Architecture

The system leverages both ESP32 cores for optimal performance:
- **Core 0**: Dedicated audio processing task
- **Core 1**: Main loop, WiFi operations, and API calls

This separation ensures smooth audio playback without network interruptions.

### Buffer Configuration

Proper buffer sizing prevents audio dropouts:
- **Input Buffer**: 64KB for file playback (larger for reliability)
- **FFat Storage**: 9.9MB available for audio files
- **DMA Buffers**: Managed by ESP32-audioI2S library

### Audio Processing

**Microphone Input (INMP441):**
- Captures 32-bit I2S data at 16kHz
- Converts to 16-bit PCM for smaller file size
- Formats as WAV with headers for Whisper API
- Uses custom MultiPartStream class to avoid memory issues

**Speaker Output (MAX98357A):**
- TTS audio downloads to FFat filesystem first (~350KB MP3)
- ESP32-audioI2S library plays from local file via `connecttoFS()`
- Supports multiple formats (MP3, AAC, Opus, FLAC)
- File-based playback eliminates network-related jitter
- Automatic resampling if needed

## 🔧 API Integration

### Whisper API
- Endpoint: `/v1/audio/transcriptions`
- Format: Multipart/form-data with WAV audio
- Model: "whisper-1"
- Response: JSON with transcribed text
- Max file size: 25MB

### Chat Completions API
- Endpoint: `/v1/chat/completions`
- Model: "gpt-4o-mini"
- Includes conversation history for context
- Temperature: 0.7 for balanced creativity
- Max tokens: 150 for concise responses

### Text-to-Speech API
- Endpoint: `/v1/audio/speech`
- Download approach: HTTPClient streams MP3 to FFat filesystem
- Models: "tts-1" (fast) or "tts-1-hd" (high quality)
- Voices: alloy, echo, fable, onyx, nova, shimmer
- Speed: 0.25 to 4.0 (1.0 = normal)
- Formats: MP3 (primary), Opus, AAC, FLAC
- Playback: ESP32-audioI2S `connecttoFS()` plays from local file
- File-based approach prevents race conditions with long audio

## 🐛 Troubleshooting

### Common Issues and Solutions

| Problem | Solution |
|---------|----------|
| **No WiFi Connection** | Verify credentials, check router settings, ensure 2.4GHz network |
| **Microphone Silent** | Check I2S connections, verify L/R pin grounded, test with oscilloscope |
| **No Audio Output** | Verify SD pin HIGH, check speaker connections, test amplifier power |
| **Distorted Audio** | Adjust GAIN jumper, verify speaker impedance, check power supply |
| **Audio Dropouts** | Increase buffer sizes, improve WiFi signal, enable PSRAM |
| **API Errors** | Verify API key, check usage limits, ensure time is synced |
| **Crackling Sound** | Shorten I2S wires, improve grounding, separate audio/power grounds |
| **Recording Cuts Off** | Increase recording buffer, check PSRAM allocation |

### Debug Tips

Enable verbose logging in Serial Monitor (115200 baud):
- Monitor WiFi connection status
- Check API response codes
- Verify audio buffer levels
- Track memory usage

### Performance Optimization

- **WiFi**: Disable power saving with `WiFi.setSleep(false)`
- **CPU**: Set frequency to 240MHz for maximum performance
- **Memory**: Use PSRAM for large buffers
- **Network**: Maintain persistent HTTPS connections where possible

## 🔒 Security Considerations

- Store API keys in separate config file (exclude from version control)
- Use HTTPS for all API communications
- Implement request timeout handling
- Consider adding local wake word detection for privacy
- Limit recording duration to prevent abuse

## 📊 Power Consumption

| State | Typical Current |
|-------|----------------|
| Idle | 80-100mA |
| WiFi Active | 150-200mA |
| Recording | 120-140mA |
| Processing | 160-180mA |
| Audio Playback | 200-400mA |

Peak consumption occurs during audio playback. Use adequate power supply (5V 1A minimum).

## 🔄 Future Enhancements

Potential improvements to consider:
- Local wake word detection
- Multiple language support
- Voice activity detection (VAD)
- Conversation history persistence
- Custom voice training
- Offline fallback mode
- Visual feedback via OLED display
- Multiple user profiles

## 📝 License

This project is released under the MIT License.

## 🤝 Contributing

Contributions welcome! Please submit pull requests or open issues for bugs and feature requests.

## 🔗 Resources

- [ESP32-S3 Technical Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [OpenAI API Documentation](https://platform.openai.com/docs/api-reference)
- [ESP32-audioI2S Library](https://github.com/schreibfaul1/ESP32-audioI2S)
- [I2S Audio on ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html)
- [INMP441 Datasheet](https://invensense.tdk.com/download-pdf/inmp441-datasheet/)
- [MAX98357A Datasheet](https://www.maximintegrated.com/en/products/analog/audio/MAX98357A.html)

## 📮 Support

For questions and support, please open an issue in the project repository.

---

**Note**: This project requires an active OpenAI API subscription. Usage will incur costs based on OpenAI's current pricing.