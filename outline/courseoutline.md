# ESP32 and AI Course Outline

## Target Audience
Arduino-familiar makers who are new to ESP32 boards and want to integrate AI/LLM capabilities.

## Course Overview
This course teaches participants how to build AI-powered voice assistants using ESP32 microcontrollers, progressing from basic ESP32 concepts to a complete voice-activated LLM chat system.

---

## Module 1: ESP32 Introduction
**Goal**: Introduce ESP32 hardware and establish foundational programming skills

### Topics:
* What is ESP32
* Types of ESP32 boards
* Comparison to Arduino (dual-core processor, WiFi/BT built-in, more GPIO, faster processor)
* Development environment setup

### Hands-on Exercise:
A game with LEDs and one button to provide immediate hands-on experience with ESP32 programming

### Slide Deck (Detailed):

---

#### Slide 1: Module 1 - ESP32 Introduction

**Title:** Welcome to Module 1: ESP32 Introduction

**Slide Text:**
- ESP32 and AI Course
- Module 1: Getting Started with ESP32
- Learn the fundamentals of ESP32 microcontrollers
- Set up your development environment
- Build your first ESP32 project

**Visuals:**
- Course logo or title graphic at top
- High-quality photo of an ESP32 development board (centered)
- Icons representing: WiFi symbol, Bluetooth symbol, microcontroller chip
- Clean, modern color scheme (blue/teal technology theme)
- Instructor name and contact info at bottom (optional)

---

#### Slide 2: What is ESP32?

**Title:** What is ESP32?

**Slide Text:**
The ESP32 is a powerful, low-cost microcontroller system-on-chip (SoC) that brings wireless connectivity to embedded projects.

**Key Features:**
- Microcontroller with integrated WiFi and Bluetooth
- Developed by Espressif Systems (Shanghai, China)
- Low-cost and low-power design
- Purpose-built for Internet of Things (IoT) applications
- Powers millions of devices worldwide

**Visuals:**
- Large image of ESP32 chip (black square chip with pins)
- Espressif Systems logo
- Icon showing WiFi + Bluetooth + microprocessor combined
- Photo collage of IoT devices using ESP32 (smart home devices, wearables, sensors)
- World map showing global ESP32 usage/popularity

---

#### Slide 3: ESP32 Architecture Overview

**Title:** Inside the ESP32: Technical Architecture

**Slide Text:**
**Processing Power:**
- Dual-core (or single-core in some variants) Xtensa LX6/LX7 32-bit processor
- Clock speed: Up to 240 MHz (15x faster than Arduino Uno!)

**Wireless Connectivity:**
- Built-in WiFi (802.11 b/g/n) - 2.4 GHz
- Bluetooth Classic and Bluetooth Low Energy (BLE)

**Rich Peripheral Set:**
- SPI, I2C, I2S, UART communication protocols
- ADC (Analog to Digital Converter) - 18 channels
- DAC (Digital to Analog Converter)
- PWM (Pulse Width Modulation)
- Touch sensors, temperature sensor

**Visuals:**
- Block diagram of ESP32 architecture showing:
  - Two CPU cores
  - WiFi/Bluetooth radio module
  - Memory blocks (RAM/Flash)
  - Peripheral blocks
- Color-coded sections for different functional areas
- Arrows showing data flow between components
- Small icon indicators for each peripheral type

---

#### Slide 4: Types of ESP32 Boards

**Title:** The ESP32 Family: Choose Your Variant

**Slide Text:**
Espressif offers multiple ESP32 variants optimized for different use cases:

**ESP32 (Original)** - The classic choice
- Dual-core Xtensa processor
- WiFi + Bluetooth Classic + BLE
- Best for: General IoT projects

**ESP32-S2** - USB-focused
- Single-core, WiFi only
- Native USB OTG support
- Best for: USB devices, keyboards

**ESP32-S3** - AI & Audio powerhouse
- Dual-core with AI acceleration
- WiFi + BLE, USB OTG, PSRAM support
- Best for: Voice assistants, image processing

**ESP32-C3** - Budget-friendly
- Single-core RISC-V processor
- WiFi + BLE, very low cost
- Best for: Cost-sensitive projects

**ESP32-C6** - Future ready
- RISC-V architecture
- WiFi 6, BLE 5.3, Zigbee, Thread
- Best for: Matter/smart home devices

