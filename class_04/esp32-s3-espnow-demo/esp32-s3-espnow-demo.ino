/*
  ESP-NOW LED Control Demo
  ========================
  Commands via Serial (115200 baud):
    PAIR AA:BB:CC:DD:EE:FF  - Add a peer
    STATUS                  - List paired peers
    RED / GREEN / BLUE      - Send color to all peers
    COLOR 255 100 0         - Send custom RGB to all peers
    BROADCAST RED           - Send to everyone in the room
    CLEAR                   - Turn off your own LED
    HELP                    - Show all commands
*/

#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>

// ── Hardware config ───────────────────────────────────────────────
#define LED_PIN    48   // Built-in NeoPixel on ESP32-S3 DevKit
#define LED_COUNT  1
// For ESP32-C3 SuperMini change LED_PIN to 8
// ─────────────────────────────────────────────────────────────────

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Message structure sent over ESP-NOW
typedef struct {
  uint8_t r, g, b;
} ColorMessage;

// Store peer MACs manually for reliable iteration
uint8_t peers[20][6];
int peerCount = 0;

// ── Boot ──────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  strip.begin();
  strip.setBrightness(80);
  strip.show();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);

  Serial.println("\n=============================");
  Serial.println("  ESP-NOW LED Demo");
  Serial.println("=============================");
  Serial.print("Your MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Share this MAC with your partner.");
  Serial.println("Then type: PAIR <their MAC>");
  Serial.println("Type HELP for all commands.");
  Serial.println("=============================\n");

  setColor(0, 0, 50); // Blue on boot = ready
}

// ── Main loop - read Serial commands ─────────────────────────────
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    handleCommand(cmd);
  }
}

// ── Command handler ───────────────────────────────────────────────
void handleCommand(String cmd) {

  if (cmd.startsWith("PAIR ")) {
    String macStr = cmd.substring(5);
    macStr.trim();
    addPeer(macStr);

  } else if (cmd == "STATUS") {
    printStatus();

  } else if (cmd == "RED") {
    sendColorToAll(255, 0, 0);

  } else if (cmd == "GREEN") {
    sendColorToAll(0, 255, 0);

  } else if (cmd == "BLUE") {
    sendColorToAll(0, 0, 255);

  } else if (cmd.startsWith("COLOR ")) {
    parseAndSendColor(cmd.substring(6));

  } else if (cmd.startsWith("BROADCAST ")) {
    String color = cmd.substring(10);
    sendBroadcast(color);

  } else if (cmd == "CLEAR") {
    setColor(0, 0, 0);
    Serial.println("LED cleared.");

  } else if (cmd == "HELP") {
    printHelp();

  } else {
    Serial.println("Unknown command. Type HELP for a list of commands.");
  }
}

// ── Pairing ───────────────────────────────────────────────────────
void addPeer(String macStr) {
  uint8_t mac[6];
  if (!parseMac(macStr, mac)) {
    Serial.println("ERROR: Bad MAC format. Use AA:BB:CC:DD:EE:FF");
    return;
  }

  if (esp_now_is_peer_exist(mac)) {
    Serial.println("Already paired with " + macStr);
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) == ESP_OK) {
    // Store in our own array for reliable iteration
    memcpy(peers[peerCount], mac, 6);
    peerCount++;

    Serial.println("Paired with " + macStr);

    // Handshake: flash their LED white so they know pairing worked
    ColorMessage msg = {255, 255, 255};
    esp_now_send(mac, (uint8_t*)&msg, sizeof(msg));
    delay(300);
    msg = {0, 0, 50};
    esp_now_send(mac, (uint8_t*)&msg, sizeof(msg));
  } else {
    Serial.println("ERROR: Could not add peer.");
  }
}

// ── Send color to all paired peers ───────────────────────────────
void sendColorToAll(uint8_t r, uint8_t g, uint8_t b) {
  if (peerCount == 0) {
    Serial.println("No peers paired yet. Use PAIR first.");
    return;
  }

  ColorMessage msg = {r, g, b};
  for (int i = 0; i < peerCount; i++) {
    esp_now_send(peers[i], (uint8_t*)&msg, sizeof(msg));
  }

  Serial.printf("Sent RGB(%d,%d,%d) to %d peer(s)\n", r, g, b, peerCount);
}

// ── Broadcast to everyone in the room ────────────────────────────
void sendBroadcast(String colorName) {
  uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  if (!esp_now_is_peer_exist(broadcastMac)) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcastMac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }

  ColorMessage msg = {0, 0, 0};
  if      (colorName == "RED")   { msg = {255, 0, 0}; }
  else if (colorName == "GREEN") { msg = {0, 255, 0}; }
  else if (colorName == "BLUE")  { msg = {0, 0, 255}; }
  else if (colorName == "WHITE") { msg = {255, 255, 255}; }
  else if (colorName == "OFF")   { msg = {0, 0, 0}; }
  else {
    Serial.println("Unknown color. Try: RED GREEN BLUE WHITE OFF");
    return;
  }

  esp_now_send(broadcastMac, (uint8_t*)&msg, sizeof(msg));
  Serial.println("Broadcast sent to everyone!");
}

// ── Receive callback ──────────────────────────────────────────────
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(ColorMessage)) return;
  ColorMessage msg;
  memcpy(&msg, data, sizeof(msg));
  setColor(msg.r, msg.g, msg.b);
  Serial.printf("Received color RGB(%d,%d,%d)\n", msg.r, msg.g, msg.b);
}

// ── Helpers ───────────────────────────────────────────────────────
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  strip.fill(strip.Color(r, g, b));
  strip.show();
}

void parseAndSendColor(String args) {
  int r, g, b;
  if (sscanf(args.c_str(), "%d %d %d", &r, &g, &b) == 3) {
    sendColorToAll(r, g, b);
  } else {
    Serial.println("Usage: COLOR 255 100 0");
  }
}

bool parseMac(String macStr, uint8_t* mac) {
  return sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
    &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6;
}

void printHelp() {
  Serial.println("\n---- Commands -------------------------");
  Serial.println("  PAIR AA:BB:CC:DD:EE:FF  - Add a peer");
  Serial.println("  STATUS                  - List paired peers");
  Serial.println("  RED / GREEN / BLUE      - Send color to all peers");
  Serial.println("  COLOR 255 100 0         - Send custom RGB to all peers");
  Serial.println("  BROADCAST RED           - Send to everyone in the room");
  Serial.println("  CLEAR                   - Turn off your own LED");
  Serial.println("  HELP                    - Show this list");
  Serial.println("---------------------------------------\n");
}

void printStatus() {
  Serial.printf("Paired peers: %d\n", peerCount);
  for (int i = 0; i < peerCount; i++) {
    Serial.printf("  %d: %02X:%02X:%02X:%02X:%02X:%02X\n", i + 1,
      peers[i][0], peers[i][1], peers[i][2],
      peers[i][3], peers[i][4], peers[i][5]);
  }
}
