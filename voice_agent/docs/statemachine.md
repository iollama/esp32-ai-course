# ESP32 AI Voice Assistant - State Machine

This document explains the state machine architecture of the ESP32 AI Voice Assistant.

## State Diagram

```
                                    ┌─────────────┐
                                    │   POWER ON  │
                                    └──────┬──────┘
                                           │
                                           ▼
                               ┌───────────────────────┐
                               │  System Initialization │
                               │  - WiFi Connect        │
                               │  - NTP Sync           │
                               │  - I2S Mic Setup      │
                               │  - I2S Speaker Setup  │
                               └───────────┬───────────┘
                                           │
                                           ▼
                            ┌──────────────────────────────┐
                            │          IDLE STATE          │◄──────────┐
                            │  - Wait for button press     │           │
                            │  - LED indicator ready       │           │
                            └──────────┬───────────────────┘           │
                                       │                                │
                           Button Pressed (GPIO 10 LOW)                │
                                       │                                │
                                       ▼                                │
                        ┌─────────────────────────────┐                │
                        │    RECORDING STATE          │                │
                        │  - Capture I2S mic data     │                │
                        │  - Fill audioBuffer         │                │
                        │  - Monitor button release   │                │
                        │  - Max 10s, Min 0.5s        │                │
                        └──────┬──────────────────────┘                │
                               │                                        │
                    ┌──────────┴──────────┐                           │
                    │                     │                            │
        Button Released              Max Time (10s)                    │
        (Duration ≥ 0.5s)            Reached                           │
                    │                     │                            │
                    └──────────┬──────────┘                           │
                               │                                        │
                               ▼                                        │
                ┌──────────────────────────────┐                      │
                │  PROCESSING_WHISPER STATE    │                      │
                │  - Create WAV header         │                      │
                │  - POST to /v1/audio/        │                      │
                │    transcriptions            │                      │
                │  - Parse JSON response       │                      │
                │  - Extract text              │                      │
                └──────┬───────────────────────┘                      │
                       │                                               │
        ┌──────────────┴──────────────┐                              │
        │                              │                               │
   Success                         Failed                              │
   (Got text)                  (Network/API error)                     │
        │                              │                               │
        ▼                              │                               │
┌──────────────────────────────┐      │                               │
│  PROCESSING_CHAT STATE       │      │                               │
│  - Build Responses API JSON  │      │                               │
│  - Include previous_response │      │                               │
│    _id if exists             │      │                               │
│  - POST to /v1/responses     │      │                               │
│  - Parse output[0].content   │      │                               │
│    [0].text                  │      │                               │
│  - Store response ID         │      │                               │
└──────┬───────────────────────┘      │                               │
       │                              │                               │
┌──────┴──────────┐                   │                               │
│                 │                   │                               │
Success        Failed                 │                               │
(Got AI text)  (Network/API error)    │                               │
│                 │                   │                               │
│                 └───────────────────┼──────────┐                    │
│                                     │          │                    │
▼                                     ▼          ▼                    │
┌──────────────────────────────┐  ┌─────────────────────┐           │
│      SPEAKING STATE          │  │   ERROR_STATE       │           │
│  - audio.openai_speech()     │  │  - Display error    │           │
│  - Stream MP3 from TTS API   │  │  - Wait 3 seconds   │           │
│  - Decode MP3 to PCM         │  │  - Auto recovery    │           │
│  - Send to I2S speaker       │  └──────────┬──────────┘           │
│  - Call audio.loop() every   │             │                       │
│    10ms                      │          After 3s                   │
│  - Monitor audio.isRunning() │             │                       │
└──────┬───────────────────────┘             └───────────────────────┤
       │                                                              │
   Audio playback                                                     │
   complete                                                           │
   (audio.isRunning()                                                │
   == false)                                                          │
       │                                                              │
       ▼                                                              │
┌──────────────────┐                                                 │
│onSpeakingComplete│                                                 │
│  - Cleanup       │                                                 │
└──────┬───────────┘                                                 │
       │                                                              │
       └──────────────────────────────────────────────────────────────┘
```

## State Transitions Table

| From State | Trigger | To State | Code Location |
|------------|---------|----------|---------------|
| **IDLE** | Button pressed (GPIO 10 LOW) | **RECORDING** | `voice_agent.ino:326` |
| **RECORDING** | Button released (≥0.5s) | **PROCESSING_WHISPER** | `voice_agent.ino:365` |
| **RECORDING** | Max time (10s) reached | **PROCESSING_WHISPER** | `voice_agent.ino:381` |
| **RECORDING** | Button released (<0.5s) | **IDLE** | `voice_agent.ino:369` |
| **PROCESSING_WHISPER** | Transcription success | **PROCESSING_CHAT** | `voice_agent.ino:525` |
| **PROCESSING_WHISPER** | API/Network error | **ERROR_STATE** | `voice_agent.ino:909` |
| **PROCESSING_CHAT** | Response success | **SPEAKING** | `voice_agent.ino:771` |
| **PROCESSING_CHAT** | API/Network error | **ERROR_STATE** | `voice_agent.ino:909` |
| **SPEAKING** | Audio playback complete | **IDLE** | `voice_agent.ino:918` |
| **SPEAKING** | TTS API error | **ERROR_STATE** | `voice_agent.ino:909` |
| **ERROR_STATE** | After 3 seconds | **IDLE** | `voice_agent.ino:395` |

## State Descriptions

### IDLE State
- **Purpose**: System ready, waiting for user input
- **Entry Condition**: System initialization complete or previous interaction finished
- **Actions**:
  - Monitor button GPIO 10 (active LOW)
  - Display ready indicator