**Visuals:**
- Photo of each ESP32 chip variant arranged in a grid
- Comparison table with icons showing features (cores, WiFi, BT, USB)
- Price indicators ($, $$, $$$)
- Application icons showing best use cases for each
- Highlighted recommendation: ESP32-S3 for this course

---

#### Slide 5: Popular Development Boards

**Title:** ESP32 Development Boards

**Slide Text:**
These pre-made development boards make it easy to start working with ESP32:

**ESP32 DevKit** (Generic)
- Most common and affordable
- USB-to-serial built-in
- Available from many manufacturers

**ESP32-S3 DevKitC-1**
- Official Espressif board
- Best for this course (audio/AI features)
- USB-C connector

**ESP32-C3 Super Mini**
- Tiny form factor
- Very affordable (~$2-3)
- Perfect for space-constrained projects

**Adafruit ESP32 Feather**
- High quality, great documentation
- Battery charging built-in
- Premium option (~$20)

**SparkFun ESP32 Thing**
- Breadboard-friendly design
- Qwiic connector for easy sensor integration
- Good community support

**Visuals:**
- High-resolution photos of each development board
- Size comparison (actual size or scale reference)
- Callout labels pointing to key features:
  - USB connector
  - Power pins
  - GPIO pins
  - Built-in LED
  - Reset/Boot buttons
- Price tags for each board
- "Recommended for Course" badge on ESP32-S3 DevKitC

---

#### Slide 6: Arduino vs ESP32 - Hardware Comparison

**Title:** How Does ESP32 Compare to Arduino Uno?

**Slide Text:**
For those familiar with Arduino, here's how ESP32 stacks up:

**Comparison Table:**
| Feature | Arduino Uno | ESP32 |
|---------|-------------|-------|
| **Processor** | ATmega328P (8-bit) | Xtensa LX6 (32-bit) |
| **Clock Speed** | 16 MHz | 240 MHz |
| **RAM** | 2 KB | 520 KB |
| **Flash Memory** | 32 KB | 4 MB+ |
| **WiFi/Bluetooth** | No (requires shields) | Built-in |
| **GPIO Pins** | 14 digital, 6 analog | 30+ GPIO, 18 ADC |
| **Price** | ~$25 | ~$5-15 |

**Key Takeaway:** ESP32 offers dramatically more power and features at a similar or lower price point!

**Visuals:**
- Side-by-side photos of Arduino Uno and ESP32 DevKit
- Color-coded comparison table (green highlights for ESP32 advantages)
- Visual representations:
  - Speed gauge showing 16 MHz vs 240 MHz
  - Memory bar charts showing size difference
  - Icons for WiFi/BT (checkmark for ESP32, X for Arduino)
- "Winner" badge or trophy icon next to ESP32 column

---

#### Slide 7: Arduino vs ESP32 - Capabilities

**Title:** Capability Comparison: Arduino vs ESP32

**Slide Text:**
**Arduino Uno: The Classic Choice**
- Simple and beginner-friendly
- Excellent for learning basics
- Limited to local/wired projects
- Large library ecosystem
- Lower power consumption

**ESP32: The Modern Powerhouse**
- Much faster processing (240 MHz vs 16 MHz)
- Built-in wireless connectivity (WiFi + Bluetooth)
- Enough memory for complex applications
- Multiple cores for multitasking
- Advanced peripherals:
  - I2S for high-quality audio
  - Capacitive touch sensors
  - Hardware encryption
  - DMA support

**When to use which?**
- Arduino Uno: Simple projects, learning, ultra-low power
- ESP32: IoT projects, WiFi/BLE needed, complex applications, AI/audio

**Visuals:**
- Split-screen design: Arduino (left) vs ESP32 (right)
- Arduino side: Simple LED circuit, basic projects
- ESP32 side: Cloud connectivity diagram, smart home, voice assistant
- Icons representing capabilities:
  - Processing: Single gear vs dual gears
  - Connectivity: Cable vs wireless signals
  - Complexity: Simple flowchart vs complex diagram
- Decision tree flowchart at bottom: "Do you need WiFi?" → Yes = ESP32, No = Consider both

---

#### Slide 8: ESP32 Advantages

**Title:** Why Choose ESP32?

**Slide Text:**
**Top Advantages of ESP32 for Modern Projects:**

✓ **Integrated Connectivity**
- WiFi and Bluetooth built-in (no shields or modules needed)
- Direct internet access out of the box

