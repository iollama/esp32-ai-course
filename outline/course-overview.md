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

---

## Module 4: Working with LLM and ESP32
**Goal**: Combine ESP32 hardware with LLM capabilities

### Project:
Implement a console-based LLM chat
* WiFi connectivity on ESP32
* Making API calls from ESP32
* Parsing JSON responses
* Serial Monitor-based text chat interface

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

---

## Questions to Address:
- Specific ESP32 variants to recommend (ESP32-S3 for audio/AI, ESP32-C3 for cost)?
- Hardware kit specifications or participant-sourced components?
- Additional out-of-the-box implementations for Module 2?
