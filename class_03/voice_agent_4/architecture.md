# Voice Agent 4 — Architecture Document

ESP32-S3 push-to-talk voice assistant using the OpenAI Realtime API (speech-to-speech over secure WebSockets).

---

## System Overview

```
  User presses PTT button
         │
         ▼
  [I2S Mic DMA] ──► [Input Ring Buffer] ──► [WebSocket] ──► OpenAI Realtime API
                                                                      │
  [I2S Speaker DMA] ◄── [Output Ring Buffer] ◄── [WebSocket] ◄───────┘
```

Audio captured from microphone is streamed in real-time to OpenAI. The API responds with synthesized speech audio, which is buffered and played through the speaker. The entire pipeline is speech-to-speech with no local STT or TTS processing.

---

## Dual-Core FreeRTOS Architecture

The firmware splits responsibilities across both ESP32-S3 cores to keep audio hardware and network I/O from interfering with each other.

| Core | Task | Responsibilities |
|------|------|-----------------|
| Core 0 | `protocol_task` | WiFi, WSS connection, JSON parsing, Base64 encode/decode, sending/receiving audio data |
| Core 1 | `loop()` (Arduino main) | I2S mic DMA capture, I2S speaker DMA playback, PTT button debounce, jitter buffer management |

The two cores communicate exclusively through three shared primitives:
- `PSRAMRingBuffer* in_ring_buf` — mic audio from Core 1 → Core 0
- `PSRAMRingBuffer* out_ring_buf` — speaker audio from Core 0 → Core 1
- `volatile bool` flags: `cmd_cancel`, `cmd_commit`, `ws_response_done`

---

## State Machine

The assistant has a single global state `g_state` (declared `volatile`) that both cores read. Only designated code paths write it.

```
                    ┌─────────────────────────────┐
                    │                             │
              WiFi fails                    WS disconnects
                    │                             │
  Boot ──► STATE_WIFI_WAIT ──► STATE_WIFI_CONFIG  │
                    │                             │
              WiFi + WS OK                        │
                    │                             │
                    ▼                             │
          STATE_READY_FOR_INPUT ◄─────────────────┘
            (LED: green)
                    │
            PTT pressed
                    │
                    ▼
          STATE_RECORDING
            (LED: pink)
                    │
            PTT released
                    │
                    ▼
          STATE_THINKING
            (LED: blue)
                    │
         Jitter threshold met
         OR response.done + buffer > 0
                    │
                    ▼
          STATE_SPEAKING
            (LED: cyan)
                    │
         Buffer empty + response.done
                    │
                    └──► STATE_READY_FOR_INPUT
```

### State → LED Color

| State | LED Color | Meaning |
|-------|-----------|---------|
| `STATE_WIFI_WAIT` | Yellow | Connecting to WiFi / TLS |
| `STATE_WIFI_CONFIG` | Flashing yellow | AP mode, awaiting WiFi credentials |
| `STATE_READY_FOR_INPUT` | Green | Idle, ready for PTT |
| `STATE_RECORDING` | Pink | Mic active, streaming to OpenAI |
| `STATE_THINKING` | Blue | Waiting for response |
| `STATE_SPEAKING` | Cyan | Playing response audio |

---

## Hardware

### Microphone — INMP441 (I2S RX)

| INMP441 Pin | ESP32-S3 Pin | Note |
|-------------|-------------|------|
| SD (data) | GPIO 40 | I2S data in |
| WS | GPIO 41 | Word select / LRCLK |
| SCK | GPIO 42 | Bit clock |
| L/R | GND | Selects left channel |
| VDD | 3.3V | |
| GND | GND | |

I2S configuration: `I2S_NUM_0`, master RX, 16kHz, 32-bit samples (INMP441 output), left channel only. The 32-bit samples are right-shifted 16 bits in software to produce 16-bit PCM for transmission.

### Speaker Amplifier — MAX98357A (I2S TX)

| MAX98357A Pin | ESP32-S3 Pin | Note |
|--------------|-------------|------|
| DIN | GPIO 17 | I2S data out |
| BCLK | GPIO 47 | Bit clock |
| LRC | GPIO 21 | Word select / LRCLK |
| SD | 3.3V | Amp enabled, left channel |
| VIN | 5V | |
| GND | GND | |

I2S configuration: `I2S_NUM_1`, master TX, 24kHz, 16-bit samples, left channel only. `tx_desc_auto_clear = true` prevents DMA underrun noise.

### Other