✓ **Processing Power**
- 240 MHz dual-core processor
- Handle complex algorithms and multitasking

✓ **Memory Abundance**
- 520 KB RAM (260x more than Arduino Uno)
- 4+ MB Flash for code and data storage

✓ **Cost Effective**
- More features at lower cost (~$5-15)
- No need for additional shields

✓ **Active Community**
- Millions of developers worldwide
- Extensive libraries and examples
- Regular updates from Espressif

✓ **Perfect for AI and IoT**
- Fast enough for edge AI processing
- Built-in connectivity for cloud services
- Low enough power for battery operation

**Visuals:**
- Six icon boxes, one for each advantage
- Icons: WiFi/BT symbol, speedometer, memory chip, dollar sign, community/people, brain/cloud
- Background: Subtle circuit board pattern
- Photos showing real applications:
  - Smart home devices
  - Wearables
  - Industrial sensors
  - Voice assistants
- "Great for This Course!" callout banner

---

#### Slide 9: Development Environment Setup

**Title:** Setting Up Your Development Environment

**Slide Text:**
**Step-by-Step Setup for Arduino IDE 2.x:**

**1. Install Arduino IDE**
- Download Arduino IDE 2.x from arduino.cc
- Available for Windows, Mac, and Linux
- Install and launch the application

