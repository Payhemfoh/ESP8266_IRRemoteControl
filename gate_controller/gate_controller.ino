// ESP32 smart autogate controller (Gmatic indoor panel replacement)
// 4-ch active-LOW relay: GPIO13->IN1 (BOTH), 14->IN2 (LEFT/pedestrian), 15->IN3 (LIGHT), 16->IN4 (LOCK)
// Board: "ESP32 Dev Module" (NOT "AI Thinker ESP32-CAM": its PSRAM flags crash WiFiManager)
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>   // tzapu/WiFiManager
#include <ArduinoOTA.h>

#include "secrets.h"  // copy secrets.h.example to secrets.h and fill in

// ---- Config ----
// Wi-Fi is set up through the WiFiManager portal (no SSID or IP in the code).
// Give the board a fixed address with a DHCP reservation on the router.
const char* HOSTNAME = "gate";   // mDNS: gate.local
const char* AUTH_USER = SECRET_AUTH_USER;
const char* AUTH_PASS = SECRET_AUTH_PASS;

const uint32_t PULSE_MS = 500;

struct Channel {
  const char* id;
  const char* label;
  uint8_t pin;
  uint32_t offAt;  // millis() when pulse ends, 0 = idle
};

Channel channels[] = {
  // ESP32-CAM (AI Thinker) pins; for a regular ESP32 devkit use 18, 19, 21, 22
  {"both",   "Both Gates",        13, 0},
  {"left",   "Left Gate (Ped.)",  14, 0},
  {"light",  "Light (I.R.)",      15, 0},
  {"lock",   "System Lock",       16, 0},
};
const size_t NCH = sizeof(channels) / sizeof(channels[0]);

WebServer server(80);

void pulse(Channel& c) {
  digitalWrite(c.pin, LOW);  // active-LOW: energise relay
  c.offAt = millis() + PULSE_MS;
  if (c.offAt == 0) c.offAt = 1;
}

String page() {
  String h = F("<!doctype html><html><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Gate</title><style>"
               "#s{margin-top:16px}"
               "body{font-family:system-ui;margin:0;padding:16px;background:#111;color:#eee}"
               "main{display:grid;grid-template-columns:1fr 1fr;gap:12px}"
               "button{padding:22px 8px;font-size:1.1rem;"
               "border:0;border-radius:12px;background:#2b6cff;color:#fff}"
               "button.w{background:#d33}"
               "button:active{opacity:.7}"
               "button:active{background:#1747b5}#s{text-align:center;min-height:1.5em}"
               "</style></head><body><h2>Gate Control</h2>");
  h += "<main>";
  for (size_t i = 0; i < NCH; i++) {
    h += "<button";
    if (String(channels[i].id) == "panic") h += " class=w";
    h += " onclick=\"fire('";
    h += channels[i].id;
    h += "')\">";
    h += channels[i].label;
    h += "</button>";
  }
  h += F("</main><div id=s></div><script>"
         "function fire(id){fetch('/api/'+id,{method:'POST'})"
         ".then(r=>r.ok?'Sent':'Error '+r.status).catch(()=>'Offline')"
         ".then(t=>{document.getElementById('s').textContent=t})}"
         "</script></body></html>");
  return h;
}

bool authed() {
  if (server.authenticate(AUTH_USER, AUTH_PASS)) return true;
  server.requestAuthentication();
  return false;
}

void handleRoot() {
  if (!authed()) return;
  server.send(200, "text/html", page());
}

void handleApi() {
  if (!authed()) return;
  if (server.method() != HTTP_POST) { server.send(405, "text/plain", "POST only"); return; }
  String id = server.uri().substring(5);  // after "/api/"
  for (size_t i = 0; i < NCH; i++) {
    if (id == channels[i].id) {
      pulse(channels[i]);
      server.send(200, "text/plain", "ok");
      return;
    }
  }
  server.send(404, "text/plain", "unknown channel");
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);

  // Hold BOOT (GPIO0) for 3 s at power-up to forget the saved WiFi
  pinMode(0, INPUT_PULLUP);
  delay(100);
  unsigned long held = millis();
  while (digitalRead(0) == LOW && millis() - held < 3000) delay(50);
  bool clearWifi = (digitalRead(0) == LOW);

  WiFiManager wm;
  if (clearWifi) { Serial.println("Clearing saved WiFi"); wm.resetSettings(); }
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(180);  // then restart and retry (e.g. router still booting)
  if (!wm.autoConnect(SETUP_AP_NAME, SETUP_AP_PASSWORD)) {
    Serial.println("WiFi setup failed/timed out, restarting");
    ESP.restart();
  }
}

void setup() {
  // Drive HIGH immediately so relays stay off through boot
  for (size_t i = 0; i < NCH; i++) {
    digitalWrite(channels[i].pin, HIGH);
    pinMode(channels[i].pin, OUTPUT);
    digitalWrite(channels[i].pin, HIGH);
  }
  Serial.begin(115200);
  connectWifi();
  Serial.println(WiFi.localIP());
  MDNS.begin(HOSTNAME);

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound([]() {
    if (server.uri().startsWith("/api/")) handleApi();
    else server.send(404, "text/plain", "not found");
  });
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  uint32_t now = millis();
  for (size_t i = 0; i < NCH; i++) {
    if (channels[i].offAt && (int32_t)(now - channels[i].offAt) >= 0) {
      digitalWrite(channels[i].pin, HIGH);
      channels[i].offAt = 0;
    }
  }
}
