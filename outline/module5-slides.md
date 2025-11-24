# Module 5: Building a Full Voice Chat with LLM

**Goal**: Create a complete voice assistant solution

## Project Components:
* I2S microphone integration
* Speaker/amplifier (e.g., MAX98357A)
* Speech-to-text processing
* LLM integration for conversation
* Text-to-speech output
* Optional: Wake word detection

## Final Outcome:
A fully functional voice-activated AI assistant running on ESP32

---

## Slide Deck

---

**Slide 1: Module 5 - Voice Chat with LLM**
- Title slide
- Building a complete voice assistant

**Slide 2: Project Vision**
- Speak to your ESP32
- Voice is transcribed to text
- Sent to ChatGPT/OpenAI
- Response converted to speech
- Played through speaker
- Natural conversation experience

**Slide 3: System Architecture**
```
[Microphone] -> [ESP32] -> [Whisper API] -> [Text]
                   ↓                          ↓
              [Speaker] <- [TTS API] <- [ChatGPT API]
```
- Complete audio pipeline
- Cloud-based processing
- Real-time interaction

**Slide 4: Required Hardware**
- ESP32 board (preferably ESP32-S3 for better audio)
- I2S MEMS microphone (e.g., INMP441, ICS-43434)
- I2S amplifier + speaker (e.g., MAX98357A)
- Push button (for trigger/PTT)
- LED indicators (status feedback)
- Power supply (5V/2A recommended)

**Slide 5: I2S Protocol Overview**
- Inter-IC Sound
- Serial bus for digital audio
- 3 main signals:
  - SCK (Serial Clock)
  - WS (Word Select / LRCLK)
  - SD (Serial Data)
- High quality digital audio
- ESP32 has built-in I2S support

**Slide 6: I2S Microphone Pinout**
- Common INMP441 connections:
  - VDD -> 3.3V
  - GND -> GND
  - SD -> GPIO (data in)
  - WS -> GPIO (word select)
  - SCK -> GPIO (clock)
  - L/R -> GND (left channel) or 3.3V (right)

**Slide 7: I2S Speaker/Amplifier Pinout**
- MAX98357A connections:
  - VIN -> 5V
  - GND -> GND
  - DIN -> GPIO (data)
  - BCLK -> GPIO (bit clock)
  - LRC -> GPIO (left/right clock)
  - Speaker terminals to 4-8Ω speaker

**Slide 8: Recommended GPIO Pins**
```
Microphone (Input):
- I2S_WS: GPIO 25
- I2S_SCK: GPIO 26
- I2S_SD: GPIO 33

Speaker (Output):
- I2S_WS: GPIO 15
- I2S_BCK: GPIO 14
- I2S_DOUT: GPIO 22

Button: GPIO 13
Status LED: GPIO 2
```

**Slide 9: I2S Library Setup**
```cpp
#include <driver/i2s.h>

#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 26
#define SAMPLE_RATE 16000

i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  // ... more config
};
```

**Slide 10: Recording Audio**
- Record when button pressed
- Fixed duration (e.g., 5 seconds)
- Or voice activity detection
- Store in buffer
- Convert to WAV format
- Send to Whisper API

**Slide 11: Audio Recording Flow**
1. User presses button
2. LED indicates recording
3. Capture audio samples via I2S
4. Store in buffer (5 seconds = ~80KB @ 16kHz)
5. Convert to WAV format
6. Base64 encode (for HTTP multipart)
7. Send to Whisper API

**Slide 12: WAV File Format**
- Header (44 bytes)
  - File size
  - Sample rate
  - Channels
  - Bit depth
- Audio data (PCM samples)
- ESP32 must generate WAV header
- Required for Whisper API

**Slide 13: Creating WAV Header**
```cpp
void createWavHeader(byte* header, int wavSize) {
  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  unsigned int fileSize = wavSize + 44 - 8;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  // ... rest of header
}
```

**Slide 14: Sending Audio to Whisper**
- Endpoint: `/v1/audio/transcriptions`
- HTTP POST with multipart/form-data
- Fields:
  - file: audio WAV data
  - model: "whisper-1"
  - language: "en" (optional)
- Response: JSON with transcribed text

**Slide 15: HTTP Multipart Form Data**
```cpp
String boundary = "----ESP32Boundary";
client.println("POST /v1/audio/transcriptions HTTP/1.1");
client.println("Host: api.openai.com");
client.println("Authorization: Bearer " + apiKey);
client.println("Content-Type: multipart/form-data; boundary=" + boundary);
// ... send file data
```