**2. Add ESP32 Board Support**
- Open: File → Preferences
- In "Additional Board Manager URLs," add:
  `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- Click OK

**3. Install ESP32 Boards**
- Open: Tools → Board → Boards Manager
- Search for "esp32"
- Install "esp32 by Espressif Systems"
- Wait for installation to complete

**4. Select Your Board**
- Tools → Board → esp32 → Select your board
- Tools → Port → Select your USB port
- You're ready to program!

**Visuals:**
- Screenshot sequence showing each step:
  1. Arduino IDE download page
  2. Preferences window with URL field highlighted
  3. Boards Manager with ESP32 search
  4. Board selection dropdown menu
- Numbered callout boxes with arrows pointing to relevant UI elements
- Green checkmarks next to completed steps
- USB cable connecting computer to ESP32 illustration
- "Pro Tip" callout: Install USB drivers if board not detected

---

#### Slide 10: Your First ESP32 Program

**Title:** Hello ESP32: Your First Program

**Slide Text:**
**Let's start with a classic: Blink (with a twist!)**

The traditional "Hello World" for microcontrollers, but we'll add WiFi status!

**Basic Structure:**
```cpp
void setup() {
  // Runs once at startup
  Serial.begin(115200);  // Note: Higher baud rate!
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  // Runs repeatedly
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
}
```

**Key Differences from Arduino:**
- Higher Serial baud rate (115200 vs 9600)
- More built-in constants (LED_BUILTIN works on most boards)
- Can use WiFi and Bluetooth libraries
- Dual-core capable (advanced topic)

**Visuals:**
- Code editor screenshot showing the blink program
- Syntax highlighting (colored code)
- Photo sequence: ESP32 board with LED on → LED off → LED on
- Animated GIF or photo series showing blinking
- Serial Monitor screenshot showing output
- Callout boxes explaining:
  - "setup() runs once"
  - "loop() runs forever"
  - "115200 baud rate is standard for ESP32"
- Connection diagram: USB cable from laptop to ESP32

---

#### Slide 11: Hands-on Exercise Preview

**Title:** Hands-On: LED and Button Game

**Slide Text:**
**Time to Build Something Fun!**

**Project: Reaction Time Game**
- 15 addresable LEDs, using FASTLED library
- One button
- LED moves from right to left and back 
- Press button at the right time when the middle LED is lit.
- middle LED blinks the number of "good consequative presses"
- starts moving from right to left and waiting for a press again.

**Learning Objectives:**
✓ Get comfortable with ESP32 pin assignments
✓ Learn digital input and output (GPIO control)
✓ Understand timing with millis() function
✓ Practice working with button interrupts
✓ Build something fun while learning!

**What You'll Need:**
- ESP32 development board
- 15 LEDs 
- 1 push button
- Breadboard and jumper wires

**Visuals:**
- Breadboard circuit diagram showing:
  - ESP32 in center
  - 3-5 LEDs connected to different GPIO pins
  - Button with pull-down resistor
  - Clear wire routing
- Photo of completed project
- Action shot: Hand pressing button while LEDs are lit
- Parts list with images of each component

---

#### Slide 12: Pin Configuration Tips

**Title:** ESP32 Pin Configuration: Know Before You Code

**Slide Text:**
**Important Pin Considerations:**

⚠ **Input-Only Pins:**
- GPIO 34, 35, 36, 39 (VP, VN)
- Can only read, cannot output
- Use for sensors and buttons only

⚠ **Strapping Pins (Avoid During Boot):**
- GPIO 0: Boot mode selection
- GPIO 2: Boot mode, must be LOW
- GPIO 12: Flash voltage
- GPIO 15: Timing/boot messages
- These pins affect boot behavior!

✓ **Safe Pins for General Use:**
- GPIO 16, 17, 18, 19, 21, 22, 23
- GPIO 25, 26, 27, 32, 33
- Use these for LEDs, buttons, sensors

**Best Practice:**
Always check your specific board's pinout diagram before wiring!

**Visuals:**
- ESP32 pinout diagram with color coding:
  - Green: Safe general-purpose pins
  - Yellow: Input-only pins
  - Red: Strapping pins (use with caution)
  - Blue: Communication pins (I2C, SPI, UART)
  - Purple: Power pins (3.3V, 5V, GND)
- Legend explaining color codes
- Zoomed callout showing pin numbers clearly
- Warning symbol next to strapping pins
- "Print This!" watermark (suggest students print pinout reference)
- QR code linking to detailed pinout reference online

---

#### Slide 13: Summary

**Title:** Module 1 Summary: You're Ready to Build!

**Slide Text:**
**What We've Covered:**

✓ **ESP32 Fundamentals**
- Powerful, WiFi-enabled microcontroller
- Dual-core processor, 240 MHz
- Built-in WiFi and Bluetooth

✓ **Comparison to Arduino**
- Much more capable than traditional Arduino boards
- More memory, faster processing, wireless connectivity
- Similar programming model (Arduino IDE compatible)

✓ **ESP32 Variants**
- Multiple variants for different needs
- ESP32-S3 recommended for AI/audio projects

✓ **Development Setup**
- Easy to get started with Arduino IDE
- Large community and extensive libraries

✓ **You're Ready!**
- Development environment configured
- Understanding of ESP32 capabilities
- Ready for IoT and AI applications

**Next Up:** Module 2 - Exploring the ESP32 Ecosystem

**Visuals:**
- Checklist with green checkmarks next to each topic
- Circular journey graphic showing progression:
  - "ESP32 Basics" → "Setup" → "First Program" → "Hands-on"
- Photo collage of accomplishments:
  - ESP32 board
  - Arduino IDE running
  - LED blinking
  - Completed game project
- Progress bar: Module 1 Complete (20% of course)
- "Great job!" or motivational badge
- Arrow pointing forward labeled "Next: ESP32 Ecosystem"
- Footer with course hashtag or community link

---

## Module 2: ESP32 Ecosystem for Apps
**Goal**: Show participants ready-made applications and professional deployment practices

### Topics:
* Out-of-the-box ESP32/ESP32-C3 implementations
  - WLED for LED control
  - ESPHome (optional)
  - Tasmota (optional)
* Over-the-air (OTA) updates
* Demonstrates what's possible with ESP32 in production environments

### Slide Deck:

**Slide 1: Module 2 - ESP32 Ecosystem**
- Title slide
- Exploring ready-made solutions and deployment practices

**Slide 2: Why Use Ready-Made Solutions?**
- Faster time to deployment
- Battle-tested code
- Community support and documentation
- Learn best practices from production code
- Focus on customization, not reinventing the wheel

**Slide 3: ESP32 Ecosystem Overview**
- Rich ecosystem of open-source projects
- Web-based configuration interfaces
- Mobile app integrations
- Professional-grade features
- Active development communities

**Slide 4: WLED - Smart LED Control**
- Fast and feature-rich implementation for LED strips
- Supports WS2812B, SK6812, APA102, and more
- Web interface for control
- Mobile apps available
- Effects, presets, and animations
- Integration with Home Assistant, Alexa, Google Home

**Slide 5: WLED Features**
- 100+ built-in effects
- Segments for different LED zones
- Sync multiple devices
- Real-time audio reactive effects
- Timeline and presets
- Easy WiFi setup (AP mode)

**Slide 6: WLED Demo**
- Live demonstration of:
  - Initial setup and configuration
  - Web interface navigation
  - Creating custom effects
  - Mobile app control
  - Syncing multiple devices

**Slide 7: ESPHome - Smart Home Integration**
- Framework for creating custom smart home devices
- YAML-based configuration
- Seamless Home Assistant integration
- Supports sensors, switches, lights, climate control
- OTA updates built-in
- No C++ coding required for basic projects

**Slide 8: ESPHome Example Use Cases**
- Custom environmental sensors (temp, humidity, CO2)
- Smart switches and relays
- LED controllers
- Garage door openers
- Custom displays and notifications
- Irrigation controllers

**Slide 9: Tasmota - IoT Firmware**
- Alternative open-source firmware
- Originally for Sonoff devices
- Supports many sensors and peripherals
- Web UI and MQTT control
- Rules engine for automation
- Extensive device support

**Slide 10: Comparing Solutions**
| Feature | WLED | ESPHome | Tasmota |
|---------|------|---------|---------|
| Primary Use | LED control | General IoT | Device firmware |
| Configuration | Web UI | YAML | Web UI |
| Complexity | Low | Medium | Medium |
| Customization | Limited | High | Medium |
| Best For | LED projects | Smart home | Device replacement |

**Slide 11: Over-The-Air (OTA) Updates**
- What is OTA?
  - Uploading firmware wirelessly
  - No USB cable required
  - Update devices in remote locations
  - Essential for production deployments

**Slide 12: OTA Benefits**
- Deploy fixes and features remotely
- Update multiple devices quickly
- No physical access needed
- Reduced maintenance costs
- Better user experience

**Slide 13: OTA in Arduino IDE**
- ESP32 OTA library (ArduinoOTA)
- Basic setup steps:
  1. Include ArduinoOTA library
  2. Configure OTA settings (hostname, password)
  3. Call `ArduinoOTA.begin()` in setup
  4. Call `ArduinoOTA.handle()` in loop
- Device appears as network port in Arduino IDE

**Slide 14: OTA Security Considerations**
- Always use password protection
- Encrypt OTA traffic when possible
- Verify firmware signatures
- Implement rollback mechanisms
- Monitor for failed updates
- Limit OTA to trusted networks

**Slide 15: OTA Best Practices**
- Version tracking and logging
- Staged rollouts (test devices first)
- Backup/rollback capabilities
- Update status indicators (LEDs)
- Fail-safe bootloader
- Monitor memory usage

**Slide 16: Production Deployment Checklist**
- WiFi credential management
- Secure OTA updates
- Error handling and recovery
- Watchdog timers
- Status monitoring
- User-friendly setup (captive portal)
- Documentation

**Slide 17: Hands-on Exercise**
- Flash WLED to ESP32
- Configure and control LED strip
- Test OTA update functionality
- Explore web interface features
- Sync multiple devices

**Slide 18: Summary**
- Rich ecosystem of ready-made solutions
- WLED, ESPHome, Tasmota offer different strengths
- OTA updates are essential for production
- Learn from existing projects
- Apply these patterns to custom applications

---

## Module 3: Introduction to LLMs
**Goal**: Bridge the knowledge gap between hardware and AI services

### Topics:
* What is REST API? (HTTP request-response and JSON)
* Calling OpenAI endpoints
* Working with Postman (visual, non-coding experimentation)
* Using different models (GPT-4, GPT-4o, TTS, and others)
* System prompts and prompt engineering basics

**Key Skill**: Understanding HTTP/REST, JSON data structures, and API authentication before coding

### Slide Deck:

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

---

## Module 4: Working with LLM and ESP32
**Goal**: Combine ESP32 hardware with LLM capabilities

### Project:
Implement a console-based LLM chat
* WiFi connectivity on ESP32
* Making API calls from ESP32
* Parsing JSON responses
* Serial Monitor-based text chat interface

### Slide Deck:

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

---

## Module 5: Building a Full Voice Chat with LLM
**Goal**: Create a complete voice assistant solution

### Project Components:
* I2S microphone integration
* Speaker/amplifier (e.g., MAX98357A)
* Speech-to-text processing
* LLM integration for conversation
* Text-to-speech output
* Optional: Wake word detection

### Final Outcome:
A fully functional voice-activated AI assistant running on ESP32

### Slide Deck:

**Slide 1: Module 5 - Voice Chat with LLM**
- Title slide
- Building a complete voice assistant

**Slide 2: Project Vision**
- Speak to your ESP32
- Voice is transcribed to text
- Sent to ChatGPT/OpenAI
- Response converted to speech
- Played through speaker
- Natural conversation experience

**Slide 3: System Architecture**
```
[Microphone] -> [ESP32] -> [Whisper API] -> [Text]
                   ↓                          ↓
              [Speaker] <- [TTS API] <- [ChatGPT API]
