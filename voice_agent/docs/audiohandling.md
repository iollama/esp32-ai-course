# Audio Handling and Playback

This document explains how audio playback works in the ESP32 AI Voice Assistant and why audio jitter/stuttering can occur.

## Audio Playback Process

### Hardware Setup

**Speaker Hardware**: MAX98357A I2S Amplifier
- Uses **I2S_NUM_1** (second I2S port on ESP32-S3)
- Pin Configuration (from `config.h`):
  - `I2S_SPK_DIN_PIN` (GPIO 7) - Data Input
  - `I2S_SPK_BCLK_PIN` (GPIO 6) - Bit Clock
  - `I2S_SPK_LRC_PIN` (GPIO 5) - Left/Right Clock
  - `I2S_SPK_SD_PIN` (GPIO 8) - Shutdown control (HIGH=enabled)

**Note**: The microphone (INMP441) uses I2S_NUM_0, while the speaker uses I2S_NUM_1. The ESP32-S3 has two independent I2S ports, allowing simultaneous operation.

### Software Components

**Audio Library**: ESP32-audioI2S library
```cpp
Audio audio(I2S_NUM_1);  // Global audio object for speaker
```

This library handles:
- HTTPS connections to OpenAI TTS API
- MP3 streaming and buffering
- Real-time MP3 decoding to PCM
- I2S DMA buffer management
- Audio playback control

### Step-by-Step Audio Flow

#### 1. Initialization (`setup()`)
```cpp
audio.setPinout(I2S_SPK_BCLK_PIN, I2S_SPK_LRC_PIN, I2S_SPK_DIN_PIN);
audio.setVolume(15);  // 0-21, 15 = ~70%
```

The audio library configures:
- I2S hardware interface
- DMA buffers for audio data
- Volume control
- Pin assignments

#### 2. Trigger TTS Request (`speakResponse()`)
```cpp
void speakResponse(String text) {
  currentState = SPEAKING;

  // Audio library handles EVERYTHING internally:
  // - Makes HTTPS request to OpenAI TTS API
  // - Receives MP3 stream
  // - Decodes MP3 in real-time
  // - Sends PCM audio to MAX98357A via I2S
  bool success = audio.openai_speech(
    OPENAI_API_KEY,
    TTS_MODEL,        // "tts-1"
    text.c_str(),     // Text to speak
    "",               // Language (blank = auto)
    TTS_VOICE,        // "alloy"
    "mp3",            // Format
    String(TTS_SPEED) // 1.0 = normal speed
  );
}
```

**What `audio.openai_speech()` does internally:**
1. Builds HTTPS POST request to `/v1/audio/speech`
2. Streams MP3 response from OpenAI servers
3. Decodes MP3 to PCM audio on-the-fly
4. Buffers decoded audio in RAM
5. Sends PCM samples to I2S DMA buffers
6. I2S hardware automatically clocks data to MAX98357A amplifier

#### 3. Continuous Playback (`loop()`)
```cpp
case SPEAKING:
  audio.loop();  // REQUIRED - feeds audio data to I2S
  if (!audio.isRunning()) {
    onSpeakingComplete();  // Done speaking
  }
  break;
```

**What `audio.loop()` does:**
- Fetches more MP3 data from network if needed
- Decodes MP3 chunks to PCM
- Fills I2S DMA buffers with PCM samples
- Must be called frequently (every ~10ms)

**Critical**: If `audio.loop()` is not called regularly, the I2S buffers will run dry, causing audio jitter.

#### 4. Completion
When audio finishes:
- `audio.isRunning()` returns `false`
- `onSpeakingComplete()` is called
- State changes back to `IDLE`

### Complete Audio Flow Diagram

```
User speaks → Whisper API → getChatResponse() → Responses API
                                                      ↓
                                              "Hello, how are you?"
                                                      ↓
                                            speakResponse(text)
                                                      ↓
                                         audio.openai_speech()
                                                      ↓
                ┌─────────────────────────────────────┴────────────────┐
                ↓                                                       ↓
    HTTPS to OpenAI TTS API                                    State = SPEAKING
    POST /v1/audio/speech                                             ↓
                ↓                                              Main loop calls
         MP3 stream received                                   audio.loop()
                ↓                                                      ↓
         Decode MP3 → PCM                                    Feeds I2S buffers
                ↓                                                      ↓
         I2S_NUM_1 DMA buffers                              While audio.isRunning()
                ↓                                                      ↓
         MAX98357A amplifier                                When done → IDLE
                ↓
            Speaker 🔊
```

### Key Points

✅ **Zero manual I2S management** - The Audio library handles everything
✅ **Streaming playback** - Audio plays while still downloading (low latency)
✅ **MP3 decoding** - Happens in real-time on ESP32
✅ **Non-blocking** - Uses callbacks and state machine
✅ **Automatic buffering** - Library manages DMA buffers

---

## Why Audio Jitters - Student Explanation

### The Core Problem: Timing Mismatch

