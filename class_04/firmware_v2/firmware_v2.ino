#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

const char* firmware_v1_url = "https://raw.githubusercontent.com/iollama/esp32-ai-course/main/class_04/firmware_v1/build/esp32.esp32.esp32c3/firmware_v1.ino.bin";
const char* firmware_v2_url = "https://raw.githubusercontent.com/iollama/esp32-ai-course/main/class_04/firmware_v2/build/esp32.esp32.esp32c3/firmware_v2.ino.bin";

const int BUTTON_PIN = 4;
const int LED_PIN = 8;

#define VERSION 2

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
  // Blink VERSION times
  for (int i = 0; i < VERSION; i++) {
    digitalWrite(LED_PIN, LOW);  // ON (active LOW)
    delay(100);
    digitalWrite(LED_PIN, HIGH);  // OFF
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

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, HIGH);  // OFF

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
