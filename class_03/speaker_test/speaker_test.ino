/*
 * ESP32-S3 Speaker Test (Offline Tone Generation)
 *
 * This sketch tests the MAX98357A amplifier and speaker by generating tones directly
 * using the I2S peripheral. It does not require a WiFi connection.
 *
 * Hardware:
 * - ESP32-S3 or similar
 * - MAX98357A I2S Amplifier
 * - Speaker
 *
 * Pinout:
 * - DIN  -> GPIO 17
 * - BCLK -> GPIO 47
 * - LRC  -> GPIO 21
 * - VIN  -> 5V
 * - GND  -> GND
 * - SD   -> 3V
 * - GAIN -> not connected
 */

#include <Arduino.h>
#include "driver/i2s.h"
#include <math.h>

// I2S pin configuration based on user request
#define I2S_DIN_PIN   17 // DOUT
#define I2S_BCLK_PIN  47 // BCLK
#define I2S_LRC_PIN   21 // LRC

// I2S settings
#define I2S_PORT_NUMBER I2S_NUM_0
#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT

// Tone generation settings
#define TONE_FREQ_1 440.0 // A4 note
#define TONE_FREQ_2 880.0 // A5 note
#define DURATION_MS 500
#define AMPLITUDE ((1 << (16 - 1)) - 1) // Max amplitude for 16-bit signed audio

void play_tone(float frequency, int duration_ms);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=================================");
  Serial.println("ESP32 I2S Offline Speaker Test");
  Serial.println("=================================\n");
  
  Serial.println("Configuring I2S...");

  // I2S configuration
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo format
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  // I2S pin configuration
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num = I2S_LRC_PIN,
    .data_out_num = I2S_DIN_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE // Not using I2S microphone
  };

  // Install and start I2S driver
  esp_err_t err = i2s_driver_install(I2S_PORT_NUMBER, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %d\n", err);
    while(1);
  }

  err = i2s_set_pin(I2S_PORT_NUMBER, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %d\n", err);
    while(1);
  }
  
  Serial.println("✓ I2S driver installed and pins set successfully.");
  Serial.printf("  - BCLK: %d\n", I2S_BCLK_PIN);
  Serial.printf("  - LRC:  %d\n", I2S_LRC_PIN);
  Serial.printf("  - DIN:  %d\n", I2S_DIN_PIN);
  Serial.println();
}

void loop() {
  play_tone(TONE_FREQ_1, DURATION_MS);
  delay(500); // Pause between tones
  play_tone(TONE_FREQ_2, DURATION_MS);
  delay(1000); // Longer pause after sequence
}

/**
 * @brief Generates a sine wave tone and plays it over I2S.
 * 
 * @param frequency The frequency of the tone in Hz.
 * @param duration_ms The duration of the tone in milliseconds.
 */
void play_tone(float frequency, int duration_ms) {
    int num_samples = (int)(SAMPLE_RATE * duration_ms / 1000.0);
    // Each sample is 16-bit and there are 2 channels (stereo)
    size_t buffer_size = num_samples * 2 * sizeof(int16_t);
    int16_t* samples_buffer = (int16_t*)malloc(buffer_size);

    if (!samples_buffer) {
        Serial.println("✗ Failed to allocate memory for audio buffer");
        return;
    }

    Serial.printf("Playing tone: %d Hz for %d ms...\n", (int)frequency, duration_ms);

    // Generate sine wave samples
    for (int i = 0; i < num_samples; i++) {
        float angle = 2.0 * M_PI * frequency * i / SAMPLE_RATE;
        int16_t sample = (int16_t)(AMPLITUDE * sin(angle));
        
        // Write the same sample to both left and right channels for mono effect
        samples_buffer[i * 2] = sample;     // Left channel
        samples_buffer[i * 2 + 1] = sample; // Right channel
    }

    // Write samples to I2S
    size_t bytes_written = 0;
    esp_err_t err = i2s_write(I2S_PORT_NUMBER, samples_buffer, buffer_size, &bytes_written, portMAX_DELAY);

    if (err != ESP_OK) {
        Serial.printf("✗ I2S write error: %d\n", err);
    }
    if (bytes_written < buffer_size) {
        Serial.printf("✗ I2S write timed out. Wrote %d of %d bytes.\n", bytes_written, buffer_size);
    }

    free(samples_buffer);
}