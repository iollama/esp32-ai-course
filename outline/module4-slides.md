# Module 4: Working with LLM and ESP32

**Goal**: Combine ESP32 hardware with LLM capabilities

## Project:
Implement a console-based LLM chat
* WiFi connectivity on ESP32
* Making API calls from ESP32
* Parsing JSON responses
* Serial Monitor-based text chat interface

---

## Slide Deck

---

**Slide 1: Module 4 - Working with LLM and ESP32**
- Title slide
- Bringing AI to embedded systems

**Slide 2: Module Goals**
- Connect ESP32 to WiFi
- Make HTTPS requests to OpenAI
- Parse JSON responses
- Build interactive chat via Serial Monitor
- Handle errors and edge cases
- Manage memory efficiently

**Slide 3: Project Architecture**
- User input via Serial Monitor
- ESP32 sends request to OpenAI API
- Receives and parses JSON response
- Displays AI response in Serial Monitor
- Maintains conversation context
- Error handling and reconnection

**Slide 4: Required Libraries**
- WiFi.h (ESP32 built-in)
- HTTPClient.h (HTTP/HTTPS requests)
- ArduinoJson.h (JSON parsing)
- WiFiClientSecure.h (HTTPS/SSL)
- Installation via Arduino Library Manager

**Slide 5: WiFi Connection Basics**
```cpp
#include <WiFi.h>

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

void setup() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected!");
}
```

**Slide 6: WiFi Best Practices**
- Store credentials securely (separate config file)
- Implement connection timeout
- Handle disconnection and reconnection
- Check signal strength
- Use static IP for faster connection (optional)
- Display connection status via LED

**Slide 7: HTTPS vs HTTP**
- OpenAI requires HTTPS (secure)
- ESP32 supports SSL/TLS
- Certificate validation (optional but recommended)
- Higher memory usage
- Slightly slower than HTTP
- Essential for API key security

**Slide 8: Making HTTPS Requests**
```cpp
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

WiFiClientSecure client;
HTTPClient https;

client.setInsecure(); // Skip cert validation
https.begin(client, "https://api.openai.com/v1/chat/completions");
https.addHeader("Content-Type", "application/json");
https.addHeader("Authorization", "Bearer " + apiKey);
```

**Slide 9: Building the JSON Request**
```cpp
#include <ArduinoJson.h>

StaticJsonDocument<1024> doc;
doc["model"] = "gpt-3.5-turbo";

JsonArray messages = doc.createNestedArray("messages");
JsonObject msg = messages.createNestedObject();
msg["role"] = "user";
msg["content"] = userInput;

String requestBody;
serializeJson(doc, requestBody);
```

**Slide 10: Sending the Request**
```cpp
int httpCode = https.POST(requestBody);

if (httpCode == HTTP_CODE_OK) {
  String response = https.getString();
  // Parse response
} else {
  Serial.println("Error: " + String(httpCode));
}

https.end();
```

**Slide 11: Parsing JSON Response**
```cpp
StaticJsonDocument<2048> responseDoc;
deserializeJson(responseDoc, response);

const char* aiResponse = responseDoc["choices"][0]["message"]["content"];

Serial.println("AI: " + String(aiResponse));
```

**Slide 12: Memory Management**
- ESP32 has limited RAM (520KB)
- JSON documents use dynamic memory
- Use StaticJsonDocument for predictable size
- Use DynamicJsonDocument for variable size
- Calculate required size: https://arduinojson.org/v6/assistant/
- Monitor free heap: `ESP.getFreeHeap()`

**Slide 13: Memory Optimization Tips**
- Use smaller models (gpt-3.5-turbo vs gpt-4)
- Limit conversation history
- Use streaming responses
- Release memory after parsing
- Avoid String concatenation in loops
- Use F() macro for string literals

**Slide 14: Reading Serial Input**
```cpp
void loop() {
  if (Serial.available()) {
    String userInput = Serial.readStringUntil('\n');
    userInput.trim();

    if (userInput.length() > 0) {
      sendToOpenAI(userInput);
    }
  }
}
```

**Slide 15: Conversation History**
- Store multiple messages for context
- Array of message objects
- Limit to last N messages
- More context = better responses
- More context = higher costs
- Balance quality vs cost

