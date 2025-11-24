# Module 1: ESP32 Introduction

**Goal**: Introduce ESP32 hardware and establish foundational programming skills

## Topics:
* What is ESP32
* Types of ESP32 boards
* Comparison to Arduino (dual-core processor, WiFi/BT built-in, more GPIO, faster processor)
* Development environment setup

## Hands-on Exercise:
A game with LEDs and one button to provide immediate hands-on experience with ESP32 programming

---

## Slide Deck

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