```
- Complete audio pipeline
- Cloud-based processing
- Real-time interaction

**Slide 4: Required Hardware**
- ESP32 board (preferably ESP32-S3 for better audio)
- I2S MEMS microphone (e.g., INMP441, ICS-43434)
- I2S amplifier + speaker (e.g., MAX98357A)
- Push button (for trigger/PTT)
- LED indicators (status feedback)
- Power supply (5V/2A recommended)

**Slide 5: I2S Protocol Overview**
- Inter-IC Sound
- Serial bus for digital audio
- 3 main signals:
  - SCK (Serial Clock)
  - WS (Word Select / LRCLK)
  - SD (Serial Data)
- High quality digital audio
- ESP32 has built-in I2S support

**Slide 6: I2S Microphone Pinout**
- Common INMP441 connections:
  - VDD -> 3.3V
  - GND -> GND
  - SD -> GPIO (data in)
  - WS -> GPIO (word select)
  - SCK -> GPIO (clock)
  - L/R -> GND (left channel) or 3.3V (right)

**Slide 7: I2S Speaker/Amplifier Pinout**
- MAX98357A connections:
  - VIN -> 5V
  - GND -> GND
  - DIN -> GPIO (data)
  - BCLK -> GPIO (bit clock)
  - LRC -> GPIO (left/right clock)
  - Speaker terminals to 4-8Ω speaker

**Slide 8: Recommended GPIO Pins**
```
Microphone (Input):
- I2S_WS: GPIO 25
- I2S_SCK: GPIO 26
- I2S_SD: GPIO 33

