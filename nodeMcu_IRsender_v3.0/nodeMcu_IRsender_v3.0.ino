#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>  // sync server: ESPAsyncWebServer clashes with WiFiManager
#include <ESP8266mDNS.h>
#include <WiFiManager.h>   // tzapu/WiFiManager
#include <ArduinoOTA.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <LittleFS.h>
#include <map>
#include "secrets.h"

// Wi-Fi credentials
const char* HOSTNAME = "esp8266";


// IR pin
const int pin_send = D2;
IRsend irsend(pin_send);

// IR codes as constants
const uint32_t IR_POWER   = 0xE0E040BF;
const uint32_t IR_SOURCE  = 0xE0E0807F;
const uint32_t IR_VOLUP   = 0xE0E0E01F;
const uint32_t IR_VOLDOWN = 0xE0E0D02F;
const uint32_t IR_MUTE    = 0xE0E0F00F;
const uint32_t IR_SMART   = 0xE0E09E61;
const uint32_t IR_UP      = 0xE0E006F9;
const uint32_t IR_DOWN    = 0xE0E08679;
const uint32_t IR_LEFT    = 0xE0E0A659;
const uint32_t IR_RIGHT   = 0xE0E046B9;
const uint32_t IR_ENTER   = 0xE0E016E9;
const uint32_t IR_RETURN  = 0xE0E01AE5;

// Mapping table
std::map<String, uint32_t> irMap = {
  {"POWER", IR_POWER},
  {"SOURCE", IR_SOURCE},
  {"VOLUP", IR_VOLUP},
  {"VOLDOWN", IR_VOLDOWN},
  {"MUTE", IR_MUTE},
  {"SMART", IR_SMART},
  {"UP", IR_UP},
  {"DOWN", IR_DOWN},
  {"LEFT", IR_LEFT},
  {"RIGHT", IR_RIGHT},
  {"ENTER", IR_ENTER},
  {"RETURN", IR_RETURN}
};

// Sync server: handlers run from loop() via handleClient(), so requests are
// naturally serialized and IR sending never overlaps.
ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  irsend.begin();

  // WiFiManager stores the credentials in flash, so leave WiFi.persistent at its default
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);  // lower latency
  WiFi.hostname(HOSTNAME);

  // Hold FLASH (GPIO0) for 3 s at power-up to forget the saved WiFi
  pinMode(0, INPUT_PULLUP);
  delay(100);
  unsigned long held = millis();
  while (digitalRead(0) == LOW && millis() - held < 3000) delay(50);
  bool clearWifi = (digitalRead(0) == LOW);

  String mac = WiFi.macAddress();
  Serial.println("MAC address: " + mac);

  WiFiManager wm;
  String macHtml = "<p style='text-align:center'>MAC address: <b>" + mac + "</b></p>";
  WiFiManagerParameter macInfo(macHtml.c_str()); // custom HTML on the portal page
  wm.addParameter(&macInfo);
  if (clearWifi) { Serial.println("Clearing saved WiFi"); wm.resetSettings(); }
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(180); // then restart and retry (e.g. router still booting)
  if (!wm.autoConnect(SETUP_AP_NAME, SETUP_AP_PASSWORD)) {
    Serial.println("WiFi setup failed/timed out, restarting");
    ESP.restart();
  }
  Serial.printf("Connected. IP: %s\n", WiFi.localIP().toString().c_str());

  // Start mDNS + advertise service
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS responder started at http://esp8266.local/");
  }

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { Serial.println("OTA start"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA done"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA error %u\n", e); });
  ArduinoOTA.begin();

  // Initialize LittleFS
  if (LittleFS.begin()) {
    server.serveStatic("/", LittleFS, "/index.html");
  } else {
    // Keep /control working even without the UI
    Serial.println("LittleFS mount failed - upload the data folder");
    server.on("/", HTTP_GET, []() {
      server.send(503, "text/plain", "UI missing: upload LittleFS data. /control?BTN=... still works.");
    });
  }

  server.on("/control", HTTP_GET, []() {
    if (!server.hasArg("BTN")) {
      server.send(400, "text/plain", "No BTN parameter");
      return;
    }
    String btn = server.arg("BTN");
    auto it = irMap.find(btn);
    if (it == irMap.end()) {
      server.send(400, "text/plain", "Unknown button");
      return;
    }
    irsend.sendSAMSUNG(it->second);
    server.send(200, "text/plain", "IR command sent: " + btn);
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  ArduinoOTA.handle();
  MDNS.update();
  server.handleClient();

  // Non-blocking Wi-Fi watchdog (ESP8266 also auto-reconnects on its own)
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi lost, reconnecting...");
      WiFi.reconnect();
    }
  }
}
