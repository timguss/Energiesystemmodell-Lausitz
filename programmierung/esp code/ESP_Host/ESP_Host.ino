/*
 * ESP-Host mit Scenario-Support (ESP32 Version) - FIXED
 * 
 * Funktionen:
 * - WiFi Access Point
 * - Web Server mit /forward Endpoint
 * - Client Management (esp1, esp2, esp3)
 * - Scenario Execution (/scenario?name=X&state=Y)
 * 
 * Hardware: ESP32 Dev Module
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

// WiFi Access Point
const char* AP_SSID = "ESP-HOST";
const char* AP_PASSWORD = "espHostPass";

// Web Server
WebServer server(80);

// Client ESP URLs (werden automatisch registriert)
#define MAX_CLIENTS 10

struct ClientDevice {
  String name;
  String ip;
  unsigned long lastSeen;
};

ClientDevice clients[MAX_CLIENTS];

// Monitoring
int lastStationCount = 0;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Findet Client-IP anhand des Namens
String getClientIP(String name) {
  Serial.println("[getClientIP] Searching for: '" + name + "'");
  
  for (int i = 0; i < MAX_CLIENTS; i++) {
    Serial.printf("[getClientIP] Checking slot %d: name='%s', ip='%s'\n", 
                  i, clients[i].name.c_str(), clients[i].ip.c_str());
    
    if (clients[i].name.length() > 0 && clients[i].name.equals(name)) {
      Serial.println("[getClientIP] Found! IP: " + clients[i].ip);
      return clients[i].ip;
    }
  }
  
  Serial.println("[getClientIP] NOT FOUND!");
  return "";
}

// Registriert oder aktualisiert einen Client
void registerClient(String name, String ip) {
  // Prüfe ob Client bereits existiert
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].name.equals(name)) {
      clients[i].ip = ip;
      clients[i].lastSeen = millis();
      Serial.println("[Client] Updated: " + name + " @ " + ip);
      return;
    }
  }
  
  // Neuer Client - finde freien Slot
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].name.length() == 0) {
      clients[i].name = name;
      clients[i].ip = ip;
      clients[i].lastSeen = millis();
      Serial.println("[Client] Registered: " + name + " @ " + ip);
      return;
    }
  }
  
  Serial.println("[Client] ERROR: Max clients reached!");
}

// Forward Request zu einem Client-ESP mit Retry-Logik
String forwardRequest(String target, String method, String path, String body) {
  String ip = getClientIP(target);
  if (ip == "") {
    return "{\"error\":\"Client not found\"}";
  }
  
  // Update lastSeen
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].name.equals(target)) {
      clients[i].lastSeen = millis();
      break;
    }
  }
  
  String url = "http://" + ip + path;
  int retryCount = 0;
  int httpCode = -1;
  String payload = "";

  // Retry-Schleife (max 2 Versuche)
  while (retryCount < 2 && httpCode <= 0) {
    HTTPClient httpLocal;
    httpLocal.begin(url);
    httpLocal.setTimeout(1500); // 1.5s pro Versuch
    httpLocal.addHeader("Connection", "close"); // Keine persistenten Verbindungen (stabiler für AP)
    
    if (method == "GET") {
      httpCode = httpLocal.GET();
    } else {
      httpLocal.addHeader("Content-Type", "application/json");
      httpCode = httpLocal.POST(body);
    }

    if (httpCode > 0) {
      payload = httpLocal.getString();
    }
    httpLocal.end();
    
    if (httpCode <= 0) {
      retryCount++;
      if (retryCount < 2) delay(50); // Kurz warten vor Retry
    }
  }
  
  // JSON Response zusammenbauen
  String response = "{\"code\":" + String(httpCode) + ",\"body\":";
  if (httpCode > 0 && payload.length() > 0) {
    if (payload.startsWith("{") || payload.startsWith("[")) {
      response += payload;
    } else {
      response += "\"" + payload + "\"";
    }
  } else {
    response += "{\"error\":\"HTTP error " + String(httpCode) + "\"}";
  }
  response += "}";
  
  return response;
}

// Delay mit Yield (verhindert Watchdog Reset)
void delayWithYield(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    yield();
    delay(1);
  }
}

// Schaltet ein Relay über Forward-Request
// Returns true on success, false on failure
bool setRelayByForward(String device, int idx, int val) {
  String path = "/set?idx=" + String(idx) + "&val=" + String(val);
  String response = forwardRequest(device, "GET", path, "");
  Serial.println("[Relay] " + device + " idx=" + String(idx) + " val=" + String(val));
  
  // Check if request was successful (response should contain "code":200)
  if (response.indexOf("\"code\":200") > 0) {
    return true;
  } else {
    Serial.println("[Relay] ERROR: Failed to set relay - " + response);
    return false;
  }
}

// ============================================================================
// SCENARIO DEFINITIONS
// ============================================================================

void scenario_kohlekraftwerk(int state) {
  if (state == 0) {
    Serial.println("[Scenario] Kohlekraftwerk AUS");
    if (!setRelayByForward("esp1", 0, 0)) return;
    delay(50);
    if (!setRelayByForward("esp1", 1, 0)) return;
    delay(50);
    if (!setRelayByForward("esp1", 4, 0)) return;
    delay(50);
    if (!setRelayByForward("esp2", 0, 0)) return;
    Serial.println("[Scenario] Kohlekraftwerk AUS - Fertig");
  } else if (state == 1) {
    Serial.println("[Scenario] Kohlekraftwerk EIN - Start");
    if (!setRelayByForward("esp1", 0, 1)) return;
    delay(50);
    if (!setRelayByForward("esp1", 1, 1)) return;
    delay(50);
    if (!setRelayByForward("esp1", 4, 1)) return;
    delay(50);
    if (!setRelayByForward("esp2", 0, 1)) return;
    Serial.println("[Scenario] Warte 5 Sekunden...");
    delayWithYield(5000);
    Serial.println("[Scenario] Relay 2 wird ausgeschaltet");
    if (!setRelayByForward("esp1", 1, 0)) return;
    Serial.println("[Scenario] Kohlekraftwerk EIN - Fertig");
  }
}

void scenario_test(int state) {
  switch(state) {
    case 0:
      Serial.println("[Scenario] Test Reset");
      for (int i = 0; i < 8; i++) {
        if (!setRelayByForward("esp1", i, 0)) return;
        delay(50);
      }
      for (int i = 0; i < 4; i++) {
        if (!setRelayByForward("esp2", i, 0)) return;
        delay(50);
      }
      break;
      
    case 1:
      Serial.println("[Scenario] Test Sequenz 1");
      if (!setRelayByForward("esp1", 0, 1)) return;
      delayWithYield(2000);
      if (!setRelayByForward("esp1", 1, 1)) return;
      delayWithYield(2000);
      if (!setRelayByForward("esp1", 2, 1)) return;
      delayWithYield(2000);
      if (!setRelayByForward("esp1", 0, 0)) return;
      delayWithYield(1000);
      if (!setRelayByForward("esp1", 1, 0)) return;
      delayWithYield(1000);
      if (!setRelayByForward("esp1", 2, 0)) return;
      break;
      
    case 2:
      Serial.println("[Scenario] Test Sequenz 2");
      if (!setRelayByForward("esp2", 0, 1)) return;
      delayWithYield(1000);
      if (!setRelayByForward("esp2", 1, 1)) return;
      delayWithYield(1000);
      if (!setRelayByForward("esp2", 0, 0)) return;
      delayWithYield(1000);
      if (!setRelayByForward("esp2", 1, 0)) return;
      break;
  }
}

void scenario_alles(int state) {
  if (state == 0) {
    Serial.println("[Scenario] Alle Relays AUS");
    for (int i = 0; i < 8; i++) {
      if (!setRelayByForward("esp1", i, 0)) return;
      delay(50);
    }
    for (int i = 0; i < 4; i++) {
      if (!setRelayByForward("esp2", i, 0)) return;
      delay(50);
    }
  } else if (state == 1) {
    Serial.println("[Scenario] Alle Relays AN");
    for (int i = 0; i < 8; i++) {
      if (!setRelayByForward("esp1", i, 1)) return;
      delay(50);
    }
    for (int i = 0; i < 4; i++) {
      if (!setRelayByForward("esp2", i, 1)) return;
      delay(50);
    }
  }
}

// ============================================================================
// WEB SERVER HANDLERS
// ============================================================================

// GET /clients - Liste aller registrierten Clients
void handleClients() {
  String json = "{\"clients\":[";
  bool first = true;
  
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].name.length() > 0) {
      if (!first) json += ",";
      json += "{\"name\":\"" + clients[i].name + "\",";
      json += "\"ip\":\"" + clients[i].ip + "\",";
      json += "\"lastSeen\":" + String(clients[i].lastSeen) + "}";
      first = false;
    }
  }
  json += "]}";
  
  server.send(200, "application/json", json);
}

// POST /register - Client registrieren
void handleRegister() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }
  
  String body = server.arg("plain");
  Serial.println("[Register] Received: " + body);
  
  // Parse JSON manuell: {"name":"esp1","ip":"..."}
  int nameStart = body.indexOf("\"name\":\"");
  if (nameStart < 0) {
    server.send(400, "application/json", "{\"error\":\"No name field\"}");
    return;
  }
  
  nameStart += 8;  // Länge von "name":"
  int nameEnd = body.indexOf("\"", nameStart);
  String name = body.substring(nameStart, nameEnd);
  
  // IP aus Request ermitteln
  String clientIP = server.client().remoteIP().toString();
  
  Serial.println("[Register] Name: '" + name + "', IP: " + clientIP);
  
  registerClient(name, clientIP);
  
  server.send(200, "application/json", "{\"success\":true}");
}

// Hilfsfunktion: Extrahiert JSON-String-Wert
String extractJsonString(String json, String key) {
  String search = "\"" + key + "\":";
  int pos = json.indexOf(search);
  if (pos < 0) {
    Serial.println("[JSON] Key '" + key + "' not found in: " + json);
    return "";
  }
  
  // Finde den Start des Wertes (nach dem ':')
  int valueStart = pos + search.length();
  
  // Überspringe Leerzeichen
  while (valueStart < json.length() && json.charAt(valueStart) == ' ') {
    valueStart++;
  }
  
  // Prüfe ob es ein String ist (beginnt mit ")
  if (valueStart >= json.length() || json.charAt(valueStart) != '"') {
    Serial.println("[JSON] Value for '" + key + "' is not a string");
    return "";
  }
  
  // Überspringe das öffnende "
  valueStart++;
  
  // Finde das schließende " (beachte escaped quotes)
  int valueEnd = valueStart;
  while (valueEnd < json.length()) {
    if (json.charAt(valueEnd) == '"' && (valueEnd == 0 || json.charAt(valueEnd - 1) != '\\')) {
      break;
    }
    valueEnd++;
  }
  
  String value = json.substring(valueStart, valueEnd);
  Serial.println("[JSON] Extracted '" + key + "' = '" + value + "'");
  return value;
}

// POST /forward - Request zu Client weiterleiten
void handleForward() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }
  
  String body = server.arg("plain");
  Serial.println("[Forward] Request body: " + body);
  Serial.println("[Forward] Body length: " + String(body.length()));
  
  // Parse JSON mit verbesserter Funktion
  String target = extractJsonString(body, "target");
  String method = extractJsonString(body, "method");
  String path = extractJsonString(body, "path");
  String reqBody = extractJsonString(body, "body");
  
  Serial.println("[Forward] Parsed - target: '" + target + "', method: '" + method + "', path: '" + path + "'");
  
  if (target.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"No target specified\"}");
    return;
  }
  
  String response = forwardRequest(target, method, path, reqBody);
  server.send(200, "application/json", response);
}

// GET /scenario?name=X&state=Y - Szenario ausführen
void handleScenario() {
  if (!server.hasArg("name") || !server.hasArg("state")) {
    server.send(400, "application/json", 
      "{\"error\":\"Missing parameters. Usage: /scenario?name=<n>&state=<state>\"}");
    return;
  }
  
  String name = server.arg("name");
  int state = server.arg("state").toInt();
  
  Serial.println("====================================");
  Serial.println("[Scenario] Request: " + name + " | State: " + String(state));
  Serial.println("====================================");
  
  if (name == "kohlekraftwerk") {
    scenario_kohlekraftwerk(state);
    server.send(200, "application/json", 
      "{\"success\":true,\"scenario\":\"kohlekraftwerk\",\"state\":" + String(state) + "}");
    
  } else if (name == "test") {
    scenario_test(state);
    server.send(200, "application/json", 
      "{\"success\":true,\"scenario\":\"test\",\"state\":" + String(state) + "}");
    
  } else if (name == "alles") {
    scenario_alles(state);
    server.send(200, "application/json", 
      "{\"success\":true,\"scenario\":\"alles\",\"state\":" + String(state) + "}");
    
  } else {
    server.send(404, "application/json", 
      "{\"error\":\"Unknown scenario: " + name + "\"}");
  }
  
  Serial.println("[Scenario] Request completed");
}

// GET / - Info Page
void handleRoot() {
  String html = "<html><body><h1>ESP-Host (ESP32)</h1>";
  html += "<p>Access Point: " + String(AP_SSID) + "</p>";
  html += "<p>IP: " + WiFi.softAPIP().toString() + "</p>";
  html += "<p>Connected WiFi Devices: " + String(WiFi.softAPgetStationNum()) + "</p>";
  html += "<h2>Registered Clients:</h2><ul>";
  
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].name.length() > 0) {
      html += "<li>" + clients[i].name + " @ " + clients[i].ip;
      unsigned long age = (millis() - clients[i].lastSeen) / 1000;
      html += " (last seen " + String(age) + "s ago)</li>";
    }
  }
  
  html += "</ul></body></html>";
  server.send(200, "text/html", html);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP-Host (ESP32) starting ===");
  
  // WiFi Access Point starten
  WiFi.mode(WIFI_AP);
  // Allow up to 8 connections (default is 4 on some ESP32 cores)
  // Parameters: SSID, Password, Channel, Hidden, Max_Connections
  WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 8);
  WiFi.setSleep(false); // Verhindert Latenz-Spitzen und verbessert Erreichbarkeit
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Web Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/clients", HTTP_GET, handleClients);
  server.on("/register", HTTP_POST, handleRegister);
  server.on("/forward", HTTP_POST, handleForward);
  server.on("/scenario", HTTP_GET, handleScenario);
  
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("=== ESP-Host ready ===\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  server.handleClient();
  
  // Überwachung der verbundenen WiFi-Geräte
  int currentStationCount = WiFi.softAPgetStationNum();
  if (currentStationCount != lastStationCount) {
    Serial.printf("[INFO] Anzahl verbundener WiFi-Geräte: %d\n", currentStationCount);
    lastStationCount = currentStationCount;
  }
  
  // Client-Timeout prüfen (2 Minuten Inaktivität)
  unsigned long now = millis();
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].name.length() > 0 && now - clients[i].lastSeen > 120000) {
      Serial.printf("[TIMEOUT] Client '%s' entfernt (keine Aktivität seit 2 Minuten)\n", 
        clients[i].name.c_str());
      clients[i].name = ""; 
      clients[i].ip = ""; 
      clients[i].lastSeen = 0;
    }
  }
  
  delay(10);
}