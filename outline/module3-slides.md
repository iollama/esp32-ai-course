# Module 3: Introduction to LLMs

**Goal**: Bridge the knowledge gap between hardware and AI services

## Topics:
* What is REST API? (HTTP request-response and JSON)
* Calling OpenAI endpoints
* Working with Postman (visual, non-coding experimentation)
* Using different models (GPT-4, GPT-4o, TTS, and others)
* System prompts and prompt engineering basics

**Key Skill**: Understanding HTTP/REST, JSON data structures, and API authentication before coding

---

## Slide Deck

---

**Slide 1: Module 3 - Introduction to LLMs**
- Title slide
- Bridging hardware and AI services

**Slide 2: What is an API?**
- Application Programming Interface
- A way for programs to communicate
- Like a menu at a restaurant:
  - You request something (order)
  - The kitchen processes it
  - You receive a response (food)

**Slide 3: REST API Basics**
- REST = Representational State Transfer
- Uses HTTP protocol (same as web browsers)
- Stateless communication
- Standard methods:
  - GET (retrieve data)
  - POST (send/create data)
  - PUT (update data)
  - DELETE (remove data)

**Slide 4: HTTP Request Structure**
- URL/Endpoint (where to send request)
- Method (GET, POST, etc.)
- Headers (metadata, authentication)
- Body (data being sent, usually in POST)
- Example: `POST https://api.openai.com/v1/chat/completions`

**Slide 5: HTTP Response Structure**
- Status Code (200 = success, 404 = not found, 500 = server error)
- Headers (response metadata)
- Body (the actual data returned)
- Common status codes:
  - 200: OK
  - 401: Unauthorized
  - 429: Too many requests
  - 500: Server error

**Slide 6: What is JSON?**
- JavaScript Object Notation
- Human-readable data format
- Key-value pairs
- Common in APIs
- Example:
```json
{
  "name": "ESP32",
  "temperature": 25.5,
  "sensors": ["DHT22", "BME280"]
}
```

**Slide 7: JSON Data Types**
- String: `"hello"`
- Number: `42` or `3.14`
- Boolean: `true` or `false`
- Array: `[1, 2, 3]`
- Object: `{"key": "value"}`
- Null: `null`

**Slide 8: API Authentication**
- Why authenticate?
  - Security
  - Usage tracking
  - Rate limiting
  - Billing
- Common methods:
  - API Keys
  - Bearer Tokens
  - OAuth

**Slide 9: OpenAI API Overview**
- Cloud-based AI services
- Access to powerful language models
- Pay-per-use pricing
- Multiple endpoints:
  - Chat completions
  - Text-to-speech (TTS)
  - Speech-to-text (STT/Whisper)
  - Image generation (DALL-E)

**Slide 10: Getting Started with OpenAI**
- Create account at platform.openai.com
- Generate API key
- Secure storage of API keys (never commit to GitHub!)
- Understanding pricing and rate limits
- Monitoring usage dashboard

**Slide 11: Introduction to Postman**
- API development and testing tool
- Visual interface for API calls
- No coding required
- Features:
  - Request builder
  - Response viewer
  - Environment variables
  - Request collections

**Slide 12: Postman Hands-on Setup**
- Install Postman (desktop or web)
- Create new request
- Set request method (POST)
- Enter OpenAI endpoint URL
- Add headers (Authorization, Content-Type)
- Build JSON request body

**Slide 13: Your First OpenAI API Call**
- Endpoint: `https://api.openai.com/v1/chat/completions`
- Method: POST
- Headers:
  - `Authorization: Bearer YOUR_API_KEY`
  - `Content-Type: application/json`
- Body:
```json
{
  "model": "gpt-4",
  "messages": [{"role": "user", "content": "Hello!"}]
}
```

**Slide 14: OpenAI Models Overview**
- GPT-4o (latest, fast, multimodal)
- GPT-4 (powerful, more expensive)
- GPT-4-turbo (faster, cheaper than GPT-4)
- GPT-3.5-turbo (fast, cost-effective)
- Differences:
  - Capability vs speed vs cost
  - Context window size
  - Knowledge cutoff dates

