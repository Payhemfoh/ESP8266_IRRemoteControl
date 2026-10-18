#include <ESP8266WiFi.h>
#include <IRremoteESP8266.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <IRsend.h>
#include <cstring>
#include <string>

const char* ssid = ""; //Hotspot name
const char* password = "";
String ip = "http://192.168.1.55";
const int pin_send = D2;

IPAddress local_IP(192,168,1,55);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);
IRsend irsend(pin_send);

void handleWebpage();
void handleNotFound();
void checkAction(String);
String addBtn(String,String);

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
    String website = "<!DOCTYPE html>\n";
    website += "<html lang='en'>\n";
    website += "<head>\n";
    website += "<meta charset='utf-8'>\n";
    website += "<title>Samsung Controller</title>\n";
    website += "<style>button{ margin:5px; width: 150px; height: 50px;}</style>";
    website += "</head>\n";
    website += "<body style=\"display:flex;justify-content:center;align-items:center;flex-direction:column;\">\n";
    website += "<h1>Samsung Controller</h1>";
    website += "<div style='display:flex;justify-content:space-evenly;width:100%;'>";
    website += addBtn("BTN=SOURCE","SOURCE");
    website += addBtn("BTN=POWER","POWER");
    website += "</div>";
    website += "<div style='display:flex;justify-content:space-evenly;width:100%;'>";
    website += addBtn("BTN=VOLDOWN","- VOL");
    website += addBtn("BTN=MUTE","MUTE");
    website += addBtn("BTN=VOLUP","+ VOL");
    website += "</div>";
    website += "<div>";
    website += addBtn("BTN=UP","UP");
    website += "</div>";
    website += "<div style='display:flex;justify-content:space-evenly;width:100%;'>";
    website += addBtn("BTN=LEFT","LEFT");
    website += addBtn("BTN=SMART","SMART");
    website += addBtn("BTN=RIGHT","RIGHT");
    website += "</div>";
    website += "<div style='display:flex;justify-content:space-evenly;width:100%;'>";
    website += addBtn("BTN=RETURN","RETURN");
    website += addBtn("BTN=DOWN","DOWN");
    website += addBtn("BTN=ENTER","ENTER");
    website += "</div>";
    //ajax
    website += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js\"></script>\n";
    website += "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\"></script>\n";
    website += "<script> function ajaxCall(url){$.ajax({\"url\":url})}</script>\n";
    website += "</body>\n";
    website += "</html>";
  
    server.send(200,"text/html",website);
  }
}

String addBtn(String link,String text){
  String btn = "<button onclick='ajaxCall(\"";
  btn += ip;
  btn += "?";
  btn += link;
  btn += "\")'>";
  btn += text;
  btn += "</button><br>\n";

  return btn;
}

void handleNotFound(){
  String message = "File Not Found\n\n";
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
  if(request.indexOf("POWER") != -1){
    irsend.sendSAMSUNG(0xE0E040BF);
  }
  else if(request.indexOf("SOURCE") != -1){
    irsend.sendSAMSUNG(0xE0E0807F);
  }
  else if(request.indexOf("VOLUP") != -1){
    irsend.sendSAMSUNG(0xE0E0E01F);
  }
  else if(request.indexOf("VOLDOWN") != -1){
    irsend.sendSAMSUNG(0xE0E0D02F);
  }
  else if(request.indexOf("MUTE") != -1){
    irsend.sendSAMSUNG(0xE0E0F00F);
  }
  else if(request.indexOf("SMART") != -1){
    irsend.sendSAMSUNG(0xE0E09E61);
  }
  else if(request.indexOf("UP") != -1){
    irsend.sendSAMSUNG(0xE0E006F9);
  }
  else if(request.indexOf("DOWN") != -1){
    irsend.sendSAMSUNG(0xE0E08679);
  }
  else if(request.indexOf("LEFT") != -1){
    irsend.sendSAMSUNG(0xE0E0A659);
  }
  else if(request.indexOf("RIGHT") != -1){
    irsend.sendSAMSUNG(0xE0E046B9);
  }
  else if(request.indexOf("ENTER") != -1){
    irsend.sendSAMSUNG(0xE0E016E9);
  }
  else if(request.indexOf("RETURN") != -1){
    irsend.sendSAMSUNG(0xE0E01AE5);
  }
}