| Component | GPIO |
|-----------|------|
| PTT Button | GPIO 1 (INPUT_PULLUP, active LOW) |
| RGB Status LED (NeoPixel) | GPIO 48 |
| Legacy single LED | GPIO 39 |

---

## Audio Pipeline Detail

### Recording (Core 1 → Core 0)

```
INMP441
  │ 32-bit I2S frames @ 16kHz
  ▼
i2s_read() [1024 byte chunks]
  │ Right-shift 16: int32 → int16 (PCM16)
  ▼
in_ring_buf (PSRAM, 64KB = 2s @ 16kHz 16-bit)
  │
  ▼ (Core 0 reads when STATE_RECORDING)
sendAudioChunk() — encodes 2048 bytes into s_b64_buf, builds JSON in s_ws_frame
  │ no heap allocation
  ▼
WebSocket: input_audio_buffer.append {audio: base64}
```

The mic DMA is always drained even when not recording to prevent stale audio accumulating in the DMA FIFO.

On PTT release, the remaining bytes in `in_ring_buf` are flushed in the same 2048-byte chunks via the same `sendAudioChunk()` path before sending `input_audio_buffer.commit`.

### Playback (Core 0 → Core 1)

```
WebSocket: response.audio.delta {delta: base64}
  │
  ▼ (Core 0 onMessage callback)
Base64 decode into s_decode_buf (PSRAM, 48KB pre-allocated) — no heap allocation
  │ raw PCM16 bytes
  ▼
out_ring_buf (PSRAM, 240KB = 5s @ 24kHz 16-bit)
  │
  ▼ (Core 1 loop, when STATE_THINKING or STATE_SPEAKING)
Jitter buffer gate: wait for 14,400 bytes (300ms)
  OR: if response.done and any bytes waiting → start immediately
  │
  ▼ (STATE_SPEAKING)
Volume scaling: sample[i] = sample[i] * g_volume / 100
  │
  ▼
i2s_write() [1024 byte chunks] → MAX98357A → speaker
```

**Jitter buffer:** The 300ms gate prevents choppy playback caused by network jitter — playback only begins once enough audio has buffered. For short responses that complete before the buffer fills, the `response.done` event bypasses the threshold and starts playback immediately with whatever is buffered.

**Barge-in:** When PTT is pressed while the assistant is speaking, `out_ring_buf` is cleared and `response.cancel` is sent to OpenAI, immediately stopping the current response. The microphone starts capturing the new input without delay.

---

## Protocol Task (Core 0)

`manage_websockets()` runs in a loop on Core 0 and handles the full WebSocket lifecycle.

### Connection Sequence

1. Wait for `WiFi.status() == WL_CONNECTED`
2. Connect via `wsClient.connectSecure("api.openai.com", 443, "/v1/realtime?model=gpt-4o-realtime-preview")`
3. Send `session.update` to configure the session:
   - Modalities: audio + text
   - Voice: alloy
   - Input/output format: pcm16
   - Turn detection: **null** (manual PTT, not VAD)
   - System instruction and temperature from NVS

### WebSocket Message Handling

Incoming messages are parsed with manual `String::indexOf()` rather than ArduinoJson for latency on the hot path:

| Event type | Action |
|-----------|--------|
| `response.audio.delta` | Extract `"delta"` field, Base64 decode, write to `out_ring_buf` |
| `response.audio.done` | Set `ws_response_done = true` |
| `response.done` | Set `ws_response_done = true` |
| `error` | Print to serial |
| anything else | Print to serial if verbose and payload < 500 bytes |

> **Important:** The audio data is in the `"delta"` field of `response.audio.delta` events, not a field called `"audio"`. This matches the pattern of text transcript delta events which also use `"delta"`.

### Outgoing Commands (polled every 5ms)

| Flag | Action |
|------|--------|
| `cmd_cancel = true` | Send `response.cancel`, clear `ws_response_done` |
| `STATE_RECORDING` and `in_ring_buf >= 2048 bytes` | Send `input_audio_buffer.append` chunk |
| `cmd_commit = true` | Flush remaining mic buffer, send `input_audio_buffer.commit`, send `response.create` |

---

## Configuration & Web UI

Configuration is stored in ESP32 NVS (Non-Volatile Storage) using the `Preferences` library and is editable via a built-in HTTP server on port 80.

| Setting | NVS Key | Default |
|---------|---------|---------|
| System prompt | `sysPrompt` | "You are a helpful voice assistant..." |
| Temperature | `temp` | 0.7 |
| Persist conversation | `persist` | true |
| Verbose logging | `verbose` | true |
| API key | `apiKey` | falls back to `config.h` |
| Volume | `volume` | 100 |
| WiFi SSID | `wifi-creds/ssid` | — |
| WiFi password | `wifi-creds/pass` | — |

