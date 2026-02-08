/*
  ESP32-C3 + WS2812 + WiFi LED Controller (single-file)

  ✅ AP fallback mode:
     - SSID:  <AP_NAME_PREFIX>-XXXX   (XXXX = last 4 hex digits of DEVICE base MAC)
     - IP:    192.168.4.1
     - Channel selection algorithm:
         1) If AP channel exists in NVS -> use it
         2) Else if AP_DEFAULT_CHANNEL (compile-time) is valid 1..11 -> use it
         3) Else derive 1..11 from DEVICE base MAC (mod)
         4) Save final selected channel to NVS
  ✅ STA mode:
     - Configure router Wi-Fi via web UI
     - Credentials stored in NVS (Preferences)
     - If STA connect fails on boot -> fallback to AP automatically
  ✅ mDNS:
     - http://<MDNS_BASE_NAME>-xxxx.local/
       (xxxx = last 4 hex digits of DEVICE base MAC, lowercase)
     - Note: mDNS is reliable on most desktop OSes in STA mode. In AP mode,
       many phones won't resolve .local; use 192.168.4.1.

  ✅ Web UI:
     - Wi-Fi status and actions (switch to STA/AP, forget Wi-Fi)
     - Wi-Fi settings include AP Channel (stored in NVS)
     - LED Configuration (stored in NVS; requires reboot to apply):
         * Number of LEDs (default 11, max 50)
     - LED Controls:
         * Power, Brightness, Color, Mode
     - Modes:
         * Solid, Rainbow, Theater, Pulse (pulse uses selected color)

  LED DATA PIN:
  - Fixed at compile time to LED_DATA_PIN.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <FastLED.h>

// ============================================================================
// USER CONFIG (compile-time)
// ============================================================================

#define AP_NAME_PREFIX   "MAKELAB"
#define MDNS_BASE_NAME   "makelab"

static const char* AP_PASS   = "makelab123";
static const bool  AP_HIDDEN = false;

// AP channel default:
// - Set to 1..11 to force a fixed default (used only if NVS not set).
// - Set to -1 to disable default and derive from MAC when NVS not set.
#define AP_DEFAULT_CHANNEL  (-1)

static const uint32_t STA_CONNECT_TIMEOUT_MS = 12000;
static const bool     STA_ALLOW_RECONNECT    = true;

#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

// ✅ Fixed data pin (compile-time)
#define LED_DATA_PIN 2

static const uint16_t DEFAULT_NUM_LEDS = 11;

static const uint16_t MIN_NUM_LEDS = 1;
static const uint16_t MAX_NUM_LEDS = 50;

#define VOLTS       5
#define MAX_MA      200

// ============================================================================
// Globals
// ============================================================================

WebServer server(80);
Preferences prefs;

static uint16_t g_numLeds = DEFAULT_NUM_LEDS;
static CRGB* g_leds = nullptr;

enum WifiRunMode : uint8_t { RUN_AP = 0, RUN_STA = 1 };
static WifiRunMode g_runMode = RUN_AP;

static String  g_apSsid;
static String  g_mdnsHost;  // without ".local"
static uint8_t g_apChannel = 6;

enum Mode : uint8_t {
  MODE_SOLID = 0,
  MODE_RAINBOW,
  MODE_THEATER,
  MODE_PULSE
};

struct {
  bool power = true;
  uint8_t brightness = 96;
  CRGB color = CRGB::Blue;
  Mode mode = MODE_RAINBOW;
} st;

static uint32_t tAnim = 0;
static uint32_t tInfo = 0;
static uint32_t tWiFi = 0;

static uint16_t rainbowHue = 0;
static uint16_t theaterIndex = 0;

static const uint16_t RAINBOW_INTERVAL_MS = 25;
static const uint16_t THEATER_INTERVAL_MS = 70;
static const uint16_t PULSE_INTERVAL_MS   = 25;

static const uint32_t SERIAL_INFO_MS   = 5000;
static const uint32_t WIFI_MAINTAIN_MS = 2000;

// ============================================================================
// Utilities
// ============================================================================

static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c; break;
    }
  }
  return out;
}

static uint8_t clampU8(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

static uint16_t clampU16(int v, uint16_t lo, uint16_t hi) {
  if (v < (int)lo) return lo;
  if (v > (int)hi) return hi;
  return (uint16_t)v;
}

static uint8_t clampChannel1to11(int v) {
  if (v < 1) return 1;
  if (v > 11) return 11;
  return (uint8_t)v;
}

static bool parseHexColor(const String& hex, CRGB& out) {
  String h = hex;
  if (h.startsWith("#")) h = h.substring(1);
  if (h.length() != 6) return false;

  char* endptr = nullptr;
  long v = strtol(h.c_str(), &endptr, 16);
  if (endptr == h.c_str() || *endptr != '\0') return false;

  out.r = (v >> 16) & 0xFF;
  out.g = (v >> 8) & 0xFF;
  out.b = (v) & 0xFF;
  return true;
}

static String modeName(Mode m) {
  switch (m) {
    case MODE_SOLID:   return F("Solid");
    case MODE_RAINBOW: return F("Rainbow");
    case MODE_THEATER: return F("Theater");
    case MODE_PULSE:   return F("Pulse");
    default:           return F("Unknown");
  }
}

static String ipToString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

static String macToString(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// --- Stable device suffix from eFuse MAC (same for AP SSID + mDNS host) -----

static uint16_t deviceMacLast16() {
  uint64_t mac = ESP.getEfuseMac();      // 48-bit base MAC in low bits
  return (uint16_t)(mac & 0xFFFF);       // last 16 bits => 4 hex digits
}

static String deviceSuffix4_upper() {
  char buf[5];
  snprintf(buf, sizeof(buf), "%04X", deviceMacLast16());
  return String(buf);
}

static String deviceSuffix4_lower() {
  String s = deviceSuffix4_upper();
  s.toLowerCase();
  return s;
}

static String buildApSsid() {
  return String(AP_NAME_PREFIX) + "-" + deviceSuffix4_upper();
}

static String buildMdnsHost() {
  return String(MDNS_BASE_NAME) + "-" + deviceSuffix4_lower();
}

// ============================================================================
// NVS (Preferences)
// ============================================================================

static bool loadStaCreds(String& ssid, String& pass) {
  prefs.begin("wifi", true);
  bool enabled = prefs.getBool("enabled", false);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();

  if (!enabled) return false;
  if (ssid.length() == 0) return false;
  return true;
}

static void saveStaCreds(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  prefs.putBool("enabled", true);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

static void disableStaCreds() {
  prefs.begin("wifi", false);
  prefs.putBool("enabled", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end();
}

static void loadLedConfig() {
  prefs.begin("ledcfg", true);
  uint16_t n = (uint16_t)prefs.getUShort("n", DEFAULT_NUM_LEDS);
  prefs.end();

  if (n < MIN_NUM_LEDS || n > MAX_NUM_LEDS) n = DEFAULT_NUM_LEDS;
  g_numLeds = n;
}

static void saveLedConfig(uint16_t n) {
  prefs.begin("ledcfg", false);
  prefs.putUShort("n", n);
  prefs.end();
}

// --- AP Channel NVS (own structure / namespace) -----------------------------

static bool loadApChannelFromNvs(uint8_t& outCh) {
  prefs.begin("apcfg", true);
  bool has = prefs.getBool("set", false);
  uint8_t ch = (uint8_t)prefs.getUChar("ch", 0);
  prefs.end();

  if (!has) return false;
  if (ch < 1 || ch > 11) return false;

  outCh = ch;
  return true;
}

static void saveApChannelToNvs(uint8_t ch) {
  ch = clampChannel1to11(ch);
  prefs.begin("apcfg", false);
  prefs.putBool("set", true);
  prefs.putUChar("ch", ch);
  prefs.end();
}

// Derive 1..11 using modulo on DEVICE base MAC (stable)
static uint8_t deriveApChannelFromDeviceMac() {
  uint16_t v = deviceMacLast16();
  return (uint8_t)((v % 11) + 1);
}

static uint8_t selectAndPersistApChannel() {
  uint8_t ch;

  // 1) NVS
  if (loadApChannelFromNvs(ch)) return ch;

  // 2) compile-time default (if valid)
  if (AP_DEFAULT_CHANNEL >= 1 && AP_DEFAULT_CHANNEL <= 11) {
    ch = (uint8_t)AP_DEFAULT_CHANNEL;
    saveApChannelToNvs(ch);
    return ch;
  }

  // 3) derive from DEVICE MAC
  ch = deriveApChannelFromDeviceMac();
  saveApChannelToNvs(ch);
  return ch;
}

// ============================================================================
// FastLED init (fixed pin) — single controller per boot
// ============================================================================

static void initLedsOrDie(uint16_t count) {
  if (g_leds) {
    delete[] g_leds;
    g_leds = nullptr;
  }

  g_leds = new CRGB[count];
  if (!g_leds) {
    Serial.println("[FATAL] LED buffer allocation failed.");
    while (true) { delay(1000); }
  }

  FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_MA);
  FastLED.setBrightness(st.brightness);

  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(g_leds, count);

  FastLED.clear(true);
}

// ============================================================================
// mDNS
// ============================================================================

static bool startMdns(const String& host) {
  MDNS.end();
  if (!MDNS.begin(host.c_str())) return false;
  MDNS.addService("http", "tcp", 80);
  return true;
}

// ============================================================================
// WiFi start/maintain
// ============================================================================

static void startSoftAP() {
  WiFi.mode(WIFI_AP);

  g_apSsid   = buildApSsid();
  g_mdnsHost = buildMdnsHost();

  g_apChannel = selectAndPersistApChannel();

  bool ok = WiFi.softAP(g_apSsid.c_str(), AP_PASS, g_apChannel, AP_HIDDEN);
  if (!ok) {
    WiFi.softAP(g_apSsid.c_str(), nullptr, g_apChannel, AP_HIDDEN);
  }

  g_runMode = RUN_AP;
  startMdns(g_mdnsHost);
}

static bool tryStartSTAFromSaved(uint32_t timeoutMs) {
  String ssid, pass;
  if (!loadStaCreds(ssid, pass)) return false;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  g_mdnsHost = buildMdnsHost();
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      g_runMode = RUN_STA;
      startMdns(g_mdnsHost);
      return true;
    }

    // Visual cue while connecting: LED0 blinks yellow
    if (g_leds && g_numLeds > 0) {
      bool on = ((millis() / 250) % 2) == 0;
      fill_solid(g_leds, g_numLeds, CRGB::Black);
      g_leds[0] = on ? CRGB::Yellow : CRGB::Black;
      FastLED.show();
    }

    delay(10);
    yield();
  }

  WiFi.disconnect(true, true);
  return false;
}

static void wifiMaintain() {
  if (!STA_ALLOW_RECONNECT) return;
  if (g_runMode != RUN_STA) return;

  uint32_t now = millis();
  if (now - tWiFi < WIFI_MAINTAIN_MS) return;
  tWiFi = now;

  if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
}

// ============================================================================
// LED effects
// ============================================================================

static void showPowerOff() {
  if (!g_leds) return;
  fill_solid(g_leds, g_numLeds, CRGB::Black);
}

static void renderSolid() {
  if (!g_leds) return;
  fill_solid(g_leds, g_numLeds, st.color);
}

static void renderRainbowTick() {
  if (!g_leds) return;
  for (int i = 0; i < (int)g_numLeds; i++) {
    g_leds[i] = CHSV((rainbowHue + i * 6) & 0xFF, 255, 255);
  }
  rainbowHue += 2;
}

static void renderTheaterTick() {
  if (!g_leds) return;
  fill_solid(g_leds, g_numLeds, CRGB::Black);
  for (int i = 0; i < (int)g_numLeds; i++) {
    if ((i + theaterIndex) % 3 == 0) g_leds[i] = st.color;
  }
  theaterIndex = (theaterIndex + 1) % 3;
}

static void renderPulseTick() {
  if (!g_leds) return;

  uint8_t v = sin8((millis() / 6) & 0xFF);
  uint8_t scaled = (uint8_t)map(v, 0, 255, 20, 255);

  fill_solid(g_leds, g_numLeds, st.color);
  for (int i = 0; i < (int)g_numLeds; i++) {
    g_leds[i].nscale8_video(scaled);
  }
}

// ============================================================================
// Web UI
// ============================================================================

static String wifiSummaryHtml() {
  String s;
  s.reserve(1600);

  s += F("<b>mDNS:</b> http://");
  s += htmlEscape(g_mdnsHost);
  s += F(".local/");

  if (g_runMode == RUN_AP) {
    s += F("<br><b>Mode:</b> AP");
    s += F("<br><b>SSID:</b> ");
    s += htmlEscape(g_apSsid);
    s += F("<br><b>IP:</b> ");
    s += htmlEscape(ipToString(WiFi.softAPIP()));
    s += F("<br><b>Channel:</b> ");
    s += String(g_apChannel);
    s += F("<br><small>AP mode tip: if .local fails, use 192.168.4.1</small>");
  } else {
    s += F("<br><b>Mode:</b> STA");
    s += F("<br><b>Status:</b> ");
    s += (WiFi.status() == WL_CONNECTED ? F("connected") : F("disconnected"));
    s += F("<br><b>SSID:</b> ");
    s += htmlEscape(WiFi.SSID());
    s += F("<br><b>IP:</b> ");
    s += htmlEscape(ipToString(WiFi.localIP()));
    if (WiFi.status() == WL_CONNECTED) {
      s += F("<br><b>RSSI:</b> ");
      s += String(WiFi.RSSI());
      s += F(" dBm");
    }
    s += F("<br><b>AP Channel (saved):</b> ");
    s += String(g_apChannel);
  }

  return s;
}

static String ledColorHex() {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", st.color.r, st.color.g, st.color.b);
  return String(buf);
}

static String buildPage(const String& msg = "") {
  String page;
  page.reserve(7000);

  page += F("<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>");
  page += F("<title>ESP32-C3 LEDs</title>");
  page += F("<style>body{font-family:system-ui;margin:16px;max-width:760px} ");
  page += F(".card{padding:14px;border:1px solid #ddd;border-radius:10px;margin-bottom:12px} ");
  page += F("label{display:block;margin:10px 0 6px} input,select{font-size:16px;padding:8px;width:100%} ");
  page += F("button{font-size:16px;padding:10px 14px;margin-top:12px;width:100%} ");
  page += F(".msg{padding:10px;border-radius:10px;background:#f6f6f6;border:1px solid #e5e5e5;margin-bottom:12px}</style>");
  page += F("</head><body>");
  page += F("<h2>ESP32-C3 WS2812 Controller</h2>");

  if (msg.length()) {
    page += F("<div class='msg'>");
    page += htmlEscape(msg);
    page += F("</div>");
  }

  // WiFi status card + AP channel setting
  page += F("<div class='card'><b>Wi-Fi</b><br>");
  page += wifiSummaryHtml();
  page += F("<hr style='border:none;border-top:1px solid #eee;margin:12px 0'>");
  page += F("<b>Wi-Fi Settings</b><br>");
  page += F("<form method='POST' action='/wifi_apch_save'>");
  page += F("<label>AP Channel (1-11)</label>");
  page += F("<input name='apch' type='number' min='1' max='11' value='");
  page += String(g_apChannel);
  page += F("'>");
  page += F("<button type='submit'>Save AP Channel</button>");
  page += F("</form>");
  page += F("</div>");

  // LED Configuration card (stored in NVS; requires reboot)
  page += F("<form class='card' method='POST' action='/ledcfg_save'>");
  page += F("<b>LED Configuration</b><br>");
  page += F("<small>Changing this saves to the device and reboots to apply.</small>");

  page += F("<label>Number of LEDs (max ");
  page += String(MAX_NUM_LEDS);
  page += F(")</label>");
  page += F("<input name='n' type='number' min='1' max='");
  page += String(MAX_NUM_LEDS);
  page += F("' value='");
  page += String(g_numLeds);
  page += F("'>");

  page += F("<button type='submit'>Save LED Configuration (Reboot)</button>");
  page += F("</form>");

  // LED Controls card
  page += F("<form class='card' method='GET' action='/set_led'>");
  page += F("<b>LED Controls</b>");

  page += F("<label>Power</label><select name='p'>");
  page += F("<option value='1'"); if (st.power) page += F(" selected"); page += F(">On</option>");
  page += F("<option value='0'"); if (!st.power) page += F(" selected"); page += F(">Off</option>");
  page += F("</select>");

  page += F("<label>Brightness (0-255)</label>");
  page += F("<input name='b' type='number' min='0' max='255' value='"); page += String(st.brightness); page += F("'>");

  page += F("<label>Color</label>");
  page += F("<input name='c' type='color' value='"); page += ledColorHex(); page += F("'>");

  page += F("<label>Mode</label><select name='m'>");
  for (int i = 0; i < 4; i++) {
    page += F("<option value='"); page += String(i); page += F("'");
    if ((int)st.mode == i) page += F(" selected");
    page += F(">"); page += htmlEscape(modeName((Mode)i)); page += F("</option>");
  }
  page += F("</select>");

  page += F("<button type='submit'>Apply LED Settings</button>");
  page += F("</form>");

  // WiFi actions
  if (g_runMode == RUN_AP) {
    page += F("<form class='card' method='POST' action='/wifi_save_sta'>");
    page += F("<b>Switch to STA mode</b><br>");
    page += F("<small>Enter your router Wi-Fi credentials. They will be saved to the device.</small>");

    page += F("<label>Wi-Fi SSID</label>");
    page += F("<input name='ssid' type='text' placeholder='Your Wi-Fi name' required>");

    page += F("<label>Wi-Fi Password</label>");
    page += F("<input name='pass' type='password' placeholder='Your Wi-Fi password'>");

    page += F("<button type='submit'>Save & Switch to STA</button>");
    page += F("</form>");
  } else {
    page += F("<div class='card'>");
    page += F("<b>WiFi Actions</b><br>");
    page += F("<form method='POST' action='/wifi_switch_ap'><button type='submit'>Switch to AP mode (keep saved WiFi)</button></form>");
    page += F("<form method='POST' action='/wifi_forget'><button type='submit'>Forget saved WiFi (AP-only)</button></form>");
    page += F("</div>");
  }

  // Quick links
  page += F("<div class='card'><b>Quick links</b><br>");
  page += F("<a href='/'>Refresh</a><br>");
  page += F("<a href='/solid'>Solid</a> | <a href='/rainbow'>Rainbow</a> | <a href='/theater'>Theater</a> | <a href='/pulse'>Pulse</a>");
  page += F("</div>");

  page += F("</body></html>");
  return page;
}

// ============================================================================
// HTTP handlers
// ============================================================================

static void handleRoot() {
  server.send(200, "text/html", buildPage());
}

static void handleSetLed() {
  if (server.hasArg("p")) st.power = (server.arg("p").toInt() != 0);
  if (server.hasArg("b")) st.brightness = clampU8(server.arg("b").toInt());

  if (server.hasArg("c")) {
    CRGB parsed;
    if (parseHexColor(server.arg("c"), parsed)) st.color = parsed;
  }

  if (server.hasArg("m")) {
    int m = server.arg("m").toInt();
    if (m >= 0 && m <= 3) st.mode = (Mode)m;
  }

  FastLED.setBrightness(st.brightness);

  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "OK");
}

static void sendMsgPage(const String& msg) {
  server.send(200, "text/html", buildPage(msg));
}

static void handleLedCfgSave() {
  if (!server.hasArg("n")) {
    sendMsgPage("Missing LED configuration field.");
    return;
  }

  int nIn = server.arg("n").toInt();
  uint16_t n = clampU16(nIn, MIN_NUM_LEDS, MAX_NUM_LEDS);

  saveLedConfig(n);

  server.send(200, "text/html",
              String("<html><body><h3>Saved.</h3><p>Rebooting to apply LED configuration...</p></body></html>"));
  delay(250);
  ESP.restart();
}

static void handleWifiApChannelSave() {
  if (!server.hasArg("apch")) {
    sendMsgPage("Missing AP channel.");
    return;
  }

  int v = server.arg("apch").toInt();
  if (v < 1 || v > 11) {
    sendMsgPage("AP channel must be between 1 and 11.");
    return;
  }

  uint8_t ch = (uint8_t)v;
  saveApChannelToNvs(ch);
  g_apChannel = ch;

  if (g_runMode == RUN_AP) {
    WiFi.softAPdisconnect(true);
    startSoftAP();
    sendMsgPage(String("Saved AP channel to NVS and restarted AP on channel ") + String(g_apChannel) + ".");
    return;
  }

  sendMsgPage(String("Saved AP channel to NVS (") + String(g_apChannel) + "). It will be used next time AP starts.");
}

static void handleWifiSaveSta() {
  if (!server.hasArg("ssid")) {
    sendMsgPage("Missing SSID.");
    return;
  }

  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";

  ssid.trim();
  pass.trim();

  if (ssid.length() == 0) {
    sendMsgPage("SSID cannot be empty.");
    return;
  }

  saveStaCreds(ssid, pass);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  g_mdnsHost = buildMdnsHost();
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t start = millis();
  while (millis() - start < STA_CONNECT_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      g_runMode = RUN_STA;
      startMdns(g_mdnsHost);
      sendMsgPage(String("Connected to ") + ssid +
                  ". IP: " + ipToString(WiFi.localIP()) +
                  ". mDNS: http://" + g_mdnsHost + ".local/");
      return;
    }
    delay(10);
    yield();
  }

  WiFi.disconnect(true, true);
  startSoftAP();
  sendMsgPage("Failed to connect to saved WiFi. Stayed in AP mode.");
}

static void handleWifiSwitchAp() {
  WiFi.disconnect(true, true);
  startSoftAP();
  sendMsgPage(String("Switched to AP mode. Channel: ") + g_apChannel);
}

static void handleWifiForget() {
  disableStaCreds();
  WiFi.disconnect(true, true);
  startSoftAP();
  sendMsgPage(String("Saved WiFi cleared. AP-only mode. Channel: ") + g_apChannel);
}

// Quick mode links
static void handleModeSolid()   { st.mode = MODE_SOLID;   server.sendHeader("Location", "/"); server.send(302, "text/plain", "OK"); }
static void handleModeRainbow() { st.mode = MODE_RAINBOW; server.sendHeader("Location", "/"); server.send(302, "text/plain", "OK"); }
static void handleModeTheater() { st.mode = MODE_THEATER; server.sendHeader("Location", "/"); server.send(302, "text/plain", "OK"); }
static void handleModePulse()   { st.mode = MODE_PULSE;   server.sendHeader("Location", "/"); server.send(302, "text/plain", "OK"); }

static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ============================================================================
// Serial logging
// ============================================================================

static void printBootInfo() {
  Serial.println();
  Serial.println("========================================");
  Serial.println(" ESP32-C3 WS2812 WiFi Controller");
  Serial.println("========================================");
  Serial.print  ("mDNS hostname  : "); Serial.println(g_mdnsHost);
  Serial.print  ("mDNS URL       : http://"); Serial.print(g_mdnsHost); Serial.println(".local/");
  Serial.println("----------------------------------------");
  Serial.print  ("LED data pin   : "); Serial.println(LED_DATA_PIN);
  Serial.print  ("NUM LEDs       : "); Serial.println(g_numLeds);
  Serial.print  ("MAX_MA         : "); Serial.println(MAX_MA);

  if (g_runMode == RUN_AP) {
    uint8_t apmac[6];
    WiFi.softAPmacAddress(apmac);
    Serial.println("----------------------------------------");
    Serial.println("WiFi MODE      : AP");
    Serial.print  ("AP SSID        : "); Serial.println(g_apSsid);
    Serial.print  ("AP CHANNEL     : "); Serial.println(g_apChannel);
    Serial.print  ("AP IP          : "); Serial.println(WiFi.softAPIP());
    Serial.print  ("AP MAC         : "); Serial.println(macToString(apmac));
    Serial.println("Access via: http://192.168.4.1/");
  } else {
    uint8_t stamac[6];
    WiFi.macAddress(stamac);
    Serial.println("----------------------------------------");
    Serial.println("WiFi MODE      : STA");
    Serial.print  ("SSID           : "); Serial.println(WiFi.SSID());
    Serial.print  ("IP             : "); Serial.println(WiFi.localIP());
    Serial.print  ("RSSI           : "); Serial.println(WiFi.RSSI());
    Serial.print  ("STA MAC        : "); Serial.println(macToString(stamac));
    Serial.print  ("Access via     : http://"); Serial.print(g_mdnsHost); Serial.println(".local/ (or device IP)");
    Serial.print  ("Saved AP ch    : "); Serial.println(g_apChannel);
  }

  Serial.println("========================================");
}

static void printPeriodicStatus() {
  uint32_t now = millis();
  if (now - tInfo < SERIAL_INFO_MS) return;
  tInfo = now;

  Serial.print("[Status] wifi=");
  Serial.print(g_runMode == RUN_AP ? "AP" : "STA");
  Serial.print("  host=");
  Serial.print(g_mdnsHost);
  Serial.print(".local");

  if (g_runMode == RUN_AP) {
    Serial.print("  ip=");
    Serial.print(WiFi.softAPIP());
    Serial.print("  ch=");
    Serial.print(g_apChannel);
  } else {
    Serial.print("  ip=");
    Serial.print(WiFi.localIP());
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("  rssi=");
      Serial.print(WiFi.RSSI());
    } else {
      Serial.print("  rssi=NA");
    }
    Serial.print("  apch=");
    Serial.print(g_apChannel);
  }

  Serial.print("  pin=");
  Serial.print(LED_DATA_PIN);
  Serial.print("  n=");
  Serial.print(g_numLeds);
  Serial.print("  mode=");
  Serial.println(modeName(st.mode));
}

// ============================================================================
// Arduino setup / loop
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(50);

  // Build identity strings once (stable suffix)
  g_apSsid   = buildApSsid();
  g_mdnsHost = buildMdnsHost();

  // 1) Load LED config
  loadLedConfig();
  if (g_numLeds < MIN_NUM_LEDS || g_numLeds > MAX_NUM_LEDS) {
    g_numLeds = DEFAULT_NUM_LEDS;
  }

  // 2) Init LEDs
  initLedsOrDie(g_numLeds);

  // 3) Load AP channel into RAM for display (even if we start STA)
  {
    uint8_t ch;
    if (loadApChannelFromNvs(ch)) g_apChannel = ch;
    else if (AP_DEFAULT_CHANNEL >= 1 && AP_DEFAULT_CHANNEL <= 11) g_apChannel = (uint8_t)AP_DEFAULT_CHANNEL;
    else g_apChannel = deriveApChannelFromDeviceMac();
  }

  // 4) Boot WiFi: try STA; else AP
  bool staOk = tryStartSTAFromSaved(STA_CONNECT_TIMEOUT_MS);
  if (!staOk) startSoftAP();

  // 5) HTTP routes
  server.on("/", handleRoot);
  server.on("/set_led", HTTP_GET, handleSetLed);

  server.on("/ledcfg_save", HTTP_POST, handleLedCfgSave);

  server.on("/wifi_apch_save", HTTP_POST, handleWifiApChannelSave);

  server.on("/wifi_save_sta", HTTP_POST, handleWifiSaveSta);
  server.on("/wifi_switch_ap", HTTP_POST, handleWifiSwitchAp);
  server.on("/wifi_forget", HTTP_POST, handleWifiForget);

  server.on("/solid", handleModeSolid);
  server.on("/rainbow", handleModeRainbow);
  server.on("/theater", handleModeTheater);
  server.on("/pulse", handleModePulse);

  server.onNotFound(handleNotFound);
  server.begin();

  printBootInfo();
}

void loop() {
  server.handleClient();

  if (g_runMode == RUN_STA) wifiMaintain();

  printPeriodicStatus();

  if (!st.power) {
    showPowerOff();
    FastLED.show();
    return;
  }

  uint32_t now = millis();
  switch (st.mode) {
    case MODE_SOLID:
      renderSolid();
      break;

    case MODE_RAINBOW:
      if (now - tAnim >= RAINBOW_INTERVAL_MS) {
        tAnim = now;
        renderRainbowTick();
      }
      break;

    case MODE_THEATER:
      if (now - tAnim >= THEATER_INTERVAL_MS) {
        tAnim = now;
        renderTheaterTick();
      }
      break;

    case MODE_PULSE:
      if (now - tAnim >= PULSE_INTERVAL_MS) {
        tAnim = now;
        renderPulseTick();
      }
      break;
  }

  FastLED.show();
}
