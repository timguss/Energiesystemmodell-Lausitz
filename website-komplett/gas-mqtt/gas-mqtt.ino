 /* ESP32 MQTT-version
   - Relais / NTC / RS232 wie in deinem Originalcode
   - kommuniziert über MQTT mit Raspberry Pi
*/

#include <WiFi.h>
#include <PubSubClient.h>

///// Benutzerkonfiguration /////
const char* WIFI_SSID = "Energiesystemmodell-RPi-AP";       // passe an, falls geändert
const char* WIFI_PASS = "Strukturwandel-RPiWiFi2025"; // passe an, falls geändert
const char* MQTT_SERVER = "192.168.4.1";   // Raspberry Pi MQTT-Broker
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "modelltisch1";
const char* MQTT_PASS = "strukturwandel";

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
  {19, 1, "Ventil - 1"}, {21, 2, "Ventil - 2"}, {22, 3, "Heizstab"}, {23, 4, "Zünder"},
  {32, 5, "Gasventil"}, {33, 6, "Kühler"}, {25, 7, "MFC - fehlt noch"}, {26, 8, "unbelegt"}
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
const unsigned long STATE_PUBLISH_INTERVAL = 5000; // alle 5s den Status senden

// Hilfsfunktionen
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
  mqtt.publish("esp32/relays/state", j.c_str(), retained);
}

void publishTemp(){
  String j = "{\"temp\":"+(isnan(cachedTempC)?"null":String(cachedTempC,2))+
             ",\"rntc\":"+(isnan(cachedRntc)?"null":String(cachedRntc,0))+"}";
  mqtt.publish("esp32/temp", j.c_str());
}

void handleRelaySetPayload(String payload){
  payload.trim();
  // Formate: "idx:val" (z.B. "3:1") oder "name:on" oder "Ventil - 1:on"
  int colon = payload.indexOf(':');
  if(colon>0){
    String left = payload.substring(0,colon);
    String right = payload.substring(colon+1);
    left.trim(); right.trim();
    int idx = left.toInt();
    if(idx>0 && idx<=RELAY_COUNT){
      int val = right=="1" || right.equalsIgnoreCase("on") ? 1 : 0;
      digitalWrite(RELAYS[idx-1].pin, logicalToPhys(val));
      publishRelayState(true);
      return;
    }
  }
  // sonst nach Namen suchen (z.B. "Ventil - 1:on")
  int sep = payload.lastIndexOf(':');
  if(sep>0){
    String name = payload.substring(0,sep);
    String valtxt = payload.substring(sep+1);
    name.trim(); valtxt.trim();
    int val = valtxt=="1" || valtxt.equalsIgnoreCase("on") ? 1 : 0;
    for(uint8_t i=0;i<RELAY_COUNT;i++){
      if(String(RELAYS[i].title).equalsIgnoreCase(name) || String(RELAYS[i].title).indexOf(name) >= 0){
        digitalWrite(RELAYS[i].pin, logicalToPhys(val));
        publishRelayState(true);
        return;
      }
    }
  }
  // falls nichts erkannt -> ignore
}

String serialReadWithTimeout(unsigned long timeout){
  String resp="";
  unsigned long start = millis();
  while(millis()-start < timeout){
    while(MySerial.available()){
      resp += (char)MySerial.read();
      // optional: break early on \n?
    }
  }
  return resp;
}

void handleRS232Send(String payload){
  // erwartet: CMD|timeout (timeout optional)
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
    mqtt.publish("esp32/rs232/reply","Serial2 not started");
    return;
  }
  MySerial.print(cmd);
  String resp = serialReadWithTimeout(timeout);
  if(resp.length()==0) resp="(keine Antwort)";
  mqtt.publish("esp32/rs232/reply", resp.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length){
  String msg = String((char*)payload, length);
  msg.trim();
  String t = String(topic);
  Serial.print("[MQTT] "); Serial.print(t); Serial.print(" -> "); Serial.println(msg);
  if(t == String("rpi/relay/set")){
    handleRelaySetPayload(msg);
  } else if(t == String("rpi/rs232/send")){
    handleRS232Send(msg);
  } else if(t == String("rpi/request/temp")){
    publishTemp();
  } else if(t == String("rpi/request/state")){
    publishRelayState(true);
  }
}

void connectToMQTT(){
  while(!mqtt.connected()){
    if(WiFi.status() != WL_CONNECTED){
      // reconnect wifi
      WiFi.reconnect();
      delay(500);
      continue;
    }
    Serial.print("[MQTT] Trying to connect...");
    if(mqtt.connect("ESP32_Client_1", MQTT_USER, MQTT_PASS)){
      Serial.println("connected");
      // subscribe topics
      mqtt.subscribe("rpi/relay/set");
      mqtt.subscribe("rpi/rs232/send");
      mqtt.subscribe("rpi/request/temp");
      mqtt.subscribe("rpi/request/state");
      // publish retained initial state
      publishRelayState(true);
    } else {
      Serial.print("failed rc="); Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

void setup(){
  Serial.begin(115200); delay(50);
  Serial.println("\n=== ESP32 Kombi MQTT ===");

  for(uint8_t i=0;i<RELAY_COUNT;i++){ pinMode(RELAYS[i].pin,OUTPUT); digitalWrite(RELAYS[i].pin,logicalToPhys(0)); }

  analogReadResolution(12); analogSetAttenuation(ADC_11db);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Verbinde WLAN...");
  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<15000){ delay(500); Serial.print("."); } Serial.println();
  if(WiFi.status()==WL_CONNECTED){
    Serial.print("WLAN verbunden. IP: "); Serial.println(WiFi.localIP());
  } else Serial.println("WARN: WLAN nicht verbunden.");

  // Serial2 initialisieren (auch wenn WLAN später kommt, aber we start here)
  MySerial.begin(38400,SERIAL_8N1,UART2_RX_PIN,UART2_TX_PIN);
  serial2Started = true;
  Serial.println("Serial2 gestartet.");

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  // initial publishes
  lastTempMillis = 0;
  publishRelayState(true);
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
    publishTemp(); // sende Temp bei Update
    Serial.print("T: "); Serial.print(tempC); Serial.println(" °C");
  }

  if(now - lastStatePublish >= STATE_PUBLISH_INTERVAL){
    lastStatePublish = now;
    publishRelayState(false);
  }
}
