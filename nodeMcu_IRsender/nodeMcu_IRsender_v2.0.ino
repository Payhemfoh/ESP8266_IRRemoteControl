#include <ESP8266WiFi.h>
#include <IRremoteESP8266.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <IRsend.h>
#include <string>

const char* ssid = ""; //Hotspot name
const char* password = "";
String ip = "http://192.168.0.55";
const int pin_send = D2;

IPAddress local_IP(192,168,0,55);
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);
IRsend irsend(pin_send);
const char webpage[] PROGMEM = R"(
<!DOCTYPE html>
<html lang='en'>
<head>
  <meta charset='utf-8'><title>Samsung Controller</title>
  <style>body{display:flex;justify-content:center;align-items:center;flex-direction:column;margin:auto;max-width:750px;}button{width:150px;height:50px;display:inline-block;padding:0.5em 1.7em;margin:0 0.1em 0.1em 0;border:0.16em solid rgba(255, 255, 255, 0);background-color:rgb(80, 10, 172);border-radius:2em;box-sizing:border-box;text-decoration:none;font-family:"Roboto", sans-serif;font-weight:700;font-size:14px;color:#ffffff;text-shadow:0 0.04em 0.04em rgba(0, 0, 0, 0.35);text-align:center;}div{display:flex;justify-content:space-between;width:100%; margin:10px 0;}</style>
</head>
<body>
  <h1>Samsung Controller</h1>
  <div><button onclick='ajaxCall("http://192.168.0.55?BTN=SOURCE")'>SOURCE</button><button onclick='ajaxCall("http://192.168.0.55?BTN=POWER")'>POWER</button></div>
  <div><button onclick='ajaxCall("http://192.168.0.55?BTN=VOLDOWN")'>- VOL</button><button onclick='ajaxCall("http://192.168.0.55?BTN=MUTE")'>MUTE</button><button onclick='ajaxCall("http://192.168.0.55?BTN=VOLUP")'>+ VOL</button></div>
  <div style="justify-content:center"><button onclick='ajaxCall("http://192.168.0.55?BTN=UP")'>UP</button></div>
  <div><button onclick='ajaxCall("http://192.168.0.55?BTN=LEFT")'>LEFT</button><button onclick='ajaxCall("http://192.168.0.55?BTN=SMART")'>SMART</button><button onclick='ajaxCall("http://192.168.0.55?BTN=RIGHT")'>RIGHT</button></div>
  <div style="justify-content:center"><button onclick='ajaxCall("http://192.168.0.55?BTN=DOWN")'>DOWN</button></div>
  <div><button onclick='ajaxCall("http://192.168.0.55?BTN=RETURN")'>RETURN</button><button onclick='ajaxCall("http://192.168.0.55?BTN=ENTER")'>ENTER</button></div>
  <script src='https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js'></script>
  <script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js'></script>
  <script> function ajaxCall(url){$.ajax({'url':url})}</script>
</body>
</html>
)";

void handleWebpage();
void handleNotFound();
void checkAction(String);

void setup(){
  irsend.begin();
  Serial.begin(115200);

  if(!WiFi.config(local_IP,gateway,subnet)){
    Serial.println("STA Failed to configure");
  }

  // setup and connect to WiFi
  WiFi.begin(ssid,password);

  Serial.println("Start to Connect to WLAN");
  
  //loop while not connected to WiFi
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  //print out success message
  Serial.printf("Success to connect to WLAN %s\n",ssid);
  Serial.printf("URL=http://%s/\n",WiFi.localIP().toString().c_str());
  
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS Responder Started");
  }
  
  //start the server at port 80
  server.on("/",handleWebpage);
  server.on("/control",handleWebpage);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Server started at port 80");
}

void loop() {
  //check if server is available
  server.handleClient();
  return;
}

void handleWebpage(){
  if(server.args() == 1){
    checkAction(server.arg(0));
    server.send(200,"text/plain","received");
  }
  else{
    server.send(200,"text/html",webpage);
  }
}

void handleNotFound(){
  String message;
  message.reserve(512);
  message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for(uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void checkAction(String request){
  if(request.indexOf(F("POWER")) != -1){
    irsend.sendSAMSUNG(0xE0E040BF);
  }
  else if(request.indexOf(F("SOURCE")) != -1){
    irsend.sendSAMSUNG(0xE0E0807F);
  }
  else if(request.indexOf(F("VOLUP")) != -1){
    irsend.sendSAMSUNG(0xE0E0E01F);
  }
  else if(request.indexOf(F("VOLDOWN")) != -1){
    irsend.sendSAMSUNG(0xE0E0D02F);
  }
  else if(request.indexOf(F("MUTE")) != -1){
    irsend.sendSAMSUNG(0xE0E0F00F);
  }
  else if(request.indexOf(F("SMART")) != -1){
    irsend.sendSAMSUNG(0xE0E09E61);
  }
  else if(request.indexOf(F("UP")) != -1){
    irsend.sendSAMSUNG(0xE0E006F9);
  }
  else if(request.indexOf(F("DOWN")) != -1){
    irsend.sendSAMSUNG(0xE0E08679);
  }
  else if(request.indexOf(F("LEFT")) != -1){
    irsend.sendSAMSUNG(0xE0E0A659);
  }
  else if(request.indexOf(F("RIGHT")) != -1){
    irsend.sendSAMSUNG(0xE0E046B9);
  }
  else if(request.indexOf(F("ENTER")) != -1){
    irsend.sendSAMSUNG(0xE0E016E9);
  }
  else if(request.indexOf(F("RETURN")) != -1){
    irsend.sendSAMSUNG(0xE0E01AE5);
  }
}