Think of audio playback like pouring water from a bucket (network download) into a glass (speaker playback):
- The **speaker drinks** at a constant rate (24,000 sips per second)
- The **network pours** at a variable rate (sometimes fast, sometimes slow)
- If pouring is too slow → glass runs dry → **jitter/silence/stuttering**

### What's Happening in the Voice Assistant

```
OpenAI TTS Server → WiFi → ESP32 → MP3 Decoder → I2S Speaker
     (varies)      (varies)  (buffer)   (CPU)    (constant 24kHz)
```

**The speaker NEVER stops asking for data:**
- Every 1/24000th of a second, the MAX98357A amplifier requests the next sample
- I2S DMA buffers feed the speaker continuously
- If the buffer is empty → **JITTER** (silence, pops, or repeating previous sample)

---

## Three Main Causes of Audio Jitter

### Cause A: Network Hiccups (Most Common)

**What happens:**
```
Time:    0ms   50ms  100ms  150ms  200ms  250ms  300ms
WiFi:    ████  ████  ░░░░   ░░░░   ████  ████   ████
Speaker: ████  ████  ████   ████   ████  ████   ████
                      ↑
                   JITTER! Buffer ran out
```

WiFi data arrival is **bursty**, not steady:
- WiFi packets arrive in bursts, not as a steady stream
- WiFi interference (2.4GHz: microwaves, Bluetooth, other networks)
- Distance from router affects signal strength
- Router congestion from other devices

**Teaching Analogy**: Like watching Netflix with bad internet - it buffers, plays, buffers, stutters.

**Real-world factors:**
- Neighbor streaming video
- Microwave oven running
- Distance from WiFi router
- Number of devices on network
- Thick walls between ESP32 and router

---

### Cause B: CPU Overload

**What the ESP32 is juggling:**
```cpp
while (SPEAKING) {
  audio.loop();  // Must run every ~10ms!
  // Doing:
  //  1. Fetch MP3 chunks from network
  //  2. Decode MP3 to PCM (CPU intensive!)
  //  3. Fill I2S DMA buffers
  //  4. Manage WiFi stack
  delay(10);
}
```

**Problem**: If `audio.loop()` is called too infrequently:
- MP3 decoder runs out of data to feed I2S
- DMA buffer underrun → jitter

**Why it might be slow:**
1. **MP3 decoding** uses significant CPU (real-time decompression)
2. **WiFi processing** steals CPU time
3. Main loop has long delays
4. Other background tasks competing for CPU

**Teaching Analogy**: A chef trying to cook AND serve simultaneously - if cooking takes too long, customers wait hungry (jitter).

**ESP32 Resource Competition:**
```
Core 0: WiFi RX → MP3 Decode → Buffer Fill
           ↓          ↓            ↓
       All compete for same CPU cycles
```

---

### Cause C: Small Buffer Size

From `config.h`:
```cpp
#define OUTPUT_BUFFER_SIZE 262144    // 256KB
#define DMA_BUFFER_COUNT 8
#define DMA_BUFFER_SIZE 1024
```

**DMA buffers are small:**
- 8 buffers × 1024 bytes = 8KB total DMA buffer
- At 24kHz 16-bit stereo: ~85ms of audio buffered
- If network pauses for >85ms → **jitter**

**Teaching Analogy:**
- Big buffer = large water tank = can handle supply interruptions
- Small buffer = shot glass = runs dry quickly with any pause

**Why small buffers?**
- Limited RAM (ESP32-S3 has 512KB, shared with code and other data)
- Trade-off: Smaller buffer = lower latency (faster response), but more sensitive to interruptions

---

## Why This Design is Vulnerable

### Current Architecture:
```
[OpenAI Server] --WiFi--> [ESP32 Tiny Buffer] --> [Speaker (never stops)]
                            256KB RAM               24,000 samples/sec
                              ↑
                         Bottleneck!
```

### The Challenges:

**A. No Pre-Buffering**
```cpp
audio.openai_speech(...);  // Starts playing IMMEDIATELY
// Doesn't wait to download entire MP3 first
```
- Starts speaking as soon as first bytes arrive
- **Pro**: Low latency ✅ (user hears response quickly)
- **Con**: Vulnerable to network jitter ❌

**B. Real-Time Constraint**
- Speaker needs 24,000 samples/sec = 48,000 bytes/sec (for 16-bit stereo)
- MP3 stream is typically 64-128 kbps compressed
- WiFi must sustain ~8-16 KB/sec minimum
- Any hiccup → audible glitch

**C. Single-Core Bottleneck**
All processing happens sequentially on one CPU core:
1. Receive WiFi data
2. Decode MP3
3. Fill buffers
4. Repeat

If any step takes too long → buffer underrun → jitter

---

## Visual Examples for Students

### Smooth Playback (No Jitter):
```
Network: ████████████████████████████████
Buffer:  ████▒▒▒▒████▒▒▒▒████▒▒▒▒████▒▒▒▒
Speaker: ████████████████████████████████
         Buffer never runs dry!
```

