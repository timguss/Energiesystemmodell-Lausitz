// ESP1.ino
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <math.h>

const char* WIFI_SSID = "ESP-HOST";
const char* WIFI_PASS = "espHostPass";

const int UART2_RX_PIN = 16;
const int UART2_TX_PIN = 17;
const unsigned long DEFAULT_REPLY_TIMEOUT = 500;
HardwareSerial MySerial(2);
bool serial2Started = false;

const bool ACTIVE_LOW = true;
struct RelayConfig { uint8_t pin; uint8_t relayNum; const char* title; };
const uint8_t RELAY_COUNT = 8;
RelayConfig RELAYS[RELAY_COUNT] = {
  {19, 1, "Ventil - 1"}, {21, 2, "Ventil - 2"}, {22, 3, "Heizstab"}, {23, 4, "Zünder"},
  {32, 5, "Gasventil"}, {33, 6, "Kühler"}, {25, 7, "MFC - fehlt noch"}, {26, 8, "unbelegt"}
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

bool rs232Enabled = true;

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

void handleSend(){
  if(!rs232Enabled){ server.send(400,"text/plain","RS232 deaktiviert"); return;}
  if(!serial2Started){ server.send(500,"text/plain","Serial2 nicht initialisiert."); return;}
  if(!server.hasArg("plain")){ server.send(400,"text/plain","Kein Body"); return;}
  
  String body = server.arg("plain");
  int idx1 = body.indexOf("\"cmd\"");
  int idx2 = body.indexOf("\"timeout\"");
  String cmd = "";
  unsigned long timeout = DEFAULT_REPLY_TIMEOUT;
  
  if(idx1 >=0){
    int q1 = body.indexOf("\"", idx1+5);
    int q2 = body.indexOf("\"", q1+1);
    if(q1>=0 && q2>q1) cmd = body.substring(q1+1,q2);
  }
  if(idx2 >=0){
    int colon = body.indexOf(":",idx2);
    int comma = body.indexOf("}",colon);
    if(colon>=0 && comma>colon){
      String tstr = body.substring(colon+1,comma);
      tstr.trim();
      timeout = tstr.toInt();
    }
  }
  
  if(cmd.length()==0){ server.send(400,"text/plain","Kein cmd gefunden"); return; }
  if(!cmd.endsWith("\r\n")) cmd += "\r\n";
  
  MySerial.print(cmd);
  String resp = "";
  unsigned long start = millis();
  while(millis()-start < timeout){
    while(MySerial.available()) resp += (char)MySerial.read();
  }
  
  server.send(200,"text/plain",resp.length()?resp:"(keine Antwort)");
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){
  switch(event){
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WIFI EVENT] Verbunden. IP: "); Serial.println(WiFi.localIP());
      if(!serial2Started){
        MySerial.begin(38400,SERIAL_8N1,UART2_RX_PIN,UART2_TX_PIN);
        serial2Started=true;
        Serial.println("Serial2 gestartet via Event.");
      }
      {
        HTTPClient http;
        String regUrl = "http://192.168.4.1/register";
        http.begin(regUrl);
        http.addHeader("Content-Type","application/json");
        String payload = "{\"name\":\"esp1\",\"ip\":\""+WiFi.localIP().toString()+"\"}";
        http.POST(payload);
        http.end();
      }
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WIFI EVENT] getrennt. Versuche reconnect...");
      WiFi.reconnect();
      break;
    default: break;
  }
}

void startServerHandlers(){
  server.on("/",HTTP_GET,handleIndex);
  server.on("/state",HTTP_GET,handleState);
  server.on("/set",HTTP_GET,handleSet);
  server.on("/temp",HTTP_GET,handleTemp);
  server.on("/send",HTTP_POST,handleSend);
  server.begin();
}

const char index_html[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP1</title></head><body><h1>ESP1</h1></body></html>
)rawliteral";

void handleIndex() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "text/html", index_html);
}

void setup(){
  Serial.begin(115200); delay(50);
  for(uint8_t i=0;i<RELAY_COUNT;i++){ pinMode(RELAYS[i].pin,OUTPUT); digitalWrite(RELAYS[i].pin,logicalToPhys(0)); }
  analogReadResolution(12); analogSetAttenuation(ADC_11db);

  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID,WIFI_PASS);

  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<15000){ delay(200); Serial.print("."); } Serial.println();
  if(WiFi.status()==WL_CONNECTED){
    MySerial.begin(38400,SERIAL_8N1,UART2_RX_PIN,UART2_TX_PIN); serial2Started=true;
    HTTPClient http; http.begin("http://192.168.4.1/register"); http.addHeader("Content-Type","application/json");
    String payload = "{\"name\":\"esp1\",\"ip\":\""+WiFi.localIP().toString()+"\"}";
    http.POST(payload); http.end();
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
    HTTPClient http; http.begin("http://192.168.4.1/register"); http.addHeader("Content-Type","application/json");
    String payload = "{\"name\":\"esp1\",\"ip\":\""+WiFi.localIP().toString()+"\"}";
    http.POST(payload); http.end();
  }
}
