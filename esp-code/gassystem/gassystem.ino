#include <WiFi.h>
#include <PubSubClient.h>
#include "Credentials.h"

const char* deviceID = "esp1";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTempMillis = 0;
const unsigned long TEMP_INTERVAL = 1000;

float cachedTempC = NAN;
float cachedRntc = NAN;
bool relayState[8] = {0,0,0,0,0,0,0,0};

// Relais Pins & Namen
struct RelayConfig { uint8_t pin; const char* title; };
RelayConfig RELAYS[8] = {
  {19, "Ventil - 1"}, {21, "Ventil - 2"}, {22, "Heizstab"}, {23, "Zünder"},
  {32, "Gasventil"}, {33, "Kühler"}, {25, "MFC - fehlt noch"}, {26, "unbelegt"}
};

const int adcPin = 34;
const float Vcc = 3.3;
const float Rf = 10000.0;
const float R0 = 10000.0;
const float T0_K = 298.15;  // 25°C in Kelvin
const float beta = 3950.0;

HardwareSerial MySerial(2); // UART2
bool serial2Started = false;
bool rs232Enabled = true;

void setup_wifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(WiFi.status() != WL_CONNECTED) delay(500);
}

void connectMQTT() {
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
  while (!client.connected()) {
    client.connect(deviceID, MQTT_USER, MQTT_PASS);
    delay(500);
  }
  client.subscribe(("rpi/"+String(deviceID)+"/set").c_str());
  client.subscribe(("rpi/"+String(deviceID)+"/send").c_str());
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
  for(int i=0;i<8;i++){
    relayMsg += String(relayState[i]?1:0);
    if(i<7) relayMsg+=",";
  }
  relayMsg += "}";
  client.publish(("esp/"+String(deviceID)+"/relays").c_str(), relayMsg.c_str());
}

void callback(char* topic, byte* payload, unsigned int length){
  String msg;
  for(int i=0;i<length;i++) msg += (char)payload[i];

  String t = String(topic);
  if(t.endsWith("/set")){
    // Format: "idx:val", z.B. "2:1"
    int colon = msg.indexOf(':');
    if(colon>0){
      int idx = msg.substring(0,colon).toInt();
      int val = msg.substring(colon+1).toInt();
      if(idx>=0 && idx<8) {
        relayState[idx] = val;
        digitalWrite(RELAYS[idx].pin, val?LOW:HIGH); // ACTIVE_LOW
        publishStatus();
      }
    }
  } else if(t.endsWith("/send")){
    if(rs232Enabled && serial2Started){
      if(!msg.endsWith("\r\n")) msg += "\r\n";
      MySerial.print(msg);
      String resp = "";
      unsigned long start = millis();
      while(millis()-start < 500){
        while(MySerial.available()) resp += (char)MySerial.read();
      }
      client.publish(("esp/"+String(deviceID)+"/reply").c_str(), resp.c_str());
    }
  }
}

void setup() {
  Serial.begin(115200);
  for(int i=0;i<8;i++){
    pinMode(RELAYS[i].pin, OUTPUT);
    digitalWrite(RELAYS[i].pin, HIGH);
  }

  analogReadResolution(12); analogSetAttenuation(ADC_11db);
  WiFi.begin(WIFI_SSID,WIFI_PASS);
  while(WiFi.status()!=WL_CONNECTED) delay(500);
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);

  MySerial.begin(38400,SERIAL_8N1,16,17);
  serial2Started = true;
}

void loop() {
  client.loop();
  if(!client.connected()) connectMQTT();

  if(millis() - lastTempMillis >= TEMP_INTERVAL){
    lastTempMillis = millis();
    cachedRntc = readR_NTC();
    cachedTempC = ntcToCelsius(cachedRntc);
    publishStatus();
  }
}
