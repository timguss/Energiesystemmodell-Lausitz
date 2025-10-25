// esp1_full.ino
// ESP1: Relays (8) + NTC-Temperatur + RS232 (Serial2) + WebServer /cmd + periodic register to esp3
// Requires: ArduinoJson (6.x)
// Serial monitor: 115200

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------- WLAN / Ziel-AP ----------
const char* AP_SSID = "ESP-AP";       // must match ESP3 AP SSID
const char* AP_PASS = "espap123";     // must match ESP3 AP PASS
const char* DEVICE_ID = "esp1";
const char* AP_ADDR = "192.168.4.1";  // ESP3 IP (AP)

// ---------- HTTP server ----------
WebServer server(80);

// ---------- Relais-Konfiguration (Pins + Titel) ----------
struct RelayConfig { uint8_t pin; const char* title; };
RelayConfig RELAYS[8] = {
  {19, "Ventil - 1"},
  {21, "Ventil - 2"},
  {22, "Heizstab"},
  {23, "Zünder"},
  {32, "Gasventil"},
  {33, "Kühler"},
  {25, "MFC"},
  {26, "Reserve"}
};
#define NUM_RELAYS 8
bool relayState[NUM_RELAYS] = {0,0,0,0,0,0,0,0};

const bool RELAY_ACTIVE_LOW = true; // if your relay board is ACTIVE LOW

// ---------- NTC (ADC) ----------
const int adcPin = 34;
const float Vcc = 3.3;
const float Rf = 10000.0;
const float R0 = 10000.0;
const float T0_K = 298.15;  // 25°C
const float beta = 3950.0;

// ---------- RS232 (Serial2) ----------
HardwareSerial RS232(2);
const int RS232_RX_PIN = 16; // RX2
const int RS232_TX_PIN = 17; // TX2
const unsigned long RS232_READ_TIMEOUT = 500; // ms

// ---------- Registration / timing ----------
unsigned long lastRegisterMillis = 0;
const unsigned long REGISTER_INTERVAL = 2000; // ms

// ---------- Helpers: NTC ----------
float readVoltage(){
  return (float)analogRead(adcPin) / 4095.0 * Vcc;
}

float readR_NTC(){
  float v = readVoltage();
  if(v <= 0.0001) return 1e9;
  if(v >= Vcc - 0.0001) return 1e-6;
  return Rf * (v / (Vcc - v));
}

float ntcToCelsius(float Rntc){
  float invT = (1.0 / T0_K) + (1.0 / beta) * log(Rntc / R0);
  return 1.0 / invT - 273.15;
}

// build status JSON (status object only)
String buildStatusJsonString(float tempC){
  DynamicJsonDocument doc(1024);
  JsonObject s = doc.createNestedObject("status");
  s["temp"] = tempC;
  JsonArray r = s.createNestedArray("relays");
  for(int i=0;i<NUM_RELAYS;i++) r.add(relayState[i]?1:0);
  JsonArray names = s.createNestedArray("relay_names");
  for(int i=0;i<NUM_RELAYS;i++) names.add(RELAYS[i].title);
  String out;
  serializeJson(s, out);
  return out;
}

void applyRelay(int idx, int val){
  if(idx < 0 || idx >= NUM_RELAYS) return;
  relayState[idx] = (val != 0);
  uint8_t pin = RELAYS[idx].pin;
  if(RELAY_ACTIVE_LOW){
    digitalWrite(pin, relayState[idx] ? LOW : HIGH);
  } else {
    digitalWrite(pin, relayState[idx] ? HIGH : LOW);
  }
  Serial.printf("Relay %d (%s) -> %d (pin %d)\n", idx, RELAYS[idx].title, relayState[idx]?1:0, pin);
}

