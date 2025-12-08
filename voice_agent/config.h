#ifndef CONFIG_H
#define CONFIG_H


#include "C:\Users\udi\keys.h"  
// =====================================
// WiFi Configuration
// =====================================
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

// =====================================
// OpenAI API Configuration
// =====================================
#ifndef OPENAI_API_KEY
#define OPENAI_API_KEY "YOUR_OPENAI_API_KEY"
#endif

// API Endpoints
#define OPENAI_API_HOST "api.openai.com"
#define WHISPER_ENDPOINT "/v1/audio/transcriptions"
#define CHAT_ENDPOINT "/v1/responses"
#define TTS_ENDPOINT "/v1/audio/speech"

// Model Configuration
#define WHISPER_MODEL "whisper-1"
#define WHISPER_LANGUAGE "en"  // ISO-639-1 language code (en, es, fr, de, etc.)
#define CHAT_MODEL "gpt-4.1-mini"
#define TTS_MODEL "tts-1"  // Options: "tts-1" (fast) or "tts-1-hd" (high quality)
#define TTS_VOICE "alloy"  // Options: alloy, echo, fable, onyx, nova, shimmer
#define TTS_SPEED 1.0      // Range: 0.25 to 4.0 (1.0 = normal)

// Chat API Settings
#define CHAT_INSTRUCTIONS "You are a helpful voice assistant. Keep responses concise and natural for speech."
#define CHAT_MAX_OUTPUT_TOKENS 20

// =====================================
// Hardware Pin Definitions
// =====================================

// INMP441 Microphone (I2S Input)
#define I2S_MIC_SD_PIN 4      // Serial Data
#define I2S_MIC_WS_PIN 17     // Word Select
#define I2S_MIC_SCK_PIN 18    // Serial Clock

// MAX98357A Amplifier (I2S Output)
#define I2S_SPK_DIN_PIN 7     // Data In
#define I2S_SPK_BCLK_PIN 6    // Bit Clock
#define I2S_SPK_LRC_PIN 5     // Left/Right Clock
#define I2S_SPK_SD_PIN 8      // Shutdown (HIGH=enabled)

// Push Button
#define BUTTON_PIN 10         // Connect between GPIO 10 and GND

// =====================================
// Audio Configuration
// =====================================

// Sample Rates
#define MIC_SAMPLE_RATE 16000    // 16kHz for Whisper API
#define SPK_SAMPLE_RATE 24000    // 24kHz for TTS output

// Buffer Sizes
#define INPUT_BUFFER_SIZE 131072     // 128KB for microphone capture (8 seconds at 16kHz)
#define OUTPUT_BUFFER_SIZE 262144    // 256KB for TTS streaming
#define DMA_BUFFER_COUNT 8
#define DMA_BUFFER_SIZE 1024

// Recording Settings
#define MAX_RECORDING_TIME_MS 10000  // 10 seconds maximum
#define MIN_RECORDING_TIME_MS 500    // 0.5 seconds minimum


// Volume for playback
#define PLAYBACK_VOLUME 10 //0-21, 15 = ~70% 
// =====================================
// System Configuration
// =====================================

// Serial Debug
#define SERIAL_BAUD_RATE 115200

// NTP Time Sync (required for HTTPS)
#define NTP_SERVER "time.google.com"
#define GMT_OFFSET_SEC 0
#define DAYLIGHT_OFFSET_SEC 0

// WiFi Settings
#define WIFI_TIMEOUT_MS 20000        // 20 seconds
#define WIFI_RETRY_DELAY_MS 1000     // 1 second between retries

// API Timeout
#define API_TIMEOUT_MS 30000         // 30 seconds

// =====================================
// Performance Tuning
// =====================================

// CPU Frequency (automatically set by Arduino IDE)
// Recommended: 240MHz for maximum performance

// WiFi Power Management
#define WIFI_SLEEP_DISABLED true     // Disable WiFi sleep for better performance

// Task Priority (if using multi-core)
#define AUDIO_TASK_PRIORITY 1
#define AUDIO_TASK_CORE 0            // Core 0 for audio processing

// =====================================
// Feature Flags
// =====================================

// Enable/disable features
#define ENABLE_SERIAL_DEBUG true
#define ENABLE_CONVERSATION_HISTORY true
#define MAX_CONVERSATION_MESSAGES 10  // Keep last N messages for context

#endif // CONFIG_H