**Slide 15: Chat Completions API**
- Most common endpoint
- Conversational interface
- Messages array with roles:
  - `system`: Sets behavior/context
  - `user`: User input
  - `assistant`: AI responses
- Maintains conversation history

**Slide 16: System Prompts**
- Sets the AI's behavior and personality
- Defines expertise and tone
- Provides context and constraints
- Example:
```json
{
  "role": "system",
  "content": "You are a helpful IoT assistant specializing in ESP32 development."
}
```

**Slide 17: Prompt Engineering Basics**
- Clear and specific instructions
- Provide context and examples
- Set constraints and format requirements
- Iterative refinement
- Tips:
  - Be explicit
  - Use examples (few-shot learning)
  - Specify output format
  - Test and refine

**Slide 18: Good vs Bad Prompts**
- Bad: "Tell me about sensors"
- Good: "List 3 temperature sensors compatible with ESP32, including their accuracy and price range. Format as JSON."
- Bad: "Make it funny"
- Good: "Respond in a friendly, encouraging tone suitable for beginners learning electronics."

**Slide 19: Text-to-Speech (TTS) API**
- Endpoint: `/v1/audio/speech`
- Converts text to spoken audio
- Multiple voices available (alloy, echo, fable, onyx, nova, shimmer)
- Output formats: mp3, opus, aac, flac
- Speed control (0.25 to 4.0)

**Slide 20: TTS Example Request**
```json
{
  "model": "tts-1",
  "input": "Hello from ESP32!",
  "voice": "alloy"
}
```
- Returns audio file
- Can be played directly on ESP32 with I2S

**Slide 21: Speech-to-Text (Whisper) API**
- Endpoint: `/v1/audio/transcriptions`
- Converts audio to text
- Supports many languages
- High accuracy
- Input: audio file (mp3, wav, etc.)
- Output: transcribed text

**Slide 22: Whisper Example Request**
- Multipart form data (not JSON)
- Fields:
  - `file`: audio file
  - `model`: "whisper-1"
  - `language`: optional (e.g., "en")
  - `response_format`: "text" or "json"

**Slide 23: API Response Structure**
- Chat completion response:
```json
{
  "id": "chatcmpl-123",
  "choices": [{
    "message": {
      "role": "assistant",
      "content": "Hello! How can I help?"
    },
    "finish_reason": "stop"
  }],
  "usage": {
    "prompt_tokens": 10,
    "completion_tokens": 20,
    "total_tokens": 30
  }
}
```

**Slide 24: Understanding Tokens**
- Tokens are pieces of words
- Roughly 4 characters = 1 token
- Pricing based on tokens
- Context limits in tokens
- Both input and output count
- Monitor usage to control costs

**Slide 25: Error Handling**
- Common errors:
  - 401: Invalid API key
  - 429: Rate limit exceeded
  - 500: OpenAI server error
- Check status codes
- Read error messages
- Implement retry logic
- Graceful degradation

**Slide 26: Rate Limits and Best Practices**
- Understand your tier limits
- Implement exponential backoff
- Cache responses when possible
- Stream responses for long outputs
- Monitor usage dashboard
- Set spending limits

**Slide 27: Hands-on Exercise with Postman**
- Create chat completion request
- Experiment with different models
- Test system prompts
- Try TTS with different voices
- Analyze response structure
- Handle errors intentionally (wrong API key)

**Slide 28: Preparing for ESP32 Integration**
- All these concepts will apply to ESP32
- HTTP requests from ESP32
- JSON parsing libraries (ArduinoJson)
- Memory considerations
- Handling asynchronous operations
- Next module: Putting it all together!

**Slide 29: Summary**
- REST APIs use HTTP for communication
- JSON is the standard data format
- OpenAI provides powerful AI services
- Postman helps test without coding
- System prompts guide AI behavior
- Multiple models for different needs
- Understanding tokens and rate limits is crucial
