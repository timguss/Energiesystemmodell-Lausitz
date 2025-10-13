// esp2_client.ino
// Kohle-System (4 Relays, Temperatur)
// connect to AP, post status, accept /cmd

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* AP_SSID = "ESP-AP";
const char* AP_PASS = "espap123";
const char* MY_ID = "esp2";
const char* AP_ADDR = "192.168.4.1";

WebServer server(80);

struct RelayConfig { uint8_t pin; const char* title; };
RelayConfig RELAYS[4] = {{32,"Ventil"},{33,"Heizstab"},{25,"Kühler"},{26,"Reserve"}};
bool relayState[4] = {0,0,0,0};

const int adcPin = 34;
const float Vcc=3.3, Rf=10000.0, R0=10000.0, T0_K=298.15, beta=3950.0;
unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 1000;

void setup_wifi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(AP_SSID, AP_PASS);
  Serial.print("Verbinde mit AP...");
  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<15000){
    delay(300); Serial.print(".");
  }
  if(WiFi.status()!=WL_CONNECTED) Serial.println("\nWLAN fehlgeschlagen");
  else Serial.println("\nVerbunden: " + WiFi.localIP().toString());
}

float readVoltage(){ return analogRead(adcPin)/4095.0 * Vcc; }
float readR_NTC(){ float v = readVoltage(); if(v<=0.0001) return 1e9; if(v>=Vcc-0.0001) return 1e-6; return Rf*(v/(Vcc-v)); }
float ntcToCelsius(float R){ float invT=(1.0/T0_K)+(1.0/beta)*log(R/R0); return 1.0/invT - 273.15; }

void sendStatus(){
  if(WiFi.status()!=WL_CONNECTED) return;
  DynamicJsonDocument doc(512);
  doc["id"] = MY_ID;
  doc["ip"] = WiFi.localIP().toString();
  JsonObject s = doc.createNestedObject("status");
  float Rntc = readR_NTC();
  float temp = ntcToCelsius(Rntc);
  s["temp"] = temp;
  JsonArray arr = s.createNestedArray("relays");
  for(int i=0;i<4;i++) arr.add(relayState[i]?1:0);
  String body; serializeJson(doc, body);

  HTTPClient http;
  String url = String("http://") + AP_ADDR + "/register";
  http.begin(url);
  http.addHeader("Content-Type","application/json");
  int code = http.POST(body);
  http.end();
}

void applyCmd(String cmd){
  int colon = cmd.indexOf(':');
  if(colon>0){
    int idx = cmd.substring(0,colon).toInt();
    int val = cmd.substring(colon+1).toInt();
    if(idx>=0 && idx<4){
      relayState[idx] = (val==1);
      digitalWrite(RELAYS[idx].pin, relayState[idx]?LOW:HIGH);
      Serial.printf("Relay %d -> %d\n", idx, val);
    }
  } else if(cmd.startsWith("pwm:")){
    // not used here
  }
}

void handleCmd(){
  if(server.hasArg("plain")==false){ server.send(400,"text/plain","no body"); return; }
  String body = server.arg("plain");
  applyCmd(body);
  server.send(200,"text/plain","ok");
}

void setup(){
  Serial.begin(115200);
  Serial.println("=== ESP2 Kohle Client ===");
  for(int i=0;i<4;i++){ pinMode(RELAYS[i].pin, OUTPUT); digitalWrite(RELAYS[i].pin, HIGH); }
  analogReadResolution(12); analogSetAttenuation(ADC_11db);
  setup_wifi();
  server.on("/cmd", HTTP_POST, handleCmd);
  server.begin();
}

void loop(){
  server.handleClient();
  if(millis() - lastPublish >= PUBLISH_INTERVAL){
    lastPublish = millis();
    sendStatus();
  }
}
