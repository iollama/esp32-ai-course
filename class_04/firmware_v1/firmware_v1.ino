#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Adafruit_NeoPixel.h>

const char* firmware_v1_url = "https://raw.githubusercontent.com/iollama/esp32-ai-course/main/class_04/firmware_v1/build/esp32.esp32.esp32s3/firmware_v1.ino.bin";
const char* firmware_v2_url = "https://raw.githubusercontent.com/iollama/esp32-ai-course/main/class_04/firmware_v2/build/esp32.esp32.esp32s3/firmware_v2.ino.bin";

const int BUTTON_PIN = 4;
const int RGB_LED_PIN = 48;  // onboard NeoPixel on ESP32-S3

Adafruit_NeoPixel pixel(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

#define VERSION 1

String readSerialLine() {
  String input = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (input.length() > 0) return input;
      } else {
        input += c;
      }
    }
  }
}

void performOTA(const char* url) {
  Serial.println("Starting OTA from: " + String(url));
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  Serial.println("HTTP response code: " + String(httpCode));

  if (httpCode == 200) {
    int contentLength = http.getSize();
    Serial.println("Content length: " + String(contentLength));

    if (contentLength <= 0) {
      Serial.println("Invalid content length!");
      return;
    }

    WiFiClient* stream = http.getStreamPtr();

    if (!Update.begin(contentLength)) {
      Serial.println("Not enough space for OTA!");
      Update.printError(Serial);
      return;
    }

    // Live download/write progress on serial
    Update.onProgress([](size_t done, size_t total) {
      int percent = (total > 0) ? (done * 100) / total : 0;
      Serial.println("Downloading: " + String(percent) + "% (" +
                     String(done) + " / " + String(total) + " bytes)");
    });

    size_t written = Update.writeStream(*stream);
    Serial.println("Written: " + String(written) + " / " + String(contentLength));

    if (Update.end()) {
      if (Update.isFinished()) {
        Serial.println("OTA Success! Rebooting...");
        ESP.restart();
      } else {
        Serial.println("OTA not finished!");
      }
    } else {
      Serial.print("OTA Error: ");
      Update.printError(Serial);
    }
  } else {
    Serial.println("HTTP Error: " + String(httpCode));
  }
  http.end();
}


void loop() {
  // Flash VERSION times: green for V1, red for V2
  uint32_t color = (VERSION == 1) ? pixel.Color(0, 255, 0)   // green
                                  : pixel.Color(255, 0, 0);  // red
  for (int i = 0; i < VERSION; i++) {
    pixel.setPixelColor(0, color);  // ON
    pixel.show();
    delay(100);
    pixel.clear();  // OFF
    pixel.show();
    delay(100);
  }

  // Pause between blink groups
  delay(700);

  // Check button
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      if (VERSION == 1) {
        Serial.println("Button pressed — downloading V2...");
        performOTA(firmware_v2_url);
      } else {
        Serial.println("Button pressed — downloading V1...");
        performOTA(firmware_v1_url);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(3000); 

  pixel.begin();
  pixel.setBrightness(50);
  pixel.clear();  // OFF
  pixel.show();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("OTA update demo");
  Serial.println("Running firmware VERSION: " + String(VERSION));

  Serial.println("Enter WiFi SSID:");
  String ssid = readSerialLine();

  Serial.println("Enter WiFi Password:");
  String password = readSerialLine();

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  Serial.println("\nPush the button to update");
}