**Slide 16: Processing Whisper Response**
```cpp
// Response JSON:
{
  "text": "Hello, how are you today?"
}

// Extract text:
StaticJsonDocument<1024> doc;
deserializeJson(doc, response);
String transcribedText = doc["text"];
Serial.println("You said: " + transcribedText);
```

**Slide 17: Sending to ChatGPT**
- Use transcribed text as user input
- Same as Module 4 (text chat)
- Add to conversation history
- Send to chat completions API
- Receive text response
- Now need to convert to speech!

**Slide 18: Text-to-Speech Request**
```cpp
// Endpoint: /v1/audio/speech
doc.clear();
doc["model"] = "tts-1";
doc["input"] = aiResponseText;
doc["voice"] = "alloy"; // or nova, shimmer, etc.
doc["response_format"] = "mp3";

// Response is binary MP3 data
```

**Slide 19: Handling MP3 Audio**
- OpenAI returns MP3 format
- ESP32 needs to decode MP3
- Use ESP32-audioI2S library
- Or request different format (opus, pcm)
- PCM is easiest but larger
- MP3 saves bandwidth

**Slide 20: Installing ESP32-audioI2S Library**
- Library by schreibfaul1
- Supports MP3, AAC, WAV, and more
- Simple API for I2S playback
- Install from Library Manager
```cpp
#include "Audio.h"

Audio audio;
audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
audio.connecttospeech(mp3Data, "en");
```

**Slide 21: Playing TTS Audio**
```cpp
// Simpler approach: request PCM16
doc["response_format"] = "pcm";

// Receive raw PCM data
// Write directly to I2S output
size_t bytes_written;
i2s_write(I2S_NUM_0, pcmData, dataSize, &bytes_written, portMAX_DELAY);
```

**Slide 22: Complete Conversation Flow**
1. Button press detected
2. LED: Recording
3. Capture 5 seconds of audio
4. LED: Processing (blinking)
5. Send to Whisper (speech-to-text)
6. Send text to ChatGPT
7. Receive AI response text
8. Send to TTS API
9. LED: Speaking (different color/pattern)
10. Play audio through speaker
11. LED: Ready (solid)

**Slide 23: State Machine Design**
```cpp
enum State {
  IDLE,
  RECORDING,
  TRANSCRIBING,
  THINKING,
  SPEAKING
};

State currentState = IDLE;

void loop() {
  switch(currentState) {
    case IDLE: checkButton(); break;
    case RECORDING: recordAudio(); break;
    case TRANSCRIBING: sendToWhisper(); break;
    // ... etc
  }
}
```

**Slide 24: Memory Management**
- Audio buffers are large (80KB+)
- MP3 data can be large
- Use PSRAM if available (ESP32-S3)
- Stream audio instead of buffering
- Release memory after each stage
- Monitor heap usage

**Slide 25: Using PSRAM**
```cpp
// ESP32-S3 has PSRAM (up to 8MB)
#ifdef BOARD_HAS_PSRAM
  byte* audioBuffer = (byte*)ps_malloc(BUFFER_SIZE);
#else
  byte* audioBuffer = (byte*)malloc(BUFFER_SIZE);
#endif

// Check available PSRAM
Serial.println("PSRAM: " + String(ESP.getPsramSize()));
```

**Slide 26: Voice Activity Detection (VAD)**
- Optional enhancement
- Detect when user starts/stops speaking
- Automatic recording start/stop
- Better UX than button press
- Libraries: webrtc-vad, simple threshold
- Requires tuning for environment

**Slide 27: Wake Word Detection**
- Advanced feature
- "Hey Assistant" triggers recording
- Edge Impulse for custom wake words
- TensorFlow Lite Micro on ESP32
- Runs on-device (no cloud)
- Battery impact consideration

**Slide 28: Power Optimization**
- Voice assistants can drain power
- Use deep sleep when idle
- Wake on button interrupt
- Disable WiFi when not in use
- Adjust microphone gain
- Consider battery capacity

**Slide 29: Error Handling**
- Audio recording failures
- Network timeouts (longer for audio)
- API errors (file too large, unsupported format)
- Playback failures
- Provide audio feedback (beeps)
- LED error indicators

**Slide 30: Audio Feedback Cues**
- Recording start: Single beep
- Recording end: Double beep
- Processing: Triple beep
- Error: Long low tone
- Enhances user experience
- Can use buzzer or speaker

