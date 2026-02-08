#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// =================================================================================
//  TODO: UPDATE CREDENTIALS BELOW
//  Then, comment out or delete the #error line to allow compilation.
// =================================================================================

#error "STOP! You must update the WiFi SSID, Password, and API Key below. Then delete this line to compile."

// --- CONFIGURATION ---
const char* ssid          = "YOUR_WIFI_SSID";
const char* password      = "YOUR_WIFI_PASSWORD";
const char* apiKey        = "YOUR_OPENAI_API_KEY"; 
const char* system_prompt = "You are a helpful assistant that answers in rhyme.";
// ---------------------

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected. Please insert input.");
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      Serial.println("User: " + input);
      sendRequest(input);
    }
  }
}

void sendRequest(String text) {
  if (WiFi.status() == WL_CONNECTED) {
    
    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate verification
    client.setTimeout(20000); // 20s timeout

    HTTPClient http;
    
    if (http.begin(client, "https://api.openai.com/v1/chat/completions")) {

      http.addHeader("Content-Type", "application/json");
      http.addHeader("Authorization", "Bearer " + String(apiKey));

      // Escape quotes/newlines in user input to prevent JSON breakage
      text.replace("\"", "\\\""); 
      text.replace("\n", " "); 

      // Build JSON Payload nicely
      String payload = "{";
      payload += "  \"model\": \"gpt-4o-mini\",";
      payload += "  \"messages\": [";
      
      // --- System Prompt ---
      payload += "    {";
      payload += "      \"role\": \"system\",";
      payload += "      \"content\": \"" + String(system_prompt) + "\"";
      payload += "    },";
      
      // --- User Prompt ---
      payload += "    {";
      payload += "      \"role\": \"user\",";
      payload += "      \"content\": \"" + text + "\"";
      payload += "    }";
      
      payload += "  ],";
      payload += "  \"max_tokens\": 100";
      payload += "}";

      int httpCode = http.POST(payload);

      if (httpCode > 0) {
        String response = http.getString();
        
        if (httpCode == 200) {
          // Manual Parsing: Find the "content" field
          int contentIndex = response.indexOf("\"content\": \"");
          
          if (contentIndex != -1) {
            contentIndex += 12; // Skip: "content": "
            int endIndex = response.indexOf("\"", contentIndex);
            
            // Extract and clean up the response
            String chatResponse = response.substring(contentIndex, endIndex);
            chatResponse.replace("\\n", "\n");
            chatResponse.replace("\\\"", "\"");
            
            Serial.print("AI: ");
            Serial.println(chatResponse);
          } else {
             Serial.println("Error: Could not find content in response.");
          }
        } else {
          Serial.print("API Error: ");
          Serial.println(response);
        }
      } else {
        Serial.printf("POST failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      
      http.end();
    } else {
      Serial.println("Unable to connect to OpenAI");
    }
  } else {
    Serial.println("WiFi Disconnected");
  }
}