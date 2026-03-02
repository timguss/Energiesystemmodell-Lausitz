// ESP2 mit HTTP-API und Temperaturmessung
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <math.h>

// ===== WiFi & Network =====
const char* WIFI_SSID = "ESP-HOST";
const char* WIFI_PASS = "espHostPass";
const IPAddress HOST_IP(192,168,4,1);

// ===== Relais =====
const int relays[] = {26, 25, 33, 32};
const int relayCount = 4;
const char* relayNames[] = {"unbelegt0", "unbelegt1", "unbelegt2", "unbelegt3"};

// ===== Temperaturmessung =====
const int adcPin = 34;  // ADC Pin für NTC
const float Vcc = 3.3;
const float Rf = 10000.0;      // Vorwiderstand in Ohm
const float R0 = 10000.0;      // NTC Widerstand bei 25°C
const float T0_K = 298.15;     // 25°C in Kelvin
const float beta = 3950.0;     // Beta-Koeffizient des NTC

float cachedTempC = NAN;
float cachedRntc = NAN;
unsigned long lastTempMillis = 0;
const unsigned long TEMP_INTERVAL = 1000;  // Alle 1 Sekunde messen

// ===== Webserver =====
WebServer server(80);

// ===== Temperatur-Funktionen =====
float readVoltage() {
  int adc = analogRead(adcPin);
  return (float)adc / 4095.0 * Vcc;
}

float readR_NTC() {
  float v = readVoltage();
  if(v <= 0.0001) return 1e9;
  if(v >= Vcc - 0.0001) return 1e-6;
  return Rf * (v / (Vcc - v));
}

float ntcToCelsius(float Rntc) {
  float invT = (1.0 / T0_K) + (1.0 / beta) * log(Rntc / R0);
  return 1.0 / invT - 273.15;
}

// ===== Relay Helper =====
void setRelay(int index, bool state) {
  if (index >= 0 && index < relayCount) {
    digitalWrite(relays[index], state ? LOW : HIGH);
  }
}

int getRelay(int index) {
  if (index >= 0 && index < relayCount) {
    return digitalRead(relays[index]) == LOW ? 1 : 0;
  }
  return 0;
}

// ===== HTTP Handlers =====

// GET /state -> {"r0":0,"r1":1,"r2":0,"r3":0,"temp":23.45,"rntc":10234}
void handleState() {
  String s = "{";
  for(int i = 0; i < relayCount; i++) {
    s += "\"r" + String(i) + "\":" + String(getRelay(i));
    s += ",";
  }
  // Füge Temperaturdaten hinzu
  s += "\"temp\":" + (isnan(cachedTempC) ? "null" : String(cachedTempC, 2)) + ",";
  s += "\"rntc\":" + (isnan(cachedRntc) ? "null" : String(cachedRntc, 0));
  s += "}";
  server.send(200, "application/json", s);
}

// GET /set?idx=0&val=1
void handleSet() {
  if(!server.hasArg("idx") || !server.hasArg("val")) {
    server.send(400, "text/plain", "missing args");
    return;
  }
  int idx = server.arg("idx").toInt();
  int val = server.arg("val").toInt();
  
  if(idx < 0 || idx >= relayCount || (val != 0 && val != 1)) {
    server.send(400, "text/plain", "invalid args");
    return;
  }
  
  setRelay(idx, val == 1);
  Serial.printf("Relay %d (%s) -> %s\n", idx, relayNames[idx], (val == 1) ? "AN" : "AUS");
  server.send(200, "text/plain", "ok");
}

// GET /meta -> {"count":4,"names":["unbelegt0",...]}
void handleMeta() {
  String s = "{\"count\":" + String(relayCount) + ",\"names\":[";
  for(int i = 0; i < relayCount; i++) {
    s += "\"" + String(relayNames[i]) + "\"";
    if(i < relayCount - 1) s += ",";
  }
  s += "]}";
  server.send(200, "application/json", s);
}

// GET /temp -> {"temp":23.45,"rntc":10234}
void handleTemp() {
  String j = "{\"temp\":" + (isnan(cachedTempC) ? "null" : String(cachedTempC, 2)) +
             ",\"rntc\":" + (isnan(cachedRntc) ? "null" : String(cachedRntc, 0)) + "}";
  server.send(200, "application/json", j);
}

// ===== Host Registration =====
void registerAtHost() {
  if(WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  String url = String("http://") + HOST_IP.toString() + "/register";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(500);
  String payload = "{\"name\":\"esp2\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  http.POST(payload);
  http.end();
}

// ===== WiFi Events =====
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    registerAtHost();
  } else if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    Serial.println("WiFi getrennt, reconnect...");
    WiFi.reconnect();
  }
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nESP2 startet...");
  
  // Relais initialisieren (aktive LOW: HIGH = AUS)
  for (int i = 0; i < relayCount; i++) {
    pinMode(relays[i], OUTPUT);
    digitalWrite(relays[i], HIGH);
    Serial.printf("Relay %d (%s) auf Pin %d\n", i, relayNames[i], relays[i]);
  }
  
  // ADC initialisieren
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  Serial.println("ADC initialisiert für Temperaturmessung");
  
  // WiFi
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.print("Verbinde mit ");
  Serial.print(WIFI_SSID);
  unsigned long start = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi verbunden!");
    registerAtHost();
  }
  
  // HTTP Server
  server.on("/state", HTTP_GET, handleState);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/meta", HTTP_GET, handleMeta);
  server.on("/temp", HTTP_GET, handleTemp);
  server.begin();
  Serial.println("HTTP Server gestartet");
  
  Serial.println("\nESP2 bereit!");
  Serial.println("HTTP-API: /state, /set, /meta, /temp");
}

// ===== Loop =====
void loop() {
  server.handleClient();
  
  unsigned long now = millis();
  
  // Temperatur alle 1 Sekunde messen
  if(now - lastTempMillis >= TEMP_INTERVAL) {
    lastTempMillis = now;
    float Rntc = readR_NTC();
    float tempC = ntcToCelsius(Rntc);
    cachedTempC = tempC;
    cachedRntc = Rntc;
    Serial.printf("ESP2 Temp: %.2f °C | R: %.0f Ω\n", tempC, Rntc);
  }
  
  // Host-Registrierung alle 60 Sekunden
  static unsigned long lastReg = 0;
  if(now - lastReg > 60000) {
    lastReg = now;
    registerAtHost();
  }
}