**Slide 16: Implementing Conversation History**
```cpp
#define MAX_HISTORY 5
String conversationHistory[MAX_HISTORY * 2]; // user + assistant pairs
int historyIndex = 0;

void addToHistory(String role, String content) {
  conversationHistory[historyIndex++] = role;
  conversationHistory[historyIndex++] = content;

  // Remove oldest if full
  if (historyIndex >= MAX_HISTORY * 2) {
    // Shift array or circular buffer
  }
}
```

**Slide 17: Error Handling**
- Network errors (connection lost)
- HTTP errors (401, 429, 500)
- JSON parsing errors
- Out of memory errors
- Timeout handling
- User feedback for all error states

**Slide 18: Error Handling Example**
```cpp
if (httpCode == 401) {
  Serial.println("Invalid API key!");
} else if (httpCode == 429) {
  Serial.println("Rate limit exceeded. Wait...");
  delay(5000);
} else if (httpCode == 500) {
  Serial.println("OpenAI server error. Retry...");
}
```

**Slide 19: Adding Visual Feedback**
- LED indicators:
  - Solid: WiFi connected
  - Blinking: Processing request
  - Fast blink: Error
- Serial Monitor formatting
- Progress indicators
- Status messages

**Slide 20: System Prompt Implementation**
```cpp
void buildRequest(String userInput) {
  doc.clear();
  doc["model"] = "gpt-3.5-turbo";

  JsonArray messages = doc.createNestedArray("messages");

  // System prompt
  JsonObject sysMsg = messages.createNestedObject();
  sysMsg["role"] = "system";
  sysMsg["content"] = "You are a helpful ESP32 assistant. Keep responses brief.";

  // User message
  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  userMsg["content"] = userInput;
}
```

**Slide 21: Advanced Features**
- Temperature parameter (creativity control)
- Max tokens (response length limit)
- Streaming responses (real-time output)
- Multiple choice responses
- Function calling (tool use)
- Fine-tuned models

**Slide 22: Temperature Parameter**
```cpp
doc["temperature"] = 0.7; // 0.0 to 2.0
```
- 0.0: Deterministic, focused
- 0.7: Balanced (default)
- 1.5+: Creative, random
- Use lower for factual responses
- Use higher for creative tasks

**Slide 23: Max Tokens Parameter**
```cpp
doc["max_tokens"] = 150; // Limit response length
```
- Controls response length
- Saves costs
- Prevents memory issues
- 1 token ≈ 4 characters
- Plan for input + output tokens

**Slide 24: Streaming Responses**
- Receive response word-by-word
- Better user experience
- Lower perceived latency
- More complex to implement
- Requires SSE (Server-Sent Events) handling
- Advanced topic for later

**Slide 25: Code Structure Best Practices**
- Separate functions for each task
- WiFi connection function
- API request function
- JSON parsing function
- Input handling function
- Clean, readable code
- Comments for clarity

**Slide 26: Testing Strategy**
- Test WiFi connection first
- Test simple API call
- Test JSON parsing separately
- Test error conditions
- Test memory usage
- Monitor Serial output
- Check OpenAI usage dashboard

**Slide 27: Hands-on Exercise**
- Connect ESP32 to WiFi
- Implement basic chat function
- Add conversation history
- Implement system prompt
- Add error handling
- Test various scenarios
- Monitor memory usage

**Slide 28: Debugging Tips**
- Use Serial.println() liberally
- Print HTTP response codes
- Print raw JSON responses
- Check WiFi connection status
- Monitor free heap memory
- Use ArduinoJson error messages
- Test with smaller inputs first

**Slide 29: Common Issues and Solutions**
- "Out of memory": Reduce buffer sizes, limit history
- "Connection failed": Check WiFi credentials, router
- "401 Unauthorized": Verify API key
- "Parsing failed": Check JSON document size
- "Timeout": Increase timeout value, check internet

**Slide 30: Performance Optimization**
- Use connection keep-alive
- Cache WiFi connection
- Reuse JSON documents
- Minimize Serial.print calls
- Use efficient string handling
- Profile memory usage
- Consider dual-core tasking

**Slide 31: Next Steps Preview**
- Current: Text-based chat
- Next module: Voice interface
- Add microphone for input
- Add speaker for output
- Speech-to-text integration
- Text-to-speech integration
- Full voice assistant!

**Slide 32: Summary**
- ESP32 can communicate with OpenAI API
- HTTPClient for HTTPS requests
- ArduinoJson for JSON handling
- Memory management is crucial
- Error handling improves reliability
- Conversation history adds context
- Foundation ready for voice interface
