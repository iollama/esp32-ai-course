/*
 * INMP441 Microphone Test Program
 *
 * This program tests the INMP441 I2S microphone by continuously capturing
 * audio and displaying real-time audio levels on the serial monitor.
 *
 * Hardware:
 * - ESP32 development board
 * - INMP441 I2S microphone
 *
 * INMP441 Microphone (I2S RX)
 *   - SD   -> GPIO 40
 *   - VDD  -> 3V
 *   - GND  -> GND
 *   - L/R  -> GND (left channel)
 *   - WS   -> GPIO 41
 *   - SCK  -> GPIO 42
 * 
 *   - if you have an amplifier connected, connect the SD (shutdown) to ground
 */

#include <driver/i2s.h>

// INMP441 Microphone (I2S Input)
#define I2S_MIC_SD_PIN 40      // Serial Data
#define I2S_MIC_WS_PIN 41     // Word Select
#define I2S_MIC_SCK_PIN 42    // Serial Clock



// Sample Rates
#define MIC_SAMPLE_RATE 16000    // 16kHz for Whisper API
#define SPK_SAMPLE_RATE 24000    // 24kHz for TTS output

// Buffer Sizes
#define INPUT_BUFFER_SIZE 131072     // 128KB for microphone capture (8 seconds at 16kHz)
#define OUTPUT_BUFFER_SIZE 262144    // 256KB for TTS streaming
#define DMA_BUFFER_COUNT 8
#define DMA_BUFFER_SIZE 1024

// Serial Debug
#define SERIAL_BAUD_RATE 115200



// I2S port number
#define I2S_PORT I2S_NUM_0

// Audio processing parameters
#define SAMPLES_PER_READ 512
#define UPDATE_INTERVAL_MS 100

// Global variables
int32_t samples[SAMPLES_PER_READ];
size_t bytes_read = 0;
unsigned long last_update = 0;
bool diagnostic_printed = false;

void setup() {
  // Initialize serial communication
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  Serial.println("\n\n=================================");
  Serial.println("INMP441 Microphone Test");
  Serial.println("=================================\n");

  // Configure I2S for microphone input
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = MIC_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  // Changed: Try both channels
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUFFER_COUNT,
    .dma_buf_len = DMA_BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  // Configure I2S pins
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_MIC_SCK_PIN,
    .ws_io_num = I2S_MIC_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD_PIN
  };

  // Install and start I2S driver
  Serial.println("Initializing I2S driver...");
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %d\n", err);
    while (1) delay(1000);
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
    while (1) delay(1000);
  }

  // Start I2S
  i2s_start(I2S_PORT);

  Serial.println("I2S driver initialized successfully!");
  Serial.printf("Sample Rate: %d Hz\n", MIC_SAMPLE_RATE);
  Serial.printf("Microphone Pins - SD:%d WS:%d SCK:%d\n\n",
                I2S_MIC_SD_PIN, I2S_MIC_WS_PIN, I2S_MIC_SCK_PIN);

  Serial.println("Starting audio capture...");
  Serial.println("Speak into the microphone to see audio levels.\n");

  delay(500);

  // Clear any initial garbage data
  i2s_read(I2S_PORT, &samples, sizeof(samples), &bytes_read, portMAX_DELAY);

  Serial.println("=== DIAGNOSTIC MODE ===");
  Serial.println("Reading first 10 raw samples to check data...\n");
}

void loop() {
  // Read audio samples from I2S
  esp_err_t result = i2s_read(I2S_PORT, &samples, sizeof(samples), &bytes_read, portMAX_DELAY);

  if (result != ESP_OK) {
    Serial.printf("I2S read error: %d\n", result);
    delay(100);
    return;
  }

  // Print diagnostic info once
  if (!diagnostic_printed) {
    int samples_count = bytes_read / sizeof(int32_t);
    Serial.printf("Bytes read: %d, Samples: %d\n", bytes_read, samples_count);
    Serial.println("First 10 raw samples:");
    for (int i = 0; i < 10 && i < samples_count; i++) {
      Serial.printf("  [%d] Raw: 0x%08X (%d), Shifted>>8: %d\n",
                    i, samples[i], samples[i], samples[i] >> 8);
    }
    Serial.println("\n=== STARTING CONTINUOUS MONITORING ===\n");
    diagnostic_printed = true;
  }

  // Calculate audio levels every UPDATE_INTERVAL_MS
  unsigned long current_time = millis();
  if (current_time - last_update >= UPDATE_INTERVAL_MS) {
    last_update = current_time;

    // Calculate RMS and peak levels
    int32_t peak = 0;
    int64_t sum_squares = 0;
    int samples_count = bytes_read / sizeof(int32_t);

    for (int i = 0; i < samples_count; i++) {
      // INMP441 outputs 24-bit data in 32-bit frames, shift to get meaningful values
      int32_t sample = samples[i] >> 8;

      // Calculate absolute value for peak detection
      int32_t abs_sample = abs(sample);
      if (abs_sample > peak) {
        peak = abs_sample;
      }

      // Accumulate for RMS calculation
      sum_squares += (int64_t)sample * (int64_t)sample;
    }

    // Calculate RMS (Root Mean Square)
    float rms = sqrt((float)sum_squares / samples_count);

    // Normalize to percentage (adjust scaling as needed)
    // Max 24-bit value is 8,388,607, but typical speech is much lower
    // Scale to make typical values more visible (using 500,000 as "100%")
    float peak_percent = (float)peak / 500000.0 * 100.0;
    float rms_percent = rms / 100000.0 * 100.0;

    // Display results
    displayAudioLevels(peak, rms, peak_percent, rms_percent, samples_count);
  }
}

void displayAudioLevels(int32_t peak, float rms, float peak_percent, float rms_percent, int samples_count) {
  // Clear line and move cursor to beginning
  Serial.print("\r");

  // Display numeric values
  Serial.printf("Peak: %7d (%5.1f%%) | RMS: %7.0f (%5.1f%%) | ",
                peak, peak_percent, rms, rms_percent);

  // Create visual level meter (40 characters wide)
  int bar_length = (int)(rms_percent * 0.4); // Scale to 40 chars max
  if (bar_length > 40) bar_length = 40;

  Serial.print("[");
  for (int i = 0; i < 40; i++) {
    if (i < bar_length) {
      if (i < 20) {
        Serial.print("=");  // Green zone
      } else if (i < 32) {
        Serial.print("*");  // Yellow zone
      } else {
        Serial.print("!");  // Red zone
      }
    } else {
      Serial.print(" ");
    }
  }
  Serial.print("]");

  // Add status indicator
  if (rms_percent > 2.0) {
    Serial.print(" SOUND");
  } else if (rms_percent > 0.5) {
    Serial.print(" noise");
  } else {
    Serial.print(" quiet");
  }
  Serial.println();
  // Flush to ensure immediate display
  Serial.flush();
}
