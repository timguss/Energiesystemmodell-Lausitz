/*
 * ESP-Host mit Scenario-Support
 *
 * Funktionen:
 * - WiFi Access Point
 * - Web Server mit /forward Endpoint
 * - Client Management (esp1, esp2, esp3)
 * - Scenario Execution (/scenario?name=X&state=Y)
 *
 * Hardware: ESP8266 (z.B. NodeMCU, Wemos D1 Mini)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

// WiFi Access Point
const char *AP_SSID = "ESP-Host";
const char *AP_PASSWORD = "12345678";

// Web Server
ESP8266WebServer server(80);

// Client ESP URLs (werden automatisch registriert)
#define MAX_CLIENTS 10

struct ClientDevice
{
  String name;
  String ip;
  unsigned long lastSeen;
};

ClientDevice clients[MAX_CLIENTS];
int clientCount = 0;

// Monitoring
int lastStationCount = 0;

// HTTP Client für Forward-Requests
WiFiClient wifiClient;
HTTPClient http;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Findet Client-IP anhand des Namens
String getClientIP(String name)
{
  for (int i = 0; i < clientCount; i++)
  {
    if (clients[i].name == name)
    {
      return clients[i].ip;
    }
  }
  return "";
}

// Registriert oder aktualisiert einen Client
void registerClient(String name, String ip)
{
  // Prüfe ob Client bereits existiert
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].name == name)
    {
      clients[i].ip = ip;
      clients[i].lastSeen = millis();
      Serial.println("[Client] Updated: " + name + " @ " + ip);
      return;
    }
  }

  // Neuer Client - finde freien Slot
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].name.length() == 0)
    {
      clients[i].name = name;
      clients[i].ip = ip;
      clients[i].lastSeen = millis();
      clientCount++;
      Serial.println("[Client] Registered: " + name + " @ " + ip);
      return;
    }
  }

  Serial.println("[Client] ERROR: Max clients reached!");
}

// Forward Request zu einem Client-ESP
String forwardRequest(String target, String method, String path, String body)
{
  String ip = getClientIP(target);
  if (ip == "")
  {
    return "{\"error\":\"Client not found\"}";
  }

  String url = "http://" + ip + path;
  Serial.println("[Forward] " + method + " " + url);

  http.begin(wifiClient, url);
  http.setTimeout(5000);

  int httpCode = -1;
  String payload = "";

  if (method == "GET")
  {
    httpCode = http.GET();
  }
  else if (method == "POST")
  {
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(body);
  }

  if (httpCode > 0)
  {
    payload = http.getString();
  }
  else
  {
    payload = "{\"error\":\"HTTP error " + String(httpCode) + "\"}";
  }

  http.end();

  // Return als JSON mit code und body
  String response = "{\"code\":" + String(httpCode) + ",\"body\":\"";
  // Escape quotes in payload
  payload.replace("\"", "\\\"");
  payload.replace("\n", "\\n");
  response += payload + "\"}";

  return response;
}

// Delay mit Yield (verhindert Watchdog Reset)
void delayWithYield(unsigned long ms)
{
  unsigned long start = millis();
  while (millis() - start < ms)
  {
    yield();
    delay(1);
  }
}

// Schaltet ein Relay über Forward-Request
void setRelayByForward(String device, int idx, int val)
{
  String path = "/set?idx=" + String(idx) + "&val=" + String(val);
  String response = forwardRequest(device, "GET", path, "");
  Serial.println("[Relay] " + device + " idx=" + String(idx) + " val=" + String(val));
}

// ============================================================================
// SCENARIO DEFINITIONS
// ============================================================================

void scenario_kohlekraftwerk(int state)
{
  if (state == 0)
  {
    // === KOHLEKRAFTWERK AUS ===
    Serial.println("[Scenario] Kohlekraftwerk AUS");

    setRelayByForward("esp1", 0, 0); // Global Relay 1 aus
    delay(50);
    setRelayByForward("esp1", 1, 0); // Global Relay 2 aus
    delay(50);
    setRelayByForward("esp1", 4, 0); // Global Relay 5 aus
    delay(50);
    setRelayByForward("esp2", 0, 0); // Global Relay 9 aus

    Serial.println("[Scenario] Kohlekraftwerk AUS - Fertig");
  }
  else if (state == 1)
  {
    // === KOHLEKRAFTWERK EIN ===
    Serial.println("[Scenario] Kohlekraftwerk EIN - Start");

    // Schritt 1: Mehrere Relays einschalten
    setRelayByForward("esp1", 0, 1); // Global Relay 1 ein
    delay(50);
    setRelayByForward("esp1", 1, 1); // Global Relay 2 ein
    delay(50);
    setRelayByForward("esp1", 4, 1); // Global Relay 5 ein
    delay(50);
    setRelayByForward("esp2", 0, 1); // Global Relay 9 ein

    // Schritt 2: 5 Sekunden warten
    Serial.println("[Scenario] Warte 5 Sekunden...");
    delayWithYield(5000);

    // Schritt 3: Relay 2 wieder ausschalten
    Serial.println("[Scenario] Relay 2 wird ausgeschaltet");
    setRelayByForward("esp1", 1, 0); // Global Relay 2 aus

    Serial.println("[Scenario] Kohlekraftwerk EIN - Fertig");
  }
}

void scenario_test(int state)
{
  switch (state)
  {
  case 0:
    Serial.println("[Scenario] Test Reset");
    for (int i = 0; i < 8; i++)
    {
      setRelayByForward("esp1", i, 0);
      delay(50);
    }
    for (int i = 0; i < 4; i++)
    {
      setRelayByForward("esp2", i, 0);
      delay(50);
    }
    break;

  case 1:
    Serial.println("[Scenario] Test Sequenz 1");
    setRelayByForward("esp1", 0, 1);
    delayWithYield(2000);
    setRelayByForward("esp1", 1, 1);
    delayWithYield(2000);
    setRelayByForward("esp1", 2, 1);
    delayWithYield(2000);
    setRelayByForward("esp1", 0, 0);
    delayWithYield(1000);
    setRelayByForward("esp1", 1, 0);
    delayWithYield(1000);
    setRelayByForward("esp1", 2, 0);
    break;

  case 2:
    Serial.println("[Scenario] Test Sequenz 2");
    setRelayByForward("esp2", 0, 1);
    delayWithYield(1000);
    setRelayByForward("esp2", 1, 1);
    delayWithYield(1000);
    setRelayByForward("esp2", 0, 0);
    delayWithYield(1000);
    setRelayByForward("esp2", 1, 0);
    break;
  }
}

void scenario_alles(int state)
{
  if (state == 0)
  {
    Serial.println("[Scenario] Alle Relays AUS");
    for (int i = 0; i < 8; i++)
    {
      setRelayByForward("esp1", i, 0);
      delay(50);
    }
    for (int i = 0; i < 4; i++)
    {
      setRelayByForward("esp2", i, 0);
      delay(50);
    }
  }
  else if (state == 1)
  {
    Serial.println("[Scenario] Alle Relays AN");
    for (int i = 0; i < 8; i++)
    {
      setRelayByForward("esp1", i, 1);
      delay(50);
    }
    for (int i = 0; i < 4; i++)
    {
      setRelayByForward("esp2", i, 1);
      delay(50);
    }
  }
}

// ============================================================================
// WEB SERVER HANDLERS
// ============================================================================

// GET /clients - Liste aller registrierten Clients
void handleClients()
{
  String json = "{\"clients\":[";
  bool first = true;

  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].name.length() > 0)
    {
      if (!first)
        json += ",";
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
void handleRegister()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }

  String body = server.arg("plain");
  // Erwarte: {"name":"esp1"}

  int nameStart = body.indexOf("\"name\":\"") + 8;
  int nameEnd = body.indexOf("\"", nameStart);
  String name = body.substring(nameStart, nameEnd);

  String clientIP = server.client().remoteIP().toString();
  registerClient(name, clientIP);

  server.send(200, "application/json", "{\"success\":true}");
}

// POST /forward - Request zu Client weiterleiten
void handleForward()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }

  String body = server.arg("plain");

  // Parse JSON manually (simple parser)
  String target = "";
  String method = "";
  String path = "";
  String reqBody = "";

  int pos = body.indexOf("\"target\":\"");
  if (pos >= 0)
  {
    int start = pos + 10;
    int end = body.indexOf("\"", start);
    target = body.substring(start, end);
  }

  pos = body.indexOf("\"method\":\"");
  if (pos >= 0)
  {
    int start = pos + 10;
    int end = body.indexOf("\"", start);
    method = body.substring(start, end);
  }

  pos = body.indexOf("\"path\":\"");
  if (pos >= 0)
  {
    int start = pos + 8;
    int end = body.indexOf("\"", start);
    path = body.substring(start, end);
  }

  pos = body.indexOf("\"body\":\"");
  if (pos >= 0)
  {
    int start = pos + 8;
    int end = body.lastIndexOf("\"");
    reqBody = body.substring(start, end);
  }

  String response = forwardRequest(target, method, path, reqBody);
  server.send(200, "application/json", response);
}

// GET /scenario?name=X&state=Y - Szenario ausführen
void handleScenario()
{
  if (!server.hasArg("name") || !server.hasArg("state"))
  {
    server.send(400, "application/json",
                "{\"error\":\"Missing parameters. Usage: /scenario?name=<n>&state=<state>\"}");
    return;
  }

  String name = server.arg("name");
  int state = server.arg("state").toInt();

  Serial.println("====================================");
  Serial.println("[Scenario] Request: " + name + " | State: " + String(state));
  Serial.println("====================================");

  if (name == "kohlekraftwerk")
  {
    scenario_kohlekraftwerk(state);
    server.send(200, "application/json",
                "{\"success\":true,\"scenario\":\"kohlekraftwerk\",\"state\":" + String(state) + "}");
  }
  else if (name == "test")
  {
    scenario_test(state);
    server.send(200, "application/json",
                "{\"success\":true,\"scenario\":\"test\",\"state\":" + String(state) + "}");
  }
  else if (name == "alles")
  {
    scenario_alles(state);
    server.send(200, "application/json",
                "{\"success\":true,\"scenario\":\"alles\",\"state\":" + String(state) + "}");
  }
  else
  {
    server.send(404, "application/json",
                "{\"error\":\"Unknown scenario: " + name + "\"}");
  }

  Serial.println("[Scenario] Request completed");
}

// GET / - Info Page
void handleRoot()
{
  String html = "<html><body><h1>ESP-Host</h1>";
  html += "<p>Access Point: " + String(AP_SSID) + "</p>";
  html += "<p>IP: " + WiFi.softAPIP().toString() + "</p>";
  html += "<p>Connected WiFi Devices: " + String(WiFi.softAPgetStationNum()) + "</p>";
  html += "<h2>Registered Clients:</h2><ul>";

  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].name.length() > 0)
    {
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

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\n=== ESP-Host starting ===");

  // WiFi Access Point starten
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

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

void loop()
{
  server.handleClient();

  // Überwachung der verbundenen WiFi-Geräte
  int currentStationCount = WiFi.softAPgetStationNum();
  if (currentStationCount != lastStationCount)
  {
    Serial.printf("[INFO] Anzahl verbundener WiFi-Geräte: %d\n", currentStationCount);
    lastStationCount = currentStationCount;
  }

  // Client-Timeout prüfen (2 Minuten Inaktivität)
  unsigned long now = millis();
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].name.length() > 0 && now - clients[i].lastSeen > 120000)
    {
      Serial.printf("[TIMEOUT] Client '%s' entfernt (keine Aktivität seit 2 Minuten)\n",
                    clients[i].name.c_str());
      clients[i].name = "";
      clients[i].ip = "";
      clients[i].lastSeen = 0;
      clientCount--;
    }
  }

  delay(10); // Kleine Pause für Stabilität
  yield();
}