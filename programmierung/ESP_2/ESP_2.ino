// ESP2.ino
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <math.h>

const char* WIFI_SSID = "ESP-HOST";
const char* WIFI_PASS = "espHostPass";

const bool ACTIVE_LOW = true;
struct RelayConfig { uint8_t pin; uint8_t relayNum; const char* title; };
const uint8_t RELAY_COUNT = 4;
RelayConfig RELAYS[RELAY_COUNT] = {
  {19, 1, "Relais 1 (Placeholder)"}, {21, 2, "Relais 2 (Placeholder)"},
  {22, 3, "Relais 3 (Placeholder)"}, {23, 4, "Relais 4 (Placeholder)"} 
};

const int adcPin = 34;
const float Vcc = 3.3;
const float Rf = 10000.0;
const float R0 = 10000.0;
const float T0_K = 298.15;
const float beta = 3950.0;

float cachedTempC = NAN;
float cachedRntc = NAN;
unsigned long lastTempMillis = 0;
const unsigned long TEMP_INTERVAL = 1000;

WebServer server(80);

int physToLogical(int physVal){ return ACTIVE_LOW ? (physVal==LOW?1:0) : (physVal==HIGH?1:0); }
int logicalToPhys(int logical){ return ACTIVE_LOW ? (logical==1?LOW:HIGH) : (logical==1?HIGH:LOW); }

float readVoltage(){ int adc=analogRead(adcPin); return (float)adc/4095.0*Vcc; }
float readR_NTC(){ float v = readVoltage(); if(v <= 0.0001) return 1e9; if(v >= Vcc-0.0001) return 1e-6; return Rf * (v / (Vcc - v)); }
float ntcToCelsius(float Rntc){ float invT = (1.0/T0_K) + (1.0/beta) * log(Rntc/R0); return 1.0/invT - 273.15; }

String getRelayStateJSON(){
  String s="{";
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    s += "\"r"+String(i)+"\":"+String(physToLogical(digitalRead(RELAYS[i].pin)));
    if(i<RELAY_COUNT-1) s+=","; 
  }
  s+="}";
  return s;
}

void handleIndex();
void handleState(){ server.send(200,"application/json",getRelayStateJSON()); }
void handleSet(){
  if(!server.hasArg("idx")||!server.hasArg("val")){server.send(400,"text/plain","missing args");return;}
  int idx=server.arg("idx").toInt();
  int val=server.arg("val").toInt();
  if(idx<0||idx>=RELAY_COUNT||(val!=0&&val!=1)){server.send(400,"text/plain","invalid args");return;}
  digitalWrite(RELAYS[idx].pin, logicalToPhys(val));
  server.send(200,"text/plain","ok");
}

void handleTemp(){
  String j = "{\"temp\":"+(isnan(cachedTempC)?"null":String(cachedTempC,2))+
             ",\"rntc\":"+(isnan(cachedRntc)?"null":String(cachedRntc,0))+"}";
  server.send(200,"application/json",j);
}

void startServerHandlers(){
  server.on("/",HTTP_GET,handleIndex);
  server.on("/state",HTTP_GET,handleState);
  server.on("/set",HTTP_GET,handleSet);
  server.on("/temp",HTTP_GET,handleTemp);
  server.begin();
}

const char index_html[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP2</title></head><body><h1>ESP2</h1></body></html>
)rawliteral";

void handleIndex() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "text/html", index_html);
}

void registerAtHost(){
  HTTPClient http;
  String regUrl = "http://192.168.4.1/register";
  http.begin(regUrl);
  http.addHeader("Content-Type","application/json");
  String payload = "{\"name\":\"esp2\",\"ip\":\""+WiFi.localIP().toString()+"\"}";
  http.POST(payload);
  http.end();
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){
  switch(event){
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WIFI EVENT] Verbunden. IP: "); Serial.println(WiFi.localIP());
      registerAtHost();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WIFI EVENT] getrennt. Versuche reconnect...");
      WiFi.reconnect();
      break;
    default: break;
  }
}

void setup(){
  Serial.begin(115200); delay(50);
  for(uint8_t i=0;i<RELAY_COUNT;i++){ pinMode(RELAYS[i].pin,OUTPUT); digitalWrite(RELAYS[i].pin,logicalToPhys(0)); }
  analogReadResolution(12); analogSetPinAttenuation(adcPin, ADC_11db); // some cores use analogSetAttenuation

  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID,WIFI_PASS);

  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<15000){ delay(200); Serial.print("."); } Serial.println();
  if(WiFi.status()==WL_CONNECTED){
    registerAtHost();
  }

  startServerHandlers();
}

void loop(){
  server.handleClient();
  unsigned long now = millis();
  if(now-lastTempMillis >= TEMP_INTERVAL){
    lastTempMillis = now;
    float Rntc = readR_NTC();
    float tempC = ntcToCelsius(Rntc);
    cachedTempC = tempC; cachedRntc = Rntc;
    registerAtHost();
  }
}
