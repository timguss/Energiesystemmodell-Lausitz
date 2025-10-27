// ESP2 mit HTTP-API und MQTT
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

// ===== WiFi & Network =====
const char* WIFI_SSID = "ESP-HOST";
const char* WIFI_PASS = "espHostPass";
const IPAddress HOST_IP(192,168,4,1);

// ===== MQTT (optional, falls du es noch brauchst) =====
const char* mqtt_server = "192.168.4.1";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ===== Relais =====
const int relays[] = {26, 25, 33, 32};
const int relayCount = 4;
const char* relayNames[] = {"unbelegt0", "unbelegt1", "unbelegt2", "unbelegt3"};

// ===== Webserver =====
WebServer server(80);

// ===== Relay Helper =====
// state = true -> RELAY EIN (aktive LOW)
// state = false -> RELAY AUS (HIGH)
void setRelay(int index, bool state) {
  if (index >= 0 && index < relayCount) {
    // Relais sind aktiv LOW: einschalten = LOW, ausschalten = HIGH
    digitalWrite(relays[index], state ? LOW : HIGH);
  }
}

int getRelay(int index) {
  if (index >= 0 && index < relayCount) {
    // Bei aktiv LOW ist LOW = 1 (AN)
    return digitalRead(relays[index]) == LOW ? 1 : 0;
  }
  return 0;
}

// ===== HTTP Handlers =====

// GET /state -> {"r0":0,"r1":1,"r2":0,"r3":0}
void handleState() {
  String s = "{";
  for(int i = 0; i < relayCount; i++) {
    s += "\"r" + String(i) + "\":" + String(getRelay(i));
    if(i < relayCount - 1) s += ",";
  }
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

// GET /meta -> {"count":4,"names":["Licht","Pumpe","Ventil","Reserve"]}
void handleMeta() {
  String s = "{\"count\":" + String(relayCount) + ",\"names\":[";
  for(int i = 0; i < relayCount; i++) {
    s += "\"" + String(relayNames[i]) + "\"";
    if(i < relayCount - 1) s += ",";
  }
  s += "]}";
  server.send(200, "application/json", s);
}

// ===== Host Registration =====
void registerAtHost() {
  if(WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  String url = String("http://") + HOST_IP.toString() + "/register";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"name\":\"esp2\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  http.POST(payload);
  http.end();
}

// ===== MQTT Callback (optional) =====
void mqttCallback(char* topic, byte* message, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)message[i];
  }
  
  if (String(topic).startsWith("esp2/relay")) {
    int relayNum = String(topic).substring(10).toInt();
    bool state = msg == "ON";
    setRelay(relayNum, state);
    Serial.printf("MQTT: Relay %d -> %s\n", relayNum, state ? "AN" : "AUS");
  }
}

void mqttReconnect() {
  while (!mqttClient.connected()) {
    Serial.print("MQTT Verbindung...");
    if (mqttClient.connect("ESP2_Client")) {
      Serial.println(" OK");
      for (int i = 0; i < relayCount; i++) {
        String topic = "esp2/relay" + String(i);
        mqttClient.subscribe(topic.c_str());
      }
    } else {
      Serial.print(" Fehler, rc=");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
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
    digitalWrite(relays[i], HIGH); // sicherstellen: AUS-Zustand
    Serial.printf("Relay %d (%s) auf Pin %d\n", i, relayNames[i], relays[i]);
  }
  
  // WiFi
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
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
  server.begin();
  Serial.println("HTTP Server gestartet");
  
  // MQTT deaktiviert (verursacht Blockierung)
  // mqttClient.setServer(mqtt_server, mqtt_port);
  // mqttClient.setCallback(mqttCallback);
  
  Serial.println("\nESP2 bereit!");
  Serial.println("HTTP-API: /state, /set, /meta");
}

// ===== Loop =====
void loop() {
  server.handleClient();
  
  // MQTT deaktiviert - blockiert zu lange
  // if (!mqttClient.connected()) {
  //   mqttReconnect();
  // }
  // mqttClient.loop();
  
  // Host-Registrierung alle 60 Sekunden
  static unsigned long lastReg = 0;
  if(millis() - lastReg > 60000) {
    lastReg = millis();
    registerAtHost();
  }
};
