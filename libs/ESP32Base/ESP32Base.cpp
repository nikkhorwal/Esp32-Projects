#include "ESP32Base.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <math.h>

static const unsigned long WIFI_CONNECT_TIMEOUT = 15000UL;
static const int MAX_WIFI_NETWORKS = 5;
static const uint16_t LOG_PORT = 23;
static const char* OTA_HOSTNAME = "esp32-s3-board";
static const size_t COMMAND_BUFFER_SIZE = 96;

static Preferences preferences;
static WiFiServer logServer(LOG_PORT);
static WiFiClient logClient;
static bool tcpStarted = false;
static bool otaStarted = false;
static char commandBuffer[COMMAND_BUFFER_SIZE];
static size_t commandLength = 0;

static void logLine(const String& message) {
  Serial0.println(message);
}

void ESP32BaseSerialClass::begin(unsigned long baud) {
  Serial.begin(baud);
  started = true;
}

void ESP32BaseSerialClass::end() {
  Serial.end();
  started = false;
}

ESP32BaseSerialClass::operator bool() const { return started; }

size_t ESP32BaseSerialClass::write(uint8_t c) {
  size_t n = Serial.write(c);
  if (tcpStarted && logClient && logClient.connected()) logClient.write(c);
  return n;
}

size_t ESP32BaseSerialClass::write(const uint8_t* buffer, size_t size) {
  size_t n = Serial.write(buffer, size);
  if (tcpStarted && logClient && logClient.connected()) logClient.write(buffer, size);
  return n;
}

ESP32BaseSerialClass ESP32BaseSerial;
ESP32BaseClass ESP32Base;

static String getSaved(int slot, const char* key) {
  preferences.begin("wifi", true);
  String value = preferences.getString((String(key) + String(slot)).c_str(), "");
  preferences.end();
  return value;
}

static bool connectToWiFi(const String& ssid, const String& password, int rssi) {
  if (rssi <= -126) logLine("Trying: " + ssid);
  else logLine("Trying: " + ssid + " (" + String(rssi) + " dBm)");

  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= WIFI_CONNECT_TIMEOUT) {
      logLine("  -> Failed: connection timeout");
      WiFi.disconnect();
      return false;
    }
    delay(250);
  }

  logLine("  -> Connected");
  logLine("  IP: " + WiFi.localIP().toString());
  logLine("  RSSI: " + String(WiFi.RSSI()) + " dBm");
  return true;
}