Speaker (Output):
- I2S_WS: GPIO 15
- I2S_BCK: GPIO 14
- I2S_DOUT: GPIO 22

Button: GPIO 13
Status LED: GPIO 2
```

**Slide 9: I2S Library Setup**
```cpp
#include <driver/i2s.h>

#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 26
#define SAMPLE_RATE 16000

i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  // ... more config
};
```

**Slide 10: Recording Audio**
- Record when button pressed
- Fixed duration (e.g., 5 seconds)
- Or voice activity detection
- Store in buffer
- Convert to WAV format
- Send to Whisper API

**Slide 11: Audio Recording Flow**
1. User presses button
2. LED indicates recording
3. Capture audio samples via I2S
4. Store in buffer (5 seconds = ~80KB @ 16kHz)
5. Convert to WAV format
6. Base64 encode (for HTTP multipart)
7. Send to Whisper API

**Slide 12: WAV File Format**
- Header (44 bytes)
  - File size
  - Sample rate
  - Channels
  - Bit depth
- Audio data (PCM samples)
- ESP32 must generate WAV header
- Required for Whisper API

**Slide 13: Creating WAV Header**
```cpp
void createWavHeader(byte* header, int wavSize) {
  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  unsigned int fileSize = wavSize + 44 - 8;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  // ... rest of header
}
```

**Slide 14: Sending Audio to Whisper**
- Endpoint: `/v1/audio/transcriptions`
- HTTP POST with multipart/form-data
- Fields:
  - file: audio WAV data
  - model: "whisper-1"
  - language: "en" (optional)
- Response: JSON with transcribed text

**Slide 15: HTTP Multipart Form Data**
```cpp
String boundary = "----ESP32Boundary";
client.println("POST /v1/audio/transcriptions HTTP/1.1");
client.println("Host: api.openai.com");
client.println("Authorization: Bearer " + apiKey);
client.println("Content-Type: multipart/form-data; boundary=" + boundary);
// ... send file data
```

**Slide 16: Processing Whisper Response**
```cpp
// Response JSON:
{
  "text": "Hello, how are you today?"
}

