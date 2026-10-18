#include <WiFi.h>
#include <IRremoteESP8266.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ir_Panasonic.h>
#include <Preferences.h>
#include <WiFiManager.h>   // tzapu/WiFiManager
#include <ArduinoOTA.h>
#include "secrets.h"

// --- Configuration ---
const char* HOSTNAME = "esp32-ac";
const int pin_send = 19;


// --- Command IDs ---
enum AC_CMD {
  CMD_OFF = 0,    CMD_ON = 1,
  CMD_TEMP_UP = 2, CMD_TEMP_DOWN = 3,
  CMD_MODE_COOL = 4, CMD_MODE_DRY = 5, CMD_MODE_FAN = 6,
  CMD_F_AUTO = 10, CMD_F_1 = 11, CMD_F_2 = 12, CMD_F_3 = 13, CMD_F_4 = 14, CMD_F_5 = 15,
  CMD_S_AUTO = 20, CMD_S_1 = 21, CMD_S_2 = 22, CMD_S_3 = 23, CMD_S_4 = 24, CMD_S_5 = 25
};

// --- Globals ---
WebServer server(80);
IRPanasonicAc ac(pin_send); 
QueueHandle_t irQueue; 
Preferences prefs;
volatile bool stateDirty = false;          // set by irTask, cleared by saveState()
volatile unsigned long stateChangedAt = 0;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED; // guards currentState

// --- State Record (Single Source of Truth) ---
struct ACState {
  bool power = false;
  int mode = 0; //0: cool, 1: dry, 2: fan
  int temp = 24;
  int fan = 0;   // 0: Auto, 1-5: Level
  int swing = 0; // 0: Auto, 1-5: Level
} currentState;