static bool connectToSavedWiFi() {
  String ssids[MAX_WIFI_NETWORKS];
  String passwords[MAX_WIFI_NETWORKS];
  int savedCount = 0;

  for (int i = 0; i < MAX_WIFI_NETWORKS; ++i) {
    ssids[i] = getSaved(i + 1, "ssid");
    passwords[i] = getSaved(i + 1, "password");
    if (ssids[i].length()) savedCount++;
  }

  logLine("Saved WiFi networks: " + String(savedCount));
  for (int i = 0; i < MAX_WIFI_NETWORKS; ++i) {
    if (ssids[i].length()) logLine("  " + String(i + 1) + ": " + ssids[i]);
  }

  if (!savedCount) {
    logLine("No saved WiFi networks found.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  logLine("Scanning nearby WiFi networks...");
  int n = WiFi.scanNetworks();
  if (n < 0) n = 0;
  logLine("Nearby networks found: " + String(n));

  int bestSlot = -1;
  int bestRSSI = -1000;

  for (int i = 0; i < n; ++i) {
    String found = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    for (int s = 0; s < MAX_WIFI_NETWORKS; ++s) {
      if (ssids[s].length() && found == ssids[s] && rssi > bestRSSI) {
        bestRSSI = rssi;
        bestSlot = s;
      }
    }
  }
  WiFi.scanDelete();

  if (bestSlot >= 0) {
    logLine("Best saved network: " + ssids[bestSlot] + " (" + String(bestRSSI) + " dBm)");
  } else {
    logLine("No saved WiFi network is currently visible.");
  }

  if (bestSlot >= 0 && connectToWiFi(ssids[bestSlot], passwords[bestSlot], bestRSSI)) return true;

  if (bestSlot >= 0) logLine("Best network failed. Trying other saved networks...");
  for (int s = 0; s < MAX_WIFI_NETWORKS; ++s) {
    if (!ssids[s].length() || s == bestSlot) continue;
    if (connectToWiFi(ssids[s], passwords[s], -127)) return true;
  }

  return false;
}

static void sendWelcomeToClient() {
  if (!logClient || !logClient.connected()) return;
  logClient.println("ESP32Base TCP console ready.");
  logClient.println("Type 'help' for commands.");
}

static void printHelp() {
  logLine("");
  logLine("ESP32Base Commands");
  logLine("------------------");
  logLine("temp       Show ESP32 internal temperature");
  logLine("status     Show complete ESP32 status");
  logLine("wifi       Scan and show available WiFi networks");
}

static void printWiFiInfo() {
  logLine("");
  logLine("WiFi Networks");
  logLine("-------------");

  int n = WiFi.scanNetworks();
  if (n < 0) n = 0;
  logLine("Available networks: " + String(n));

  String connectedSSID = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    String line = String(i + 1) + ": " + (ssid.length() ? ssid : "<hidden>");
    line += " | RSSI: " + String(WiFi.RSSI(i)) + " dBm";
    if (ssid == connectedSSID) line += " | CONNECTED";
    logLine(line);
  }

  if (WiFi.status() == WL_CONNECTED) {
    logLine("");
    logLine("Connected WiFi: " + WiFi.SSID());
    logLine("IP: " + WiFi.localIP().toString());
    logLine("RSSI: " + String(WiFi.RSSI()) + " dBm");
  } else {
    logLine("");
    logLine("Connected WiFi: none");
    logLine("IP: Unavailable");
  }

  WiFi.scanDelete();
}

static void processCommand(String command) {
  command.trim();
  command.toLowerCase();
  if (!command.length()) return;

  if (command == "help" || command == "?") {
    printHelp();
  } else if (command == "temp" || command == "temperature") {
    float t = ESP32Base.temperature();
    if (isnan(t)) logLine("Temperature: unavailable");
    else logLine("Temperature: " + String(t, 1) + " °C");
  } else if (command == "status") {
    logLine("");
    logLine("ESP32 Status");
    logLine("------------");
    if (WiFi.status() == WL_CONNECTED) {
      logLine("WiFi: " + WiFi.SSID());
      logLine("IP: " + WiFi.localIP().toString());
      logLine("mDNS: " + String(OTA_HOSTNAME) + ".local");
      logLine("RSSI: " + String(WiFi.RSSI()) + " dBm");
    } else {
      logLine("WiFi: Disconnected");
      logLine("IP: Unavailable");
      logLine("RSSI: unavailable");
    }
    float t = ESP32Base.temperature();
    if (isnan(t)) logLine("Temperature: unavailable");
    else logLine("Temperature: " + String(t, 1) + " °C");
    logLine("OTA: " + String(otaStarted ? "Ready" : "Not ready"));
    logLine("TCP logs: port " + String(LOG_PORT));
    logLine("Uptime: " + String(millis() / 1000UL) + " s");
  } else if (command == "wifi") {
    printWiFiInfo();
  } else {
    logLine("Unknown command: " + command);
    logLine("Type 'help' for commands.");
  }
}

static void handleClientCommands() {
  if (!logClient || !logClient.connected()) return;

  while (logClient.available()) {
    char c = (char)logClient.read();
    if (c == '\r') continue;
    if (c == '\n') {
      commandBuffer[commandLength] = '\0';
      processCommand(String(commandBuffer));
      commandLength = 0;
      continue;
    }
    if (commandLength < COMMAND_BUFFER_SIZE - 1) commandBuffer[commandLength++] = c;
    else commandLength = 0;
  }
}

static void handleTCPLogs() {
  if (!tcpStarted) return;

  if (!logClient || !logClient.connected()) {
    WiFiClient incoming = logServer.available();
    if (incoming) {
      if (logClient) logClient.stop();
      logClient = incoming;
      logClient.setNoDelay(true);
      commandLength = 0;
      sendWelcomeToClient();
    }
  }

  handleClientCommands();
}

static void startTCPLogs() {
  logServer.begin();
  logServer.setNoDelay(true);
  tcpStarted = true;
}

static void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([](){ logLine("OTA update starting..."); });
  ArduinoOTA.onEnd([](){ logLine("OTA update complete. Rebooting..."); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t){
    if (!t) return;
    unsigned int percent = (p * 100) / t;
    static int last = -1;
    if (percent != last && percent % 10 == 0) {
      last = percent;
      logLine("OTA progress: " + String(percent) + "%");
    }
  });
  ArduinoOTA.onError([](ota_error_t e){
    String m = "OTA error: ";
    if (e == OTA_AUTH_ERROR) m += "Auth failed";
    else if (e == OTA_BEGIN_ERROR) m += "Begin failed";
    else if (e == OTA_CONNECT_ERROR) m += "Connect failed";
    else if (e == OTA_RECEIVE_ERROR) m += "Receive failed";
    else if (e == OTA_END_ERROR) m += "End failed";
    else m += "Unknown error";
    logLine(m);
  });
  ArduinoOTA.begin();
  otaStarted = true;
}

bool ESP32BaseClass::begin() {
  commandLength = 0;
  tcpStarted = false;
  otaStarted = false;

  bool connected = connectToSavedWiFi();
  if (!connected) return false;

  // Start mDNS so board is reachable as esp32-s3-board.local
  if (!MDNS.begin(OTA_HOSTNAME)) {
    logLine("mDNS failed to start");
  } else {
    MDNS.addService("_arduino", "_tcp", 3232);  // OTA service
    MDNS.addService("_telnet", "_tcp", LOG_PORT);  // TCP terminal
    logLine("mDNS: " + String(OTA_HOSTNAME) + ".local");
  }

  startTCPLogs();
  setupOTA();
  return true;
}

void ESP32BaseClass::loop() {
  handleTCPLogs();
  if (otaStarted) ArduinoOTA.handle();
}

bool ESP32BaseClass::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String ESP32BaseClass::ipAddress() {
  return isConnected() ? WiFi.localIP().toString() : "";
}

float ESP32BaseClass::temperature() {
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32)
  return temperatureRead();
#else
  return NAN;
#endif
}