// send register to ESP3
bool sendRegister(float tempC){
  if(WiFi.status() != WL_CONNECTED) return false;

  DynamicJsonDocument full(1536);
  full["id"] = DEVICE_ID;
  full["ip"] = WiFi.localIP().toString();

  JsonObject s = full.createNestedObject("status");
  s["temp"] = tempC;

  JsonArray rel = s.createNestedArray("relays");
  JsonArray names = s.createNestedArray("relay_names");
  for(int i=0;i<NUM_RELAYS;i++){
    rel.add(relayState[i]?1:0);
    names.add(RELAYS[i].title);
  }

  String body;
  serializeJson(full, body);

  Serial.println("POST to ESP3: " + body);

  HTTPClient http;
  String url = String("http://") + AP_ADDR + "/register";
  http.begin(url);
  http.addHeader("Content-Type","application/json");
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  Serial.printf("Register -> code=%d resp=%s\n", code, resp.c_str());
  return (code >= 200 && code < 300);
}

// ---- /cmd handler ----
void handleCmd(){
  if(!server.hasArg("plain")){
    server.send(400,"text/plain","no body");
    return;
  }
  String body = server.arg("plain");
  Serial.println("HTTP POST /cmd  body: " + body);

  // RS232 command
  if(body.startsWith("rs232:")){
    String payload = body.substring(6);
    if(!payload.endsWith("\r\n")) payload += "\r\n";
    Serial.print("RS232 send -> "); Serial.print(payload);
    RS232.print(payload);

    String resp = "";
    unsigned long start = millis();
    while(millis() - start < RS232_READ_TIMEOUT){
      while(RS232.available()){
        char c = (char)RS232.read();
        resp += c;
      }
      delay(5);
    }
    Serial.print("RS232 reply -> "); Serial.println(resp);
    server.send(200, "text/plain", resp);
    return;
  }

  // Relay command idx:val
  int colon = body.indexOf(':');
  if(colon > 0){
    String a = body.substring(0, colon);
    String b = body.substring(colon + 1);
    int idx = a.toInt();
    int val = b.toInt();
    if(idx >= 0 && idx < NUM_RELAYS){
      applyRelay(idx, val);
      float Rntc = readR_NTC();
      float tempC = ntcToCelsius(Rntc);
      String status = buildStatusJsonString(tempC);
      server.send(200,"application/json", status);
      return;
    }
  }

  server.send(400,"text/plain","bad cmd");
}

// /status handler for direct queries
void handleStatus(){
  float Rntc = readR_NTC();
  float tempC = ntcToCelsius(Rntc);
  String status = buildStatusJsonString(tempC);
  server.send(200,"application/json", status);
}

// ----------------- setup / loop -----------------
void setup(){
  Serial.begin(115200);
  delay(50);
  Serial.println("\n=== ESP1 starting ===");

  // setup relay pins
  for(int i=0;i<NUM_RELAYS;i++){
    pinMode(RELAYS[i].pin, OUTPUT);
    if(RELAY_ACTIVE_LOW) digitalWrite(RELAYS[i].pin, HIGH);
    else digitalWrite(RELAYS[i].pin, LOW);
  }

  // ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // RS232 Serial2
  RS232.begin(38400, SERIAL_8N1, RS232_RX_PIN, RS232_TX_PIN);
  Serial.println("Serial2 (RS232) started (baud 38400)");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(AP_SSID, AP_PASS);
  Serial.printf("Connecting to AP '%s' ...\n", AP_SSID);
  unsigned long start = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - start < 15000){
    delay(300);
    Serial.print(".");
  }
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection timeout!");
  }

  // start HTTP server
  server.on("/cmd", HTTP_POST, handleCmd);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
  Serial.println("HTTP server started: /cmd, /status");
}

void loop(){
  server.handleClient();

  unsigned long now = millis();
  if(now - lastRegisterMillis >= REGISTER_INTERVAL){
    lastRegisterMillis = now;
    float Rntc = readR_NTC();
    float tempC = ntcToCelsius(Rntc);
    bool ok = sendRegister(tempC);
    if(!ok) Serial.println("Register failed (will retry).");
    else Serial.println("✔ register successful");
  }

  // forward any incoming RS232 data to main serial for debugging
  while(RS232.available()){
    char c = (char)RS232.read();
    Serial.write(c);
  }
}
