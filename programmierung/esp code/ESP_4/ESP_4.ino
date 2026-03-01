// ESP4_api.ino
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// -------------------- RELAIS --------------------
const bool ACTIVE_LOW = true;

struct RelayConfig {
  uint8_t pin;
  const char* name;
  bool activeLow;
};

const uint8_t RELAY_COUNT = 5;

RelayConfig RELAYS[RELAY_COUNT] = {
  {18, "Relay 1", false}, // First relay reversed (High Trigger)
  {19, "Relay 2", true},  // Others stay Active Low
  {21, "Relay 3", true},
  {22, "Relay 4", true},
  {23, "Relay 5", true},
};

float currents[5];
float pressures[5];
int relayState[RELAY_COUNT];
bool hostConnected = false;

// -------------------- WIFI --------------------
const char* WIFI_SSID = "ESP-HOST";
const char* WIFI_PASS = "espHostPass";

// -------------------- FLOWMETER KONFIG --------------------
#define FLOW_PIN 17
float K_FACTOR = 1.0; // Kalibrierung: Pulse pro Liter
volatile uint32_t pulseCount = 0;
float flow_L_per_min = 0;
unsigned long lastFlowMillis = 0;

void IRAM_ATTR onPulse() {
  pulseCount++;
}

// -------------------- SENSOR KONFIG --------------------
// 5x 4–20mA Sensoren 
const int SENSOR_PINS[5] = {36,39,34,35, 32};   // nur ADC Pins!
const float SHUNT_RESISTOR = 165.0;

// Gemeinsamer Nullpunkt
const float I_ZERO = 3.19;   // mA
const float I_FULL = 20.0;   // mA

// Druckbereiche
const float PRESSURE_MAX[5] = {
  4.0,
  4.0,
  6.0,
  10.0,
  10.0
};


// -------------------- SERVER --------------------
WebServer server(80);

float pressureValues[5];
float currentValues[5];

// ------------------------------------------------

inline int physToLogical(int physVal, bool activeLow){
  return activeLow ? (physVal==LOW?1:0) : (physVal==HIGH?1:0);
}

inline int logicalToPhys(int logical, bool activeLow){
  return activeLow ? (logical==1?LOW:HIGH) : (logical==1?HIGH:LOW);
}

// -------------------- SENSOR LESEN --------------------
void readSensors() {
  // 1. Druck/Strom Sensoren
  for(int i=0;i<5;i++){
    int rawADC = analogRead(SENSOR_PINS[i]);
    float voltage = (rawADC / 4095.0) * 3.3;
    float current_mA = (voltage / SHUNT_RESISTOR) * 1000.0;
    float pressure_bar = ((current_mA - I_ZERO) / (I_FULL - I_ZERO)) * PRESSURE_MAX[i];
    if (pressure_bar < 0) pressure_bar = 0;
    if (pressure_bar > PRESSURE_MAX[i]) pressure_bar = PRESSURE_MAX[i];
    currentValues[i] = current_mA;
    pressureValues[i] = pressure_bar;
  }

  // 2. Flowmeter (alle 1s berechnen)
  unsigned long now = millis();
  if (now - lastFlowMillis >= 1000) {
    uint32_t duration = now - lastFlowMillis;
    lastFlowMillis = now;
    
    noInterrupts();
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    float pulsesPerSecond = pulses / (duration / 1000.0);
    flow_L_per_min = (pulsesPerSecond * 60.0) / K_FACTOR;
  }
}

// -------------------- JSON STATE --------------------
void handleState(){

  readSensors();

  String s="{";

  // Sensoren
  s += "\"sensors\":[";
  for(int i=0;i<5;i++){
    s += "{";
    s += "\"current\":" + String(currentValues[i],2) + ",";
    s += "\"pressure\":" + String(pressureValues[i],2);
    s += "}";
    if(i<4) s+=",";
  }
  s += "],";

  // Relais
  s += "\"relays\":[";
  for(int i=0;i<RELAY_COUNT;i++){
    s += String(physToLogical(digitalRead(RELAYS[i].pin), RELAYS[i].activeLow));
    if(i<RELAY_COUNT-1) s+=",";
  }
  s += "]";

  // Flowmeter
  s += ",\"flow\":" + String(flow_L_per_min, 2);

  s+="}";

  server.send(200,"application/json",s);
}

