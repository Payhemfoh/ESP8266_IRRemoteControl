# ESP IR Remote Controller + Smart Autogate

A collection of ESP8266 / ESP32 projects: web-controlled IR remotes (smart TV, air-conditioner) and a web-controlled autogate panel.

## Repository layout

| Folder / file | Board | What it is |
|---|---|---|
| [`nodeMcu_IRreceiver/`](nodeMcu_IRreceiver) | nodeMCU ESP8266 | Reads the IR signal from a remote with an IR receiver diode (VS1838B) and prints the decoded codes to Serial. Use it to capture codes for the senders. |
| [`nodeMcu_IRsender_v1.0/`](nodeMcu_IRsender_v1.0) | nodeMCU ESP8266 | First IR sender: sends captured codes through an IR transmitter, with a basic web server. Wi-Fi settings are hard-coded. |
| [`nodeMcu_IRsender_v2.0/`](nodeMcu_IRsender_v2.0) | nodeMCU ESP8266 | IR sender with the web page embedded in the sketch. Wi-Fi settings are hard-coded. |
| [`nodeMcu_IRsender_v2.1/`](nodeMcu_IRsender_v2.1) | ESP32 | Air-conditioner (Panasonic) controller: web UI for power, mode, temperature, fan and swing. Saves state to flash (NVS), Wi-Fi setup portal (WiFiManager), OTA updates, mDNS. |
| [`nodeMcu_IRsender_v3.0/`](nodeMcu_IRsender_v3.0) | nodeMCU ESP8266 | Smart TV (Samsung) controller. Web UI served from LittleFS (`data/index.html`), WiFiManager setup portal, OTA updates, mDNS. |
| [`gate_controller/`](gate_controller) | ESP32-CAM (or ESP32 devkit) | Smart autogate: replaces the indoor Gmatic wall panel with a web page driving a 4-channel relay (Both Gates, Left/Pedestrian Gate, Light, System Lock). |
| [`webpage.html`](webpage.html) | n/a | Standalone copy of the v2.0 web page (buttons call the sender's IP). |
| [`wiring.drawio`](wiring.drawio) | n/a | Wiring diagram for the IR sender / receiver. |

## Secrets

Projects that need credentials (`nodeMcu_IRsender_v2.1`, `nodeMcu_IRsender_v3.0`, `gate_controller`) read their passwords from a `secrets.h` file that is **gitignored**. In each folder, copy `secrets.h.example` to `secrets.h` and fill it in. Never commit `secrets.h`.

---

## IR remote projects

Hardware:

1. nodeMCU ESP8266: https://my.cytron.io/p-nodemcu-lua-v3-esp8266-wifi-with-ch340c
2. IR transmitter: https://my.cytron.io/p-ir-tansmitter
3. NPN transistor (2N3904): https://my.cytron.io/p-transistor-2n3904
4. IR receiver diode VS1838B: https://my.cytron.io/p-ir-receiver-diode-vs1838b-38khz

Workflow: flash `nodeMcu_IRreceiver`, point your remote at the receiver and note the codes from the Serial Monitor, put the codes into a sender sketch, then flash the sender. The sender exposes a web page that devices on the same network can use to send the IR signals.

## gate_controller

Converts a Gmatic indoor autogate wall panel into a web-controlled system.

Hardware:

- ESP32-CAM (AI Thinker) or ESP32 devkit
- 5V 4-channel active-LOW optocoupler relay module
- LM2596 buck converter (Gmatic 12/24V down to 5.0V; set the output with a multimeter **before** connecting the ESP32)

Relay wiring (ESP32-CAM pins; use GPIO 18, 19, 21, 22 on a regular devkit):

| Web button | GPIO | Relay | Gmatic terminal |
|---|---|---|---|
| Both Gates | 13 | IN1 / NO1 | BOTH |
| Left Gate (Ped.) | 14 | IN2 / NO2 | LEFT |
| Light (I.R.) | 15 | IN3 / NO3 | LIGHT |
| System Lock | 16 | IN4 / NO4 | LOCK |

All four relay COM terminals are chained together and connected to the Gmatic GND/COM. Each button press pulses its relay for 500 ms, like a momentary panel button. The web UI is protected by HTTP basic auth.

Setup:

1. Copy `secrets.h.example` to `secrets.h` and set the setup-portal password, OTA password and web login (no Wi-Fi details go in the code).
2. Install the `WiFiManager` library (tzapu), select board "AI Thinker ESP32-CAM" with PSRAM disabled, and upload.
3. On first boot (or after a Wi-Fi reset) the board starts a hotspot named `Gate-Setup`. Join it from your phone, choose your home Wi-Fi in the portal and enter its password.
4. Give the board a fixed address with a DHCP reservation on your router (or a Pi-hole local DNS record). Reach it at `http://gate.local` or its IP.
5. To forget the saved Wi-Fi, hold the BOOT/FLASH button (GPIO0) for 3 s at power-up. Later updates can be sent over Wi-Fi (OTA) using the OTA password.

Always check the Gmatic terminals with a multimeter before connecting the relay contacts.
