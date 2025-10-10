#include <WiFi.h>
#include <PubSubClient.h>
#include "Credentials.h"

const char* deviceID = "esp2";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTempMillis = 0;
const unsigned long TEMP_INTERVAL = 1000;

float cachedTempC = NAN;
float cachedRntc = NAN;
bool relayState[4] = {0,0,0,0};

// Relais Pins & Namen
struct RelayConfig { uint8_t pin; const char* title; };
RelayConfig RELAYS[4] = {
  {32, "Ventil"}, {33, "Heizstab"}, {25, "Kühler"}, {26, "unbelegt"}
};

const int adcPin = 34;
const float Vcc = 3.3;
const float Rf = 10000.0;
const float R0 = 10000.0;
const float T0_K = 298.15;
const float beta = 3950.0;

void setup_wifi() {
  Serial.print("🔌 Connecting to WiFi: "); Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int c=0;
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
    c++;
    if(c>30){
      Serial.println("❌ WiFi timeout, restarting...");
      ESP.restart();
    }
  }
  Serial.print("✅ Connected. IP: "); Serial.println(WiFi.localIP());
}

void connectMQTT() {
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
  while (!client.connected()) {
    if(WiFi.status()!=WL_CONNECTED) setup_wifi();
    Serial.print("🔄 Connecting to MQTT...");
    if(client.connect(deviceID, MQTT_USER, MQTT_PASS)){
      Serial.println("✅ Connected!");
      client.subscribe(("rpi/"+String(deviceID)+"/set").c_str());
    } else {
      Serial.print("❌ failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

float readVoltage(){ return (float)analogRead(adcPin)/4095.0*Vcc; }
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

void publishStatus() {
  String tempMsg = String(cachedTempC,2);
  client.publish(("esp/"+String(deviceID)+"/temp").c_str(), tempMsg.c_str());

  String relayMsg = "{";
  for(int i=0;i<4;i++){
    relayMsg += String(relayState[i]?1:0);
    if(i<3) relayMsg+=",";
  }
  relayMsg += "}";
  client.publish(("esp/"+String(deviceID)+"/relays").c_str(), relayMsg.c_str());

  Serial.print("📤 Published status - Temp: "); Serial.print(cachedTempC);
  Serial.print(" | Relays: "); 
  for(int i=0;i<4;i++) Serial.print(relayState[i]?1:0); Serial.print(" ");
  Serial.println();
}

void callback(char* topic, byte* payload, unsigned int length){
  String msg;
  for(int i=0;i<length;i++) msg += (char)payload[i];

  String t = String(topic);
  Serial.print("📨 Received MQTT: "); Serial.print(t); Serial.print(" -> "); Serial.println(msg);

  if(t.endsWith("/set")){
    int colon = msg.indexOf(':');
    if(colon>0){
      int idx = msg.substring(0,colon).toInt();
      int val = msg.substring(colon+1).toInt();
      if(idx>=0 && idx<4){
        relayState[idx] = val;
        digitalWrite(RELAYS[idx].pin, val?LOW:HIGH); // ACTIVE_LOW
        Serial.print("💡 Relay "); Serial.print(idx); Serial.print(" set to "); Serial.println(val);
        publishStatus();
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP2 Starting ===");

  for(int i=0;i<4;i++){
    pinMode(RELAYS[i].pin, OUTPUT);
    digitalWrite(RELAYS[i].pin, HIGH);
  }

  analogReadResolution(12); analogSetAttenuation(ADC_11db);

  setup_wifi();
  connectMQTT();
}

void loop() {
  client.loop();
  if(!client.connected()) connectMQTT();

  if(millis() - lastTempMillis >= TEMP_INTERVAL){
    lastTempMillis = millis();
    cachedRntc = readR_NTC();
    cachedTempC = ntcToCelsius(cachedRntc);
    Serial.print("🌡 Temp: "); Serial.print(cachedTempC); Serial.print(" °C | Rntc: "); Serial.println(cachedRntc);
    publishStatus();
  }
}