// Extract text:
StaticJsonDocument<1024> doc;
deserializeJson(doc, response);
String transcribedText = doc["text"];
Serial.println("You said: " + transcribedText);
```

**Slide 17: Sending to ChatGPT**
- Use transcribed text as user input
- Same as Module 4 (text chat)
- Add to conversation history
- Send to chat completions API
- Receive text response
- Now need to convert to speech!

**Slide 18: Text-to-Speech Request**
```cpp
// Endpoint: /v1/audio/speech
doc.clear();
doc["model"] = "tts-1";
doc["input"] = aiResponseText;
doc["voice"] = "alloy"; // or nova, shimmer, etc.
doc["response_format"] = "mp3";

// Response is binary MP3 data
```

**Slide 19: Handling MP3 Audio**
- OpenAI returns MP3 format
- ESP32 needs to decode MP3
- Use ESP32-audioI2S library
- Or request different format (opus, pcm)
- PCM is easiest but larger
- MP3 saves bandwidth

**Slide 20: Installing ESP32-audioI2S Library**
- Library by schreibfaul1
- Supports MP3, AAC, WAV, and more
- Simple API for I2S playback
- Install from Library Manager
```cpp
#include "Audio.h"

Audio audio;
audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
audio.connecttospeech(mp3Data, "en");
```

**Slide 21: Playing TTS Audio**
```cpp
// Simpler approach: request PCM16
doc["response_format"] = "pcm";

// Receive raw PCM data
// Write directly to I2S output
size_t bytes_written;
i2s_write(I2S_NUM_0, pcmData, dataSize, &bytes_written, portMAX_DELAY);
```

**Slide 22: Complete Conversation Flow**
1. Button press detected
2. LED: Recording
3. Capture 5 seconds of audio
4. LED: Processing (blinking)
5. Send to Whisper (speech-to-text)
6. Send text to ChatGPT
7. Receive AI response text
8. Send to TTS API
9. LED: Speaking (different color/pattern)
10. Play audio through speaker
11. LED: Ready (solid)

**Slide 23: State Machine Design**
```cpp
enum State {
  IDLE,
  RECORDING,
  TRANSCRIBING,
  THINKING,
  SPEAKING
};

State currentState = IDLE;

void loop() {
  switch(currentState) {
    case IDLE: checkButton(); break;
    case RECORDING: recordAudio(); break;
    case TRANSCRIBING: sendToWhisper(); break;
    // ... etc
  }
}
```

**Slide 24: Memory Management**
- Audio buffers are large (80KB+)
- MP3 data can be large
- Use PSRAM if available (ESP32-S3)
- Stream audio instead of buffering
- Release memory after each stage
- Monitor heap usage

**Slide 25: Using PSRAM**
```cpp
// ESP32-S3 has PSRAM (up to 8MB)
#ifdef BOARD_HAS_PSRAM
  byte* audioBuffer = (byte*)ps_malloc(BUFFER_SIZE);
#else
  byte* audioBuffer = (byte*)malloc(BUFFER_SIZE);
#endif

