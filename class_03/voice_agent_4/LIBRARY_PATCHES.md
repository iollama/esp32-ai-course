# ArduinoWebsockets Library Patches

These patches are required for the ESP32 WSS (secure WebSocket) connection to OpenAI's Realtime API to work correctly.

## Problem

`WebsocketsClient::setInsecure()` has no effect on ESP32. The internal `upgradeToSecuredConnection()` method has an `else { client->setInsecure(); }` fallback for ESP8266, but the ESP32 branch is missing it entirely. As a result, the underlying `WiFiClientSecure` attempts certificate verification with no CA bundle, and the WebSocket handshake silently fails every time.

---

## Patch 1: `esp32_tcp.hpp`

**File:** `<Arduino libraries folder>/ArduinoWebsockets/src/tiny_websockets/network/esp32/esp32_tcp.hpp`

Add a `setInsecure()` method to `SecuredEsp32TcpClient`:

```cpp
class SecuredEsp32TcpClient : public GenericEspTcpClient<WiFiClientSecure> {
public:
  // ADD THIS METHOD:
  void setInsecure() {
    this->client.setInsecure();
  }

  void setCACert(const char* ca_cert) {
    this->client.setCACert(ca_cert);
  }
  // ... rest of class unchanged
```

---

## Patch 2: `websockets_client.cpp`

**File:** `<Arduino libraries folder>/ArduinoWebsockets/src/websockets_client.cpp`

In `WebsocketsClient::upgradeToSecuredConnection()`, find the `#elif defined(ESP32)` block and add an `else` branch after `setPrivateKey`:

```cpp
    #elif defined(ESP32)
        if(this->_optional_ssl_ca_cert) {
            client->setCACert(this->_optional_ssl_ca_cert);
        }
        if(this->_optional_ssl_client_ca) {
            client->setCertificate(this->_optional_ssl_client_ca);
        }
        if(this->_optional_ssl_private_key) {
            client->setPrivateKey(this->_optional_ssl_private_key);
        } else {
            // ADD THIS: without this, setInsecure() is never called and WSS always fails on ESP32
            client->setInsecure();
        }
    #endif
```

---

## Notes

- Tested with ArduinoWebsockets v0.5.3 on ESP32-S3
- The Arduino libraries folder is typically at `~/Documents/Arduino/libraries/`
- These are upstream bugs; check if a newer version of the library has fixed them before applying manually
