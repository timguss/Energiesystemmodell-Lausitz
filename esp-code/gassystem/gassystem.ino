/* ESP1 — MQTT-fähig, publisht Relay-Namen (meta), Temp, Relay-State, RS232
   Device ID: "esp1"
   Relais: 8 Stück mit names (Ventil - 1, Ventil - 2, Heizstab, Zünder, Gasventil, Kühler, MFC - fehlt noch, unbelegt)
   NTC: adcPin = 34
   RS232: UART2 RX=16, TX=17
   Topics:
     Publishes:
       esp/esp1/meta               -> {"relays":["Ventil - 1",...]}
       esp/esp1/temp               -> {"temp":xx,"rntc":yyyy}
       esp/esp1/relays/state       -> {"r0":0,"r1":1,...} (retained initial)
       esp/esp1/rs232/reply        -> text replies
     Subscribes:
       rpi/esp1/relay/set          -> payload "3:1" or "r2:1" or "Ventil - 1:on" (name-based)
       rpi/esp1/rs232/send         -> payload "CMD|timeout"
       rpi/esp1/request/temp       -> triggers immediate temp publish
       rpi/esp1/request/state      -> triggers state publish (retained)
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include <Credentials.h>

const char* DEVICE_ID = "esp1";

///// UART2 (ProPar) /////
const int UART2_RX_PIN = 16; // ESP RX  <- Modul TX
const int UART2_TX_PIN = 17; // ESP TX  -> Modul RX
const unsigned long DEFAULT_REPLY_TIMEOUT = 500; // ms
HardwareSerial MySerial(2); // UART2
bool serial2Started = false;

///// Relais /////
const bool ACTIVE_LOW = true;   // LOW = an, HIGH = aus
struct RelayConfig { uint8_t pin; uint8_t relayNum; const char* title; };
const uint8_t RELAY_COUNT = 8;
RelayConfig RELAYS[RELAY_COUNT] = {
  {19, 1, "Ventil - 1"},
  {21, 2, "Ventil - 2"},
  {22, 3, "Heizstab"},
  {23, 4, "Zünder"},
  {32, 5, "Gasventil"},
  {33, 6, "Kühler"},
  {25, 7, "MFC - fehlt noch"},
  {26, 8, "unbelegt"}
};

///// NTC /////
const int adcPin = 34;
const float Vcc = 3.3;
const float Rf = 10000.0;
const float R0 = 10000.0;
const float T0_K = 298.15;  // 25°C in Kelvin
const float beta = 3950.0;

float cachedTempC = NAN;
float cachedRntc = NAN;
unsigned long lastTempMillis = 0;
const unsigned long TEMP_INTERVAL = 2000;

bool rs232Enabled = true;

WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastStatePublish = 0;
const unsigned long STATE_PUBLISH_INTERVAL = 5000; // periodisches Senden

// --- Hilfsfunktionen ---
int physToLogical(int physVal){ return ACTIVE_LOW ? (physVal==LOW?1:0) : (physVal==HIGH?1:0);}
int logicalToPhys(int logical){ return ACTIVE_LOW ? (logical==1?LOW:HIGH) : (logical==1?HIGH:LOW);}

float readVoltage(){ int adc=analogRead(adcPin); return (float)adc/4095.0*Vcc; }
float readR_NTC(){ 
  float v = readVoltage(); 
  if(v <= 0.0001) return 1e9; 
  if(v >= Vcc-0.0001) return 1e-6; 
  return Rf * (v / (Vcc - v)); 
}
float ntcToCelsius(float Rntc){ 
  float invT = (1.0/T0_K) + (1.0/beta) * log(Rntc/R0); 
  return 1.0/invT - 273.15; 
}

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

void publishMeta() {
  // publish relay names so UI can display human-readable names
  String j = "{\"relays\":[";
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    j += "\"" + String(RELAYS[i].title) + "\"";
    if(i<RELAY_COUNT-1) j += ",";
  }
  j += "]}";
  String topic = String("esp/") + DEVICE_ID + "/meta";
  mqtt.publish(topic.c_str(), j.c_str(), true); // retained meta
}

// --- Serial read with timeout ---
String serialReadWithTimeout(unsigned long timeout){
  String resp="";
  unsigned long start = millis();
  while(millis()-start < timeout){
    while(MySerial.available()){
      resp += (char)MySerial.read();
    }
    // small yield to let other tasks run
    delay(1);
  }
  return resp;
}

// --- RS232 handler ---
void handleRS232Send(String payload){
  int sep = payload.indexOf('|');
  String cmd;
  unsigned long timeout = DEFAULT_REPLY_TIMEOUT;
  if(sep>0){
    cmd = payload.substring(0,sep);
    String tstr = payload.substring(sep+1);
    tstr.trim();
    unsigned long t = tstr.toInt();
    if(t>0) timeout = t;
  } else cmd = payload;

  cmd.trim();
  if(cmd.length()==0) return;
  if(!cmd.endsWith("\r\n")) cmd += "\r\n";

  if(!serial2Started){
    mqtt.publish((String("esp/")+DEVICE_ID+"/rs232/reply").c_str(), "Serial2 not started");
    return;
  }

  MySerial.print(cmd);
  String resp = serialReadWithTimeout(timeout);
  if(resp.length()==0) resp="(keine Antwort)";
  mqtt.publish((String("esp/")+DEVICE_ID+"/rs232/reply").c_str(), resp.c_str());
}

// --- Relay set payloads: supports "idx:val" (1..N), "r0:1", or "Name:on" ---
void handleRelaySetPayload(String payload){
  payload.trim();
  int colon = payload.indexOf(':');
  if(colon>0){
    String left = payload.substring(0,colon);
    String right = payload.substring(colon+1);
    left.trim(); right.trim();

    // Case "r0" style
    if(left.length()>1 && left.charAt(0)=='r'){
      int idx = left.substring(1).toInt();
      if(idx >= 0 && idx < RELAY_COUNT){
        int val = (right=="1" || right.equalsIgnoreCase("on")) ? 1 : 0;
        digitalWrite(RELAYS[idx].pin, logicalToPhys(val));
        publishRelayState(true);
        return;
      }
    }

    // Case index-based 1..N
    int idxNum = left.toInt();
    if(idxNum > 0 && idxNum <= RELAY_COUNT){
      int val = (right=="1" || right.equalsIgnoreCase("on")) ? 1 : 0;
      digitalWrite(RELAYS[idxNum-1].pin, logicalToPhys(val));
      publishRelayState(true);
      return;
    }

    // Case name-based (match beginning or full)
    for(uint8_t i=0;i<RELAY_COUNT;i++){
      String title = String(RELAYS[i].title);
      if(title.equalsIgnoreCase(left) || title.indexOf(left) >= 0){
        int val = (right=="1" || right.equalsIgnoreCase("on")) ? 1 : 0;
        digitalWrite(RELAYS[i].pin, logicalToPhys(val));
        publishRelayState(true);
        return;
      }
    }
  }
  // if not recognized, ignore
}

// --- MQTT callback ---
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

// --- MQTT connect / (re)subscribe ---
void connectToMQTT(){
  while(!mqtt.connected()){
    if(WiFi.status() != WL_CONNECTED){
      WiFi.reconnect();
      delay(500);
      continue;
    }
    Serial.print("[MQTT] Trying to connect...");
    if(mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS)){
      Serial.println("connected");
      // subscribe topics for this device
      String t1 = String("rpi/") + DEVICE_ID + "/relay/set";
      String t2 = String("rpi/") + DEVICE_ID + "/rs232/send";
      String t3 = String("rpi/") + DEVICE_ID + "/request/temp";
      String t4 = String("rpi/") + DEVICE_ID + "/request/state";
      mqtt.subscribe(t1.c_str());
      mqtt.subscribe(t2.c_str());
      mqtt.subscribe(t3.c_str());
      mqtt.subscribe(t4.c_str());
      // publish retained meta + initial state
      publishMeta();
      publishRelayState(true);
    } else {
      Serial.print("failed rc="); Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// --- Setup & Loop ---
void setup(){
  Serial.begin(115200); delay(50);
  Serial.println("\n=== ESP1 MQTT (with meta) ===");

  for(uint8_t i=0;i<RELAY_COUNT;i++){
    pinMode(RELAYS[i].pin, OUTPUT);
    digitalWrite(RELAYS[i].pin, logicalToPhys(0)); // default off
  }

  analogReadResolution(12);
  analogSetPinAttenuation(adcPin, ADC_11db);
  // fallback if analogSetPinAttenuation not available, keep analogSetAttenuation(ADC_11db)
  // analogSetAttenuation(ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Verbinde WLAN...");
  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<15000){
    delay(500); Serial.print(".");
  }
  Serial.println();
  if(WiFi.status()==WL_CONNECTED){
    Serial.print("WLAN verbunden. IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("WARN: WLAN nicht verbunden.");
  }

  MySerial.begin(38400, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
  serial2Started = true;
  Serial.println("Serial2 gestartet.");

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  // initial publish will happen on connect
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