**Slide 31: Multi-language Support**
- Whisper supports 50+ languages
- Specify language in API call
- Or use auto-detect
- TTS supports many languages
- Match TTS language to detected language
- Unicode text handling

**Slide 32: Latency Optimization**
- Current flow has 3 API calls (slow)
- Optimize:
  - Use faster models (gpt-3.5-turbo)
  - Reduce audio length
  - Stream TTS responses
  - Keep WiFi connected
  - Reuse HTTP connections
- Typical latency: 5-10 seconds

**Slide 33: Cost Considerations**
- Whisper: $0.006/minute
- GPT-3.5-turbo: $0.0015/1K tokens
- TTS: $0.015/1K characters
- Example conversation:
  - 5 sec audio = ~$0.0005
  - Response = ~$0.0003
  - TTS = ~$0.001
  - Total: ~$0.002 per interaction

**Slide 34: Alternative: Local TTS**
- ESP32-compatible TTS engines
- Lower quality than OpenAI
- No API costs
- No internet required
- Examples:
  - eSpeak (port available)
  - SAM (Software Automatic Mouth)
  - Trade-off: quality vs cost/latency

**Slide 35: Alternative: Local Speech Recognition**
- Edge Impulse audio classification
- Limited vocabulary
- No API costs
- Lower latency
- Good for simple commands
- Not suitable for conversation

**Slide 36: Hands-on Exercise Part 1**
- Wire up I2S microphone
- Test audio recording
- Verify audio quality
- Display waveform data via Serial
- Record and save to SD card (optional)

**Slide 37: Hands-on Exercise Part 2**
- Wire up I2S speaker/amplifier
- Test audio playback
- Play sample WAV file
- Adjust volume
- Test different audio formats

**Slide 38: Hands-on Exercise Part 3**
- Implement Whisper integration
- Record voice and transcribe
- Display transcribed text
- Test accuracy with different accents
- Handle errors gracefully

**Slide 39: Hands-on Exercise Part 4**
- Integrate TTS
- Convert text to speech
- Play through speaker
- Test different voices
- Combine with ChatGPT responses

**Slide 40: Final Integration**
- Combine all components
- Implement state machine
- Add LED indicators
- Test complete conversation flow
- Debug and optimize
- Polish user experience

**Slide 41: Testing Scenarios**
- Simple questions
- Long responses (memory test)
- Background noise
- Different speaking volumes
- Network interruptions
- Error recovery
- Continuous conversations

**Slide 42: Enclosure Design**
- 3D printed case
- Microphone placement (away from speaker)
- Speaker positioning
- Button accessibility
- LED visibility
- Ventilation for heat
- Cable management

**Slide 43: Troubleshooting Audio Issues**
- No audio input: Check I2S pins, mic power
- Distorted input: Adjust gain, check sample rate
- No audio output: Check amp power, speaker wiring
- Garbled output: Verify I2S config, bit depth
- Use oscilloscope or logic analyzer
- Test with known-good audio files

**Slide 44: Advanced Features**
- Conversation history persistence
- User identification (voice print)
- Multi-turn conversations
- Context awareness
- Integration with smart home
- Custom skills/commands

**Slide 45: Integration Ideas**
- Control smart home devices
- Weather updates
- Calendar/reminders
- Language translation
- Music player control
- IoT sensor readings
- Custom automation

**Slide 46: Project Showcase**
- Demo completed projects
- Share challenges and solutions
- Discuss customizations
- Creative use cases
- Community sharing

**Slide 47: Next Steps and Resources**
- OpenAI documentation
- ESP32 audio examples
- Community forums
- GitHub repositories
- Further learning:
  - Edge AI on ESP32
  - Custom wake words
  - Local LLM inference

**Slide 48: Course Summary**
- Module 1: ESP32 fundamentals
- Module 2: ESP32 ecosystem
- Module 3: Understanding LLMs and APIs
- Module 4: Text-based AI chat
- Module 5: Full voice assistant
- You've built a complete AI-powered device!

**Slide 49: Beyond This Course**
- Contribute to open source
- Share your projects
- Build commercial products
- Explore edge AI
- Join maker communities
- Keep learning and building!

**Slide 50: Thank You!**
- Congratulations on completing the course!
- You now have skills to build AI-powered IoT devices
- Questions and discussion
- Stay in touch
- Course resources and code available