- **Exit Condition**: Button press detected

### RECORDING State
- **Purpose**: Capture audio from microphone
- **Entry Condition**: Button pressed
- **Actions**:
  - Start I2S microphone capture
  - Fill `audioBuffer` with samples from INMP441
  - Monitor button state (debounced)
  - Track recording duration
- **Constraints**:
  - Minimum duration: 0.5 seconds
  - Maximum duration: 10 seconds
  - Buffer size: 128KB (allows ~4 seconds at 16kHz)
- **Exit Conditions**:
  - Button released after minimum duration
  - Maximum time reached
  - Button released before minimum (returns to IDLE)

### PROCESSING_WHISPER State
- **Purpose**: Convert audio to text using OpenAI Whisper API
- **Entry Condition**: Valid audio recording captured
- **Actions**:
  - Create WAV header for recorded audio
  - Build multipart/form-data request
  - POST to `/v1/audio/transcriptions` endpoint
  - Parse JSON response
  - Extract transcribed text
- **Blocking**: Yes - waits for HTTP response
- **Exit Conditions**:
  - Success: Transcription text received → PROCESSING_CHAT
  - Failure: Network/API error → ERROR_STATE

### PROCESSING_CHAT State
- **Purpose**: Generate AI response using OpenAI Responses API
- **Entry Condition**: User text from Whisper received
- **Actions**:
  - Build Responses API request with nested JSON structure
  - Include `previous_response_id` for conversation context
  - Include system instructions
  - POST to `/v1/responses` endpoint
  - Parse `output[0].content[0].text` from response
  - Store response `id` for next turn
- **API Details**:
  - Model: `gpt-4.1-mini`
  - Max tokens: 100
  - Conversation history: Automatic via `previous_response_id` + `store: true`
- **Blocking**: Yes - waits for HTTP response
- **Exit Conditions**:
  - Success: AI response text received → SPEAKING
  - Failure: Network/API error → ERROR_STATE

### SPEAKING State
- **Purpose**: Play AI response through speaker
- **Entry Condition**: AI response text received
- **Actions**:
  - Trigger `audio.openai_speech()` (starts TTS streaming)
  - TTS API streams MP3 audio
  - ESP32-audioI2S library decodes MP3 to PCM in real-time
  - Call `audio.loop()` every ~10ms to feed I2S DMA buffers
  - Monitor `audio.isRunning()` status
- **Audio Details**:
  - TTS Model: `tts-1`
  - Voice: `alloy`
  - Format: MP3 (decoded to PCM)
  - Output: I2S to MAX98357A amplifier
  - Sample rate: 24kHz
- **Exit Conditions**:
  - Audio playback complete → IDLE
  - TTS API error → ERROR_STATE

### ERROR_STATE
- **Purpose**: Handle errors and recover gracefully
- **Entry Condition**: Any API or network failure
- **Actions**:
  - Display error message on serial
  - Wait 3 seconds (non-blocking)
  - Automatic recovery
- **Error Types**:
  - `ERROR_WIFI_DISCONNECTED` - WiFi connection lost
  - `ERROR_WHISPER_FAILED` - Transcription API failed
  - `ERROR_CHAT_FAILED` - Responses API failed
  - `ERROR_TTS_FAILED` - TTS API failed
  - `ERROR_RECORDING_FAILED` - I2S capture issue
- **Exit Condition**: After 3 second timeout → IDLE

## Main Loop Structure

```cpp
void loop() {
  switch (currentState) {
    case IDLE:
      handleIdle();           // Monitor button
      break;

    case RECORDING:
      handleRecording();      // Capture audio
      break;

    case PROCESSING_WHISPER:
    case PROCESSING_CHAT:
      // Blocking API calls, no loop processing needed
      break;

    case SPEAKING:
      audio.loop();           // REQUIRED - feeds audio data to I2S
      if (!audio.isRunning()) {
        onSpeakingComplete();
      }
      break;

    case ERROR_STATE:
      handleError();          // Auto-recovery after timeout
      break;
  }

  delay(10);  // Prevent watchdog issues
}
```

## Key Design Decisions

1. **Blocking API Calls**: PROCESSING_WHISPER and PROCESSING_CHAT states block the main loop while waiting for HTTP responses. This simplifies the code but means the device is unresponsive during API calls.

2. **Automatic Error Recovery**: All errors lead to ERROR_STATE which automatically recovers after 3 seconds, returning to IDLE. This prevents the device from getting stuck.

3. **Conversation Context**: The Responses API manages conversation history server-side using `previous_response_id`, eliminating the need to manually track message history.

4. **Real-Time Audio**: The SPEAKING state requires `audio.loop()` to be called frequently (every ~10ms) to maintain smooth audio playback without jitter.

5. **Debouncing**: Button state changes are debounced (50ms) to prevent spurious triggers from mechanical contact bounce.

## Common State Flows

### Successful Interaction
```
IDLE → RECORDING → PROCESSING_WHISPER → PROCESSING_CHAT → SPEAKING → IDLE
```

### Short Recording (ignored)
```
IDLE → RECORDING → IDLE
        (<0.5s)
```

### Transcription Failure
```
IDLE → RECORDING → PROCESSING_WHISPER → ERROR_STATE → IDLE
                         (API error)        (3s timeout)
```

### Network Issue During Chat
```
IDLE → RECORDING → PROCESSING_WHISPER → PROCESSING_CHAT → ERROR_STATE → IDLE
                      (success)              (API error)      (3s timeout)
```