// --- HTML & UI ---
const char webpage[] PROGMEM = R"(
<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
  body{display:flex;flex-direction:column;align-items:center;font-family:sans-serif;background:#f0f2f5;padding:20px;color:#333;}
  .card{background:white;padding:25px;border-radius:20px;box-shadow:0 10px 25px rgba(0,0,0,0.1);width:100%;max-width:350px;text-align:center;}
  .temp-display{font-size:64px;font-weight:bold;color:#2c3e50;margin:10px 0;}
  .btn-grp{display:flex;justify-content:center;gap:10px;margin-bottom:15px;flex-wrap:wrap;}
  button{padding:10px;border-radius:8px;border:none;background:#3498db;color:white;cursor:pointer;transition:0.1s;min-width:50px;}
  button:active{transform:scale(0.95);}
  button.active{background:#2ecc71;box-shadow:inset 0 2px 4px rgba(0,0,0,0.2);}
  .pwr{width:120px;height:45px;font-weight:bold;}
  .pwr.on-state{background:#27ae60;} .pwr.off-state{background:#e74c3c;}
  h3{font-size:11px;color:#95a5a6;margin-top:15px;text-transform:uppercase;}
  .status-bar{font-size:11px; color:#bdc3c7; margin-top:15px; border-top:1px solid #eee; padding-top:10px;}
</style></head>
<body>
  <div class='card'>
    <h3>Panasonic AC Pro</h3>
    <div class='temp-display' id='tempOut'>--&deg;C</div>
    <div class='btn-grp'>
      <button onclick='send(3)'>&minus;</button><button onclick='send(2)'>&plus;</button>
    </div>
    <div class='btn-grp'>
      <button id='pwrBtn' class='pwr' onclick='togglePower()'>POWER</button>
    </div>
    <h3>Mode</h3>
    <div class='btn-grp'>
      <button id='m0' onclick='send(4)'>COOL</button>
      <button id='m1' onclick='send(5)'>DRY</button>
      <button id='m2' onclick='send(6)'>FAN</button>
    </div>
    <h3>Fan Speed</h3>
    <div class='btn-grp'>
      <button id='f0' onclick='send(10)'>AUTO</button>
      <button id='f1' onclick='send(11)'>1</button><button id='f2' onclick='send(12)'>2</button>
      <button id='f3' onclick='send(13)'>3</button><button id='f4' onclick='send(14)'>4</button>
      <button id='f5' onclick='send(15)'>5</button>
    </div>
    <h3>Swing</h3>
    <div class='btn-grp'>
      <button id='s0' onclick='send(20)'>AUTO</button>
      <button id='s1' onclick='send(21)'>TOP</button>
      <button id='s2' onclick='send(22)'>2</button>
      <button id='s3' onclick='send(23)'>3</button>
      <button id='s4' onclick='send(24)'>4</button>
      <button id='s5' onclick='send(25)'>LOW</button>
    </div>
    <div class='status-bar' id='syncStatus'>Syncing...</div>
  </div>
<script>
  let powerState = false;
  function updateUI(data) {
    const state = JSON.parse(data);
    powerState = state.power;
    document.getElementById('tempOut').innerHTML = state.temp + '&deg;C';
    
    // Update Power Button Color
    const pbtn = document.getElementById('pwrBtn');
    pbtn.className = state.power ? 'pwr on-state' : 'pwr off-state';
    pbtn.innerHTML = state.power ? 'AC IS ON' : 'AC IS OFF';

    // Highlight Active Fan/Swing Buttons
    document.querySelectorAll('.btn-grp button').forEach(b => b.classList.remove('active'));
    document.getElementById('f'+state.fan)?.classList.add('active');
    document.getElementById('s'+state.swing)?.classList.add('active');
    document.getElementById('m'+state.mode)?.classList.add('active');

    document.getElementById('syncStatus').innerHTML = 'Last Sync: ' + new Date().toLocaleTimeString();
  }

  function fetchStatus() {
    fetch("/getStatus").then(r => r.text()).then(updateUI);
  }

  function send(id){
    fetch("/", {method:"POST", headers:{"Content-Type":"application/x-www-form-urlencoded"}, body:"ID=" + id}).then(() => setTimeout(fetchStatus, 250));
  }

  function togglePower() { send(powerState ? 0 : 1); }

  fetchStatus();
  setInterval(fetchStatus, 30000);
</script>
</body></html>
)";

// --- Persistence (NVS) ---
void saveState() {
  taskENTER_CRITICAL(&stateMux);
  ACState snap = currentState;
  stateDirty = false;
  taskEXIT_CRITICAL(&stateMux);
  prefs.putBool("power", snap.power);
  prefs.putUChar("mode", snap.mode);
  prefs.putUChar("temp", snap.temp);
  prefs.putUChar("fan", snap.fan);
  prefs.putUChar("swing", snap.swing);
}

void loadState() {
  prefs.begin("ac", false);
  currentState.power = prefs.getBool("power", false);
  currentState.mode  = constrain(prefs.getUChar("mode", 0), 0, 2);
  currentState.temp  = constrain(prefs.getUChar("temp", 24), 16, 30);
  currentState.fan   = constrain(prefs.getUChar("fan", 0), 0, 5);
  currentState.swing = constrain(prefs.getUChar("swing", 0), 0, 5);
}

// Push the restored state into the IR library (nothing is transmitted)
void applyStateToLibrary() {
  static const uint8_t modes[] = {kPanasonicAcCool, kPanasonicAcDry, kPanasonicAcFan};
  static const uint8_t fans[] = {kPanasonicAcFanAuto, kPanasonicAcFanMin, kPanasonicAcFanLow,
                                 kPanasonicAcFanMed, kPanasonicAcFanHigh, kPanasonicAcFanMax};
  static const uint8_t swings[] = {kPanasonicAcSwingVAuto, kPanasonicAcSwingVHighest, kPanasonicAcSwingVHigh,
                                   kPanasonicAcSwingVMiddle, kPanasonicAcSwingVLow, kPanasonicAcSwingVLowest};
  ac.setPower(currentState.power);
  ac.setMode(modes[currentState.mode]);
  if (currentState.mode != 2) ac.setTemp(currentState.temp);
  ac.setFan(fans[currentState.fan]);
  ac.setSwingVertical(swings[currentState.swing]);
}

bool isValidCmd(const String& arg, int id) {
  if (arg.length() == 0) return false;
  for (size_t i = 0; i < arg.length(); i++) if (!isDigit(arg[i])) return false;
  return id == CMD_OFF || id == CMD_ON || id == CMD_TEMP_UP || id == CMD_TEMP_DOWN ||
         (id >= CMD_MODE_COOL && id <= CMD_MODE_FAN) ||
         (id >= CMD_F_AUTO && id <= CMD_F_5) ||
         (id >= CMD_S_AUTO && id <= CMD_S_5);
}

void irTask(void * pvParameters) {
  int id;
  for(;;) {
    if (xQueueReceive(irQueue, &id, portMAX_DELAY)) {
      ACState snap;
      taskENTER_CRITICAL(&stateMux); // only quick in-memory updates inside
      switch(id) {
        case CMD_ON:  currentState.power = true; ac.on(); break;
        case CMD_OFF: currentState.power = false; ac.off(); break;
        case CMD_TEMP_UP:   if(currentState.temp < 30) currentState.temp++; break;
        case CMD_TEMP_DOWN: if(currentState.temp > 16) currentState.temp--; break;
        
        case CMD_MODE_COOL: currentState.mode = 0; ac.setMode(kPanasonicAcCool); break;
        case CMD_MODE_DRY: currentState.mode = 1; ac.setMode(kPanasonicAcDry); break;
        case CMD_MODE_FAN: currentState.mode = 2; ac.setMode(kPanasonicAcFan); break;

        case CMD_F_AUTO: currentState.fan = 0; ac.setFan(kPanasonicAcFanAuto); break;
        case CMD_F_1:    currentState.fan = 1; ac.setFan(kPanasonicAcFanMin); break;
        case CMD_F_2:    currentState.fan = 2; ac.setFan(kPanasonicAcFanLow); break;
        case CMD_F_3:    currentState.fan = 3; ac.setFan(kPanasonicAcFanMed); break;
        case CMD_F_4:    currentState.fan = 4; ac.setFan(kPanasonicAcFanHigh); break;
        case CMD_F_5:    currentState.fan = 5; ac.setFan(kPanasonicAcFanMax); break;

        case CMD_S_AUTO: currentState.swing = 0; ac.setSwingVertical(kPanasonicAcSwingVAuto); break;
        case CMD_S_1:    currentState.swing = 1; ac.setSwingVertical(kPanasonicAcSwingVHighest); break;
        case CMD_S_2:    currentState.swing = 2; ac.setSwingVertical(kPanasonicAcSwingVHigh); break;
        case CMD_S_3:    currentState.swing = 3; ac.setSwingVertical(kPanasonicAcSwingVMiddle); break;
        case CMD_S_4:    currentState.swing = 4; ac.setSwingVertical(kPanasonicAcSwingVLow); break;
        case CMD_S_5:    currentState.swing = 5; ac.setSwingVertical(kPanasonicAcSwingVLowest); break;
      }
      snap = currentState;
      stateDirty = true;
      stateChangedAt = millis();
      taskEXIT_CRITICAL(&stateMux);

      // Fan mode uses a fixed temperature set by setMode(); don't override it
      if (snap.mode != 2) ac.setTemp(snap.temp);
      ac.send();
      Serial.printf("[Core 1] State: Pwr:%d, T:%d, F:%d, S:%d\n", snap.power, snap.temp, snap.fan, snap.swing);
    }
  }
}

void setup() {
  Serial.begin(115200);
  ac.begin();
  
  loadState();
  applyStateToLibrary();

  irQueue = xQueueCreate(10, sizeof(int));
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(HOSTNAME);

  // Hold BOOT (GPIO0) for 3 s at power-up to forget the saved WiFi
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
  Serial.println("Connected: " + WiFi.localIP().toString());

  MDNS.begin(HOSTNAME);

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { Serial.println("OTA start"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA done"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA error %u\n", e); });
  ArduinoOTA.begin();


  server.on("/", []() {
    if (server.hasArg("ID")) {
      String arg = server.arg("ID");
      int id = arg.toInt();
      if (!isValidCmd(arg, id)) {
        server.send(400, "text/plain", "Invalid ID");
      } else if (xQueueSend(irQueue, &id, 0) != pdTRUE) {
        server.send(503, "text/plain", "Busy");
      } else {
        server.send(200, "text/plain", "OK");
      }
    } else {
      server.send(200, "text/html", webpage);
    }
  });

  // NEW JSON STATUS ROUTE (Homebridge Ready)
  server.on("/getStatus", []() {
    taskENTER_CRITICAL(&stateMux);
    ACState snap = currentState;
    taskEXIT_CRITICAL(&stateMux);
    String json = "{";
    json += "\"power\":" + String(snap.power ? "true" : "false") + ",";
    json += "\"mode\":" + String(snap.mode) + ",";
    json += "\"temp\":" + String(snap.temp) + ",";
    json += "\"fan\":" + String(snap.fan) + ",";
    json += "\"swing\":" + String(snap.swing);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  // Higher priority than loop() (1) so IR bit-banging isn't preempted mid-frame
  xTaskCreatePinnedToCore(irTask, "IRTask", 6144, NULL, 3, NULL, 1);
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  // Save 5 s after the last change to limit flash wear
  if (stateDirty && millis() - stateChangedAt > 5000) saveState();
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, reconnecting");
      WiFi.reconnect();
    }
  }
  delay(1); 
}