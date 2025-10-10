/* ESP2 -- 4 Relais (32,33,25,26) + NTC on ADC34, MQTT device id "esp2" */
#include <WiFi.h>
#include <PubSubClient.h>
#include <Credentials.h>

const char* DEVICE_ID = "esp2";

///// UART2 (falls RS232) /////
const int UART2_RX_PIN = 16;
const int UART2_TX_PIN = 17;
const unsigned long DEFAULT_REPLY_TIMEOUT = 500;
HardwareSerial MySerial(2);
bool serial2Started = false;

///// Relais /////
const bool ACTIVE_LOW = true;
struct RelayConfig { uint8_t pin; uint8_t relayNum; const char* title; };
const uint8_t RELAY_COUNT = 4;
RelayConfig RELAYS[RELAY_COUNT] = {
  {32, 1, "R1"}, {33, 2, "R2"}, {25, 3, "R3"}, {26, 4, "R4"}
};

///// NTC /////
const int adcPin = 34;
const float Vcc = 3.3;
const float Rf = 10000.0;
const float R0 = 10000.0;
const float T0_K = 298.15;
const float beta = 3950.0;

float cachedTempC = NAN;
float cachedRntc = NAN;
unsigned long lastTempMillis = 0;
const unsigned long TEMP_INTERVAL = 2000;

WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastStatePublish = 0;
const unsigned long STATE_PUBLISH_INTERVAL = 5000;

int physToLogical(int physVal){ return ACTIVE_LOW ? (physVal==LOW?1:0) : (physVal==HIGH?1:0);}
int logicalToPhys(int logical){ return ACTIVE_LOW ? (logical==1?LOW:HIGH) : (logical==1?HIGH:LOW);}

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

void publishRelayState(bool retained=false){
  String j = getRelayStateJSON();
  String topic = String("esp/") + DEVICE_ID + "/relays/state";
  mqtt.publish(topic.c_str(), j.c_str(), retained);
}
void publishTemp(){
  String j = "{\"temp\":"+(isnan(cachedTempC)?"null":String(cachedTempC,2))+
             ",\"rntc\":"+(isnan(cachedRntc)?"null":String(cachedRntc,0))+"}";
  String topic = String("esp/") + DEVICE_ID + "/temp";
  mqtt.publish(topic.c_str(), j.c_str());
}

String serialReadWithTimeout(unsigned long timeout){
  String resp="";
  unsigned long start = millis();
  while(millis()-start < timeout){
    while(MySerial.available()){
      resp += (char)MySerial.read();
    }
  }
  return resp;
}

void handleRS232Send(String payload){
  int sep = payload.indexOf('|');
  String cmd; unsigned long timeout = DEFAULT_REPLY_TIMEOUT;
  if(sep>0){ cmd = payload.substring(0,sep); String tstr = payload.substring(sep+1); tstr.trim(); unsigned long t = tstr.toInt(); if(t>0) timeout = t; }
  else cmd = payload;
  cmd.trim();
  if(cmd.length()==0) return;
  if(!cmd.endsWith("\r\n")) cmd += "\r\n";
  if(!serial2Started){ mqtt.publish((String("esp/")+DEVICE_ID+"/rs232/reply").c_str(), "Serial2 not started"); return; }
  MySerial.print(cmd);
  String resp = serialReadWithTimeout(timeout);
  if(resp.length()==0) resp="(keine Antwort)";
  mqtt.publish((String("esp/")+DEVICE_ID+"/rs232/reply").c_str(), resp.c_str());
}

void handleRelaySetPayload(String payload){
  payload.trim();
  int colon = payload.indexOf(':');
  if(colon>0){
    String left = payload.substring(0,colon);
    String right = payload.substring(colon+1);
    left.trim(); right.trim();
    // support r0:1 or index(1..N):val
    if(left.length()>1 && left.startsWith("r")){
      // r0 style
      for(uint8_t i=0;i<RELAY_COUNT;i++){
        if(String("r"+String(i))==left){
          int val = (right=="1"|| right.equalsIgnoreCase("on"))?1:0;
          digitalWrite(RELAYS[i].pin, logicalToPhys(val));
          publishRelayState(true);
          return;
        }
      }
    } else {
      int idx = left.toInt();
      if(idx>0 && idx<=RELAY_COUNT){
        int val = (right=="1"|| right.equalsIgnoreCase("on"))?1:0;
        digitalWrite(RELAYS[idx-1].pin, logicalToPhys(val));
        publishRelayState(true);
        return;
      }
    }
  }
  // name based not implemented here
}

void mqttCallback(char* topic, byte* payload, unsigned int length){
  String msg = String((char*)payload, length);
  msg.trim();
  String t = String(topic);
  Serial.print("[MQTT] "); Serial.print(t); Serial.print(" -> "); Serial.println(msg);
  String base = String("rpi/") + DEVICE_ID + "/";
  if(t == base + "relay/set"){
    handleRelaySetPayload(msg);
  } else if(t == base + "rs232/send"){
    handleRS232Send(msg);
  } else if(t == base + "request/temp" || t == base + "request_temp"){
    publishTemp();
  } else if(t == base + "request/state"){
    publishRelayState(true);
  }
}

void connectToMQTT(){
  while(!mqtt.connected()){
    if(WiFi.status() != WL_CONNECTED){
      WiFi.reconnect(); delay(500); continue;
    }
    Serial.print("[MQTT] Trying to connect...");
    if(mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS)){
      Serial.println("connected");
      mqtt.subscribe((String("rpi/")+DEVICE_ID+"/relay/set").c_str());
      mqtt.subscribe((String("rpi/")+DEVICE_ID+"/rs232/send").c_str());
      mqtt.subscribe((String("rpi/")+DEVICE_ID+"/request/temp").c_str());
      mqtt.subscribe((String("rpi/")+DEVICE_ID+"/request/state").c_str());
      publishRelayState(true);
    } else {
      Serial.print("failed rc="); Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

void setup(){
  Serial.begin(115200); delay(50);
  Serial.println("\n=== ESP2 MQTT ===");
  for(uint8_t i=0;i<RELAY_COUNT;i++){ pinMode(RELAYS[i].pin,OUTPUT); digitalWrite(RELAYS[i].pin,logicalToPhys(0)); }
  analogReadResolution(12); analogSetAttenuation(ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Verbinde WLAN...");
  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<15000){ delay(500); Serial.print("."); } Serial.println();
  if(WiFi.status()==WL_CONNECTED) Serial.print("WLAN IP: "), Serial.println(WiFi.localIP());
  else Serial.println("WARN: WLAN nicht verbunden.");

  MySerial.begin(38400,SERIAL_8N1,UART2_RX_PIN,UART2_TX_PIN);
  serial2Started = true;

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}

void loop(){
  if(!mqtt.connected()) connectToMQTT();
  mqtt.loop();
  unsigned long now = millis();
  if(now - lastTempMillis >= TEMP_INTERVAL){
    lastTempMillis = now;
    float Rntc = readR_NTC();
    float tempC = ntcToCelsius(Rntc);
    cachedTempC = tempC; cachedRntc = Rntc;
    publishTemp();
    Serial.print("T: "); Serial.print(tempC); Serial.println(" °C");
  }
  if(now - lastStatePublish >= STATE_PUBLISH_INTERVAL){
    lastStatePublish = now;
    publishRelayState(false);
  }
}