### WiFi Setup (Captive Portal)

On first boot (no credentials in NVS), or after a failed connection, the device starts a soft AP named `VOICE-AGENT-XXYY` (last 2 bytes of MAC). A DNS captive portal redirects all traffic to the config page at `192.168.4.1`. Once credentials are saved, the device restarts and connects in STA mode.

After successful connection, the web UI is accessible at `http://voice-agent-XXYY.local` (mDNS) or via IP.

---

## Memory Layout

| Buffer | Location | Size | Purpose |
|--------|----------|------|---------|
| `in_ring_buf` | PSRAM | 64 KB | Mic audio staging (2s @ 16kHz 16-bit) |
| `out_ring_buf` | PSRAM | 240 KB | Speaker audio staging (5s @ 24kHz 16-bit) |
| `s_decode_buf` | PSRAM | 48 KB | Audio delta decode output (pre-allocated, reused each delta) |
| `s_mic_chunk` | Internal SRAM (.bss) | 2 KB | Mic DMA read staging, encode input |
| `s_b64_buf` | Internal SRAM (.bss) | 2.7 KB | Base64 encode output (ceil(2048×4/3)+4) |
| `s_ws_frame` | Internal SRAM (.bss) | 2.8 KB | JSON frame for audio append |
| I2S TX DMA | Internal SRAM | ~8 KB | Speaker DMA (8 buffers × 256 samples × 2 bytes) |
| I2S RX DMA | Internal SRAM | ~8 KB | Mic DMA (8 buffers × 256 samples × 4 bytes) |
| WebSocket payload (String) | Heap | up to ~32 KB | Audio delta JSON — allocated by ArduinoWebsockets, freed after callback |

All audio processing buffers are pre-allocated at boot. The only remaining heap activity during normal operation is inside `wsClient.send()` and `wsClient.poll()` (ArduinoWebsockets internals), which are short-lived and fixed-pattern.

PSRAM buffers are allocated via `ps_malloc()` to keep the internal heap free for the TLS stack and WebSocket library.

---

## Known Issues & Bugs Fixed

### 1. ArduinoWebsockets — ESP32 `setInsecure()` not propagated
**File:** `ArduinoWebsockets/src/websockets_client.cpp`

The `upgradeToSecuredConnection()` method had an `else { client->setInsecure(); }` fallback for ESP8266 but not ESP32. Without it, the internal `WiFiClientSecure` attempted certificate verification with no CA bundle, silently failing every WSS connection attempt.

See `LIBRARY_PATCHES.md` for the exact fix.

### 2. Audio delta field name mismatch
**File:** `voice_agent_4.ino`

The `response.audio.delta` WebSocket event carries audio in a field named `"delta"`, not `"audio"`. The original code searched for `"audio":"` and found nothing, leaving the output ring buffer permanently empty.

**Fix:** `payload.indexOf("\"audio\":\"")` → `payload.indexOf("\"delta\":\"")`

### 4. Heap fragmentation from repeated audio buffer malloc/free
**File:** `voice_agent_4.ino`

Every audio chunk sent (every ~130ms while recording) called `malloc(2048)` + `malloc(2733)` for encoding, and every incoming audio delta called `malloc(~12–24KB)` for decoding. Over time these variable-size allocations fragment the heap.

**Fix:** All audio processing buffers are now pre-allocated at boot:
- `s_mic_chunk[2048]` — static, replaces per-chunk malloc in recording loop
- `s_b64_buf[2736]` — static, replaces malloc inside `encodeBase64()`
- `s_ws_frame[2800]` — static, eliminates String concatenation for JSON frame
- `s_decode_buf` (48KB, PSRAM) — replaces per-delta malloc in onMessage

`encodeBase64()` was replaced by `sendAudioChunk()` which encodes directly into the static buffers. The commit flush was refactored to loop in 2048-byte chunks instead of allocating a single variable-size buffer for all remaining data.

### 3. Short response jitter buffer deadlock
**File:** `voice_agent_4.ino`

For short responses (< 300ms of audio), the jitter buffer threshold (14,400 bytes) was never reached. The `ws_response_done` flag was only checked inside the `STATE_SPEAKING` underrun handler, which was never entered. Audio sat in the buffer indefinitely.

**Fix:** Added a secondary condition to the jitter check: if `ws_response_done` is true and any bytes are waiting, start playback immediately regardless of threshold.