// Check available PSRAM
Serial.println("PSRAM: " + String(ESP.getPsramSize()));
```

**Slide 26: Voice Activity Detection (VAD)**
- Optional enhancement
- Detect when user starts/stops speaking
- Automatic recording start/stop
- Better UX than button press
- Libraries: webrtc-vad, simple threshold
- Requires tuning for environment

**Slide 27: Wake Word Detection**
- Advanced feature
- "Hey Assistant" triggers recording
- Edge Impulse for custom wake words
- TensorFlow Lite Micro on ESP32
- Runs on-device (no cloud)
- Battery impact consideration

**Slide 28: Power Optimization**
- Voice assistants can drain power
- Use deep sleep when idle
- Wake on button interrupt
- Disable WiFi when not in use
- Adjust microphone gain
- Consider battery capacity

**Slide 29: Error Handling**
- Audio recording failures
- Network timeouts (longer for audio)
- API errors (file too large, unsupported format)
- Playback failures
- Provide audio feedback (beeps)
- LED error indicators

**Slide 30: Audio Feedback Cues**
- Recording start: Single beep
- Recording end: Double beep
- Processing: Triple beep
- Error: Long low tone
- Enhances user experience
- Can use buzzer or speaker

**Slide 31: Multi-language Support**
- Whisper supports 50+ languages
- Specify language in API call
- Or use auto-detect
- TTS supports many languages
- Match TTS language to detected language
- Unicode text handling

**Slide 32: Latency Optimization**
- Current flow has 3 API calls (slow)
- Optimize:
  - Use faster models (gpt-3.5-turbo)
  - Reduce audio length
  - Stream TTS responses
  - Keep WiFi connected
  - Reuse HTTP connections
- Typical latency: 5-10 seconds

**Slide 33: Cost Considerations**
- Whisper: $0.006/minute
- GPT-3.5-turbo: $0.0015/1K tokens
- TTS: $0.015/1K characters
- Example conversation:
  - 5 sec audio = ~$0.0005
  - Response = ~$0.0003
  - TTS = ~$0.001
  - Total: ~$0.002 per interaction

**Slide 34: Alternative: Local TTS**
- ESP32-compatible TTS engines
- Lower quality than OpenAI
- No API costs
- No internet required
- Examples:
  - eSpeak (port available)
  - SAM (Software Automatic Mouth)
  - Trade-off: quality vs cost/latency

**Slide 35: Alternative: Local Speech Recognition**
- Edge Impulse audio classification
- Limited vocabulary
- No API costs
- Lower latency
- Good for simple commands
- Not suitable for conversation

**Slide 36: Hands-on Exercise Part 1**
- Wire up I2S microphone
- Test audio recording
- Verify audio quality
- Display waveform data via Serial
- Record and save to SD card (optional)

**Slide 37: Hands-on Exercise Part 2**
- Wire up I2S speaker/amplifier
- Test audio playback
- Play sample WAV file
- Adjust volume
- Test different audio formats

**Slide 38: Hands-on Exercise Part 3**
- Implement Whisper integration
- Record voice and transcribe
- Display transcribed text
- Test accuracy with different accents
- Handle errors gracefully

**Slide 39: Hands-on Exercise Part 4**
- Integrate TTS
- Convert text to speech
- Play through speaker
- Test different voices
- Combine with ChatGPT responses

**Slide 40: Final Integration**
- Combine all components
- Implement state machine
- Add LED indicators
- Test complete conversation flow
- Debug and optimize
- Polish user experience

**Slide 41: Testing Scenarios**
- Simple questions
- Long responses (memory test)
- Background noise
- Different speaking volumes
- Network interruptions
- Error recovery
- Continuous conversations

**Slide 42: Enclosure Design**
- 3D printed case
- Microphone placement (away from speaker)
- Speaker positioning
- Button accessibility
- LED visibility
- Ventilation for heat
- Cable management

**Slide 43: Troubleshooting Audio Issues**
- No audio input: Check I2S pins, mic power
- Distorted input: Adjust gain, check sample rate
- No audio output: Check amp power, speaker wiring
- Garbled output: Verify I2S config, bit depth
- Use oscilloscope or logic analyzer
- Test with known-good audio files

**Slide 44: Advanced Features**
- Conversation history persistence
- User identification (voice print)
- Multi-turn conversations
- Context awareness
- Integration with smart home
- Custom skills/commands

**Slide 45: Integration Ideas**
- Control smart home devices
- Weather updates
- Calendar/reminders
- Language translation
- Music player control
- IoT sensor readings
- Custom automation

**Slide 46: Project Showcase**
- Demo completed projects
- Share challenges and solutions
- Discuss customizations
- Creative use cases
- Community sharing

**Slide 47: Next Steps and Resources**
- OpenAI documentation
- ESP32 audio examples
- Community forums
- GitHub repositories
- Further learning:
  - Edge AI on ESP32
  - Custom wake words
  - Local LLM inference

**Slide 48: Course Summary**
- Module 1: ESP32 fundamentals
- Module 2: ESP32 ecosystem
- Module 3: Understanding LLMs and APIs
- Module 4: Text-based AI chat
- Module 5: Full voice assistant
- You've built a complete AI-powered device!

**Slide 49: Beyond This Course**
- Contribute to open source
- Share your projects
- Build commercial products
- Explore edge AI
- Join maker communities
- Keep learning and building!

**Slide 50: Thank You!**
- Congratulations on completing the course!
- You now have skills to build AI-powered IoT devices
- Questions and discussion
- Stay in touch
- Course resources and code available

---

## Questions to Address:
- Specific ESP32 variants to recommend (ESP32-S3 for audio/AI, ESP32-C3 for cost)?
- Hardware kit specifications or participant-sourced components?
- Additional out-of-the-box implementations for Module 2?