// -------------------- RELAIS SETZEN --------------------
void handleSet(){

  if(!server.hasArg("idx") || !server.hasArg("val")){
    server.send(400,"text/plain","missing args");
    return;
  }

  int idx = server.arg("idx").toInt();
  int val = server.arg("val").toInt();

  if(idx<0 || idx>=RELAY_COUNT){
    server.send(400,"text/plain","invalid idx");
    return;
  }

  digitalWrite(RELAYS[idx].pin, logicalToPhys(val, RELAYS[idx].activeLow));
  relayState[idx] = val;
  server.send(200,"application/json","{\"ok\":true,\"idx\":" + String(idx) + ",\"val\":" + String(val) + "}");
}

// -------------------- META --------------------
void handleMeta(){
  String s = "{\"count\":" + String(RELAY_COUNT) + ",\"names\":[";
  for(int i=0; i<RELAY_COUNT; i++){
    s += "\"" + String(RELAYS[i].name) + "\"";
    if(i < RELAY_COUNT - 1) s += ",";
  }
  s += "]}";
  server.send(200,"application/json", s);
}

void printStatus() {
  Serial.println("\n--- ESP4 Status ---");

  for (int i = 0; i < 5; i++) {
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(currentValues[i], 2);   // <-- currentValues statt currents
    Serial.print(" mA | ");
    Serial.print(pressureValues[i], 2);  // <-- pressureValues statt pressures
    Serial.println(" bar");
  }

  Serial.print("Relais: [");
  for (int i = 0; i < RELAY_COUNT; i++) {
    Serial.print(relayState[i]);
    if (i < RELAY_COUNT - 1) Serial.print(",");
  }
  Serial.println("]");

  Serial.print("Flow: ");
  Serial.print(flow_L_per_min, 2);
  Serial.println(" L/min");
  Serial.println("-------------------");
}

// -------------------- HOST REG --------------------
void registerWithHost() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin("http://192.168.4.1/register");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(2000); // Short timeout so we don't block web server

  String payload = "{\"name\":\"esp4\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    if (!hostConnected) {
      Serial.println("ESP4 erfolgreich mit Host verbunden.");
    }
    hostConnected = true;
  } else {
    if (hostConnected) {
      Serial.println("Verbindung zum Host verloren!");
    }
    hostConnected = false;
  }

  http.end();
}

// -------------------- SETUP --------------------
void setup(){

  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Flowmeter init
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onPulse, FALLING);

  // Relais init
  for(int i=0;i<RELAY_COUNT;i++){
    pinMode(RELAYS[i].pin, OUTPUT);
    digitalWrite(RELAYS[i].pin, logicalToPhys(0, RELAYS[i].activeLow));
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);  // Auto-reconnect on WiFi drop
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi");
  unsigned long wifiStart = millis();
  while(WiFi.status()!=WL_CONNECTED && millis() - wifiStart < 10000){
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    registerWithHost();
  } else {
    Serial.println("WiFi connection timeout, will retry...");
  }

  server.on("/state", HTTP_GET, handleState);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/meta", HTTP_GET, handleMeta);

  server.begin();
}

// -------------------- LOOP --------------------
unsigned long lastPrint = 0;
unsigned long lastCheck = 0;

void loop() {
  server.handleClient();

  unsigned long now = millis();

  // Print status every 2 seconds (reduced frequency to lower load)
  if (now - lastPrint > 2000) {
    readSensors();   
    printStatus();
    lastPrint = now;
  }

  // Re-register with host every 15 seconds
  if (now - lastCheck > 15000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      // Non-blocking reconnect attempt
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    } else {
      registerWithHost();
    }
    lastCheck = now;
  }
}