### Jittery Playback:
```
Network: ████░░░░░░░░████░░░░░░░░████░░░░
Buffer:  ████▒░░░░░░░████▒░░░░░░░████▒░░░
Speaker: ████░░░░░░░░████░░░░░░░░████░░░░
            ↑  JITTER!  ↑  JITTER!  ↑
```
Legend:
- █ = Full buffer/data flowing
- ▒ = Half-full buffer
- ░ = Empty buffer/silence

---

## Configuration Settings

### Current Settings from `config.h`:

```cpp
// Buffer Sizes
#define OUTPUT_BUFFER_SIZE 262144    // 256KB for TTS streaming
#define DMA_BUFFER_COUNT 8           // Number of DMA buffers
#define DMA_BUFFER_SIZE 1024         // Size of each DMA buffer

// Volume for playback
#define PLAYBACK_VOLUME 10           // 0-21, 15 = ~70%
```

**Buffer Analysis:**
- **Total DMA buffer**: 8 × 1024 = 8KB
- **At 24kHz stereo 16-bit**: 8192 bytes ÷ (24000 samples/sec × 4 bytes/sample) = ~85ms
- **Implication**: System can only tolerate network pauses <85ms before jitter occurs

---

## Teaching Demonstrations

### 1. Water Analogy Demo
**Materials**: Cup, water pitcher, volunteer

**Steps:**
1. **Person 1** (network): Pour water into cup at variable rates
2. **Person 2** (speaker): Drink from cup at constant rate
3. **Person 1** stops pouring randomly → **Person 2** runs out → **JITTER!**

**Lesson**: Real-time audio requires constant data flow

### 2. Code Demonstration

**Add delay to cause jitter:**
```cpp
case SPEAKING:
  delay(500);        // ARTIFICIALLY SLOW - causes jitter
  audio.loop();
  if (!audio.isRunning()) {
    onSpeakingComplete();
  }
  break;
```
Students will hear severe jitter/stuttering.

**Move ESP32 far from router:**
- Show good playback when close to router
- Show jitter when far from router
- Demonstrates WiFi impact on audio quality

### 3. Monitor Network with Serial Output

Add debug output to see network behavior:
```cpp
// In audio.loop() or similar location
static unsigned long lastPrint = 0;
if (millis() - lastPrint > 100) {
  Serial.printf("Buffer level: %d bytes\n", getBufferLevel());
  lastPrint = millis();
}
```

Students can observe buffer levels dropping during jitter.

---

## Teaching Key Concepts

### Concept 1: Real-Time Constraints
**Definition**: System must process data at a rate that keeps up with external timing requirements (speaker playback rate).

**In this system:**
- External constraint: 24,000 samples/second (fixed by hardware)
- Processing must keep up with this rate
- Falling behind = audible artifacts

### Concept 2: Buffering Trade-offs
| Buffer Size | Latency | Jitter Resistance | RAM Usage |
|-------------|---------|-------------------|-----------|
| Small (8KB) | Low ✅   | Low ❌            | Low ✅     |
| Large (128KB)| High ❌  | High ✅           | High ❌    |

This system chooses: **Low latency + Low RAM** at the cost of jitter susceptibility.

### Concept 3: Network Unpredictability
WiFi is **not deterministic**:
- Packet delivery time varies
- Interference is random
- Congestion is dynamic

**Lesson**: Real-time systems on unreliable networks need careful design!

---

## Advanced Topics

### Why Not Pre-Buffer the Entire Audio?

**Problem with full pre-buffering:**
1. TTS response can be 10-30 seconds of audio
2. At 128 kbps MP3: 30 seconds = ~480KB
3. ESP32-S3 has only 512KB RAM total
4. Would need to wait 3-5 seconds before starting playback (high latency)

**Current approach trades lower latency for jitter risk.**

### Could We Use Both CPU Cores?

ESP32-S3 has 2 cores, but:
- Core 0: WiFi stack (required by hardware)
- Core 1: Available for application
- MP3 decoding could move to Core 1
- However, ESP32-audioI2S library doesn't currently support this

**Future improvement possibility!**

### How Professional Systems Handle This

Commercial devices (Alexa, Google Home) use:
1. **Larger RAM** (>1GB) - can buffer 10+ seconds
2. **Pre-buffering** - wait before starting playback
3. **Adaptive bitrate** - lower quality when network is poor
4. **Wired Ethernet** - more reliable than WiFi
5. **Dedicated audio DSPs** - offload CPU

**Lesson**: Embedded systems have different constraints than consumer devices!

---

## Summary for Students

**Why audio jitters in this system:**

1. ⏱️ **Timing Critical** - Speaker never waits, must have data ready
2. 📡 **Unreliable Network** - WiFi delivery is variable
3. 💾 **Limited Buffers** - Small buffers can't absorb network pauses
4. 🔌 **Single Core** - CPU must juggle networking + decoding + playback
5. ⚡ **Low Latency Design** - Starts playing immediately (no pre-buffer)

**Trade-offs made:**
- ✅ Fast response (low latency)
- ✅ Low memory usage
- ❌ Sensitive to network jitter
- ❌ No quality adaptation

**This demonstrates fundamental embedded systems challenges: real-time constraints, resource limitations, and network unpredictability!**
