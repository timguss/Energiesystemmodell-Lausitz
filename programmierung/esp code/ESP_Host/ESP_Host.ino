// ESPHost.ino
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

WebServer server(80);

struct ClientInfo { String name; String ip; unsigned long lastSeen; };
#define MAX_CLIENTS 12
ClientInfo clients[MAX_CLIENTS];

const char* AP_SSID = "ESP-HOST";
const char* AP_PASS = "espHostPass";

// Tracking für verbundene WiFi-Clients
int lastStationCount = 0;

void addOrUpdateClient(const String &name, const String &ip){
  bool isNew = true;
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name == name){
      clients[i].ip = ip; 
      clients[i].lastSeen = millis();
      Serial.printf("[UPDATE] Client '%s' aktualisiert - IP: %s\n", name.c_str(), ip.c_str());
      isNew = false;
      return;
    }
  }
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name.length()==0){
      clients[i].name = name; 
      clients[i].ip = ip; 
      clients[i].lastSeen = millis();
      Serial.printf("[NEU] Client '%s' registriert - IP: %s\n", name.c_str(), ip.c_str());
      return;
    }
  }
  Serial.println("[WARNUNG] Maximale Client-Anzahl erreicht!");
}

String listClientsJSON(){
  String s = "[";
  bool first=true;
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name.length()){
      if(!first) s += ",";
      s += "{\"name\":\""+clients[i].name+"\",\"ip\":\""+clients[i].ip+"\"}";
      first=false;
    }
  }
  s += "]";
  return s;
}

void handleRegister(){
  Serial.printf("[HTTP] POST /register von %s\n", server.client().remoteIP().toString().c_str());
  
  if(server.method() != HTTP_POST){ 
    Serial.println("[FEHLER] Falsche Methode für /register");
    server.send(405,"text/plain","method"); 
    return; 
  }
  
  String body = server.arg("plain");
  Serial.printf("[DATA] Body: %s\n", body.c_str());
  
  int nidx = body.indexOf("\"name\"");
  int iidx = body.indexOf("\"ip\"");
  String name="", ip="";
  
  if(nidx>=0){
    int q1 = body.indexOf('"', nidx+6);
    int q2 = body.indexOf('"', q1+1);
    if(q1>=0 && q2>q1) name = body.substring(q1+1,q2);
  }
  if(iidx>=0){
    int q1 = body.indexOf('"', iidx+4);
    int q2 = body.indexOf('"', q1+1);
    if(q1>=0 && q2>q1) ip = body.substring(q1+1,q2);
  }
  
  if(name.length()==0) { 
    Serial.println("[FEHLER] Name fehlt in Registrierung");
    server.send(400,"text/plain","missing name"); 
    return; 
  }
  if(ip.length()==0) ip = server.client().remoteIP().toString();
  
  addOrUpdateClient(name, ip);
  server.send(200,"application/json","{\"status\":\"ok\"}");
}

void handleClients(){ 
  Serial.printf("[HTTP] GET /clients von %s\n", server.client().remoteIP().toString().c_str());
  String json = listClientsJSON();
  Serial.printf("[RESPONSE] Clients-Liste: %s\n", json.c_str());
  server.send(200,"application/json", json); 
}

// Ersetze vorhandene handleForward() mit diesem robusten Parser
void handleForward(){
  Serial.printf("[HTTP] POST /forward von %s\n", server.client().remoteIP().toString().c_str());

  if(server.method() != HTTP_POST){
    Serial.println("[FEHLER] Falsche Methode für /forward");
    server.send(405,"text/plain","method");
    return;
  }

  String b = server.arg("plain");
  Serial.printf("[DATA] Forward Body: %s\n", b.c_str());

  // Hilfsfunktion: extrahiere String/Objekt/Token nach key
  auto extractValue = [&](const char* key)->String{
    int idx = b.indexOf(String("\"") + String(key) + String("\""));
    if(idx < 0) return "";
    int colon = b.indexOf(":", idx);
    if(colon < 0) return "";

    // skip spaces
    int p = colon + 1;
    while(p < (int)b.length() && isspace(b[p])) p++;

    if(p >= (int)b.length()) return "";

    char c = b[p];
    if(c == '"'){
      // quoted string, handle escapes
      int i = p + 1;
      String out = "";
      while(i < (int)b.length()){
        char ch = b[i++];
        if(ch == '\\' && i < (int)b.length()){
          char next = b[i++];
          if(next == 'n') out += '\n';
          else if(next == 'r') out += '\r';
          else if(next == 't') out += '\t';
          else out += next;
        } else if(ch == '"'){
          return out;
        } else {
          out += ch;
        }
      }
      return out; // unterbrochen, aber gib was wir haben
    } else if(c == '{' || c == '['){
      // balanced JSON object/array
      char open = c;
      char close = (c == '{') ? '}' : ']';
      int depth = 0;
      int i = p;
      for(; i < (int)b.length(); ++i){
        if(b[i] == open) depth++;
        else if(b[i] == close){
          depth--;
          if(depth == 0){
            // substring from p .. i inclusive
            return b.substring(p, i+1);
          }
        } else if(b[i] == '"' ){ // skip strings inside to avoid braces in strings
          i++;
          while(i < (int)b.length()){
            if(b[i] == '\\' && i+1 < (int)b.length()) i += 2;
            else if(b[i] == '"') { i++; break; }
            else i++;
          }
          i--; // adjust for loop increment
        }
      }
      // if we get here, return rest
      return b.substring(p);
    } else {
      // unquoted token until comma or end/}
      int i = p;
      while(i < (int)b.length() && b[i] != ',' && b[i] != '}' && b[i] != ']') i++;
      String tok = b.substring(p, i);
      tok.trim();
      return tok;
    }
  };

  String target = extractValue("target");
  String method = extractValue("method");
  String path = extractValue("path");
  String body = extractValue("body");

  if(target.length()==0 || method.length()==0 || path.length()==0){
    Serial.println("[FEHLER] Fehlende Parameter in /forward");
    server.send(400,"text/plain","missing args");
    return;
  }

  String targetIp="";
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name==target){ targetIp = clients[i].ip; break; }
  }

  if(targetIp.length()==0){
    Serial.printf("[FEHLER] Target '%s' nicht registriert\n", target.c_str());
    server.send(404,"text/plain","target not registered");
    return;
  }

  String url = "http://"+targetIp+path;
  Serial.printf("[FORWARD] %s zu %s (%s)\n", method.c_str(), target.c_str(), url.c_str());

  HTTPClient http;
  http.begin(url);
  int code = 0;
  String resp="";

  // wenn body ein JSON-Objekt/Array beginnt, setze Content-Type auf application/json
  bool bodyIsJson = body.length()>0 && (body.charAt(0) == '{' || body.charAt(0) == '[');

  if(method == "GET"){
    code = http.GET();
    if(code > 0) resp = http.getString();
  } else if(method == "POST"){
    if(bodyIsJson) http.addHeader("Content-Type","application/json");
    else http.addHeader("Content-Type","text/plain");
    code = http.POST(body);
    if(code > 0) resp = http.getString();
  } else {
    Serial.printf("[FEHLER] Nicht unterstützte Methode: %s\n", method.c_str());
    server.send(400,"text/plain","unsupported method");
    http.end();
    return;
  }

  Serial.printf("[RESPONSE] Code: %d, Body: %s\n", code, resp.c_str());

  String out = "{\"code\":"+String(code)+",\"body\":\"";
  for(size_t i=0;i<resp.length();++i){
    char c = resp[i];
    if(c=='\\') out += "\\\\";
    else if(c=='"') out += "\\\"";
    else if(c=='\n') out += "\\n";
    else if(c=='\r') out += "\\r";
    else out += c;
  }
  out += "\"}";
  http.end();
  server.send(200,"application/json",out);
}


// WiFi Event Handler für AP-Verbindungen
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){
  switch(event){
    case ARDUINO_EVENT_WIFI_AP_START:
      Serial.println("\n[AP] Access Point gestartet!");
      Serial.printf("[AP] SSID: %s\n", AP_SSID);
      Serial.printf("[AP] IP: %s\n", WiFi.softAPIP().toString().c_str());
      Serial.println("[AP] Warte auf Client-Verbindungen...\n");
      break;
      
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf("\n[WIFI] Neues Gerät verbunden!\n");
      Serial.printf("[WIFI] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
        info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
        info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
      Serial.printf("[WIFI] Verbundene Geräte: %d\n\n", WiFi.softAPgetStationNum());
      break;
      
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.printf("\n[WIFI] Gerät getrennt!\n");
      Serial.printf("[WIFI] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
        info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
        info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);
      Serial.printf("[WIFI] Verbundene Geräte: %d\n\n", WiFi.softAPgetStationNum());
      break;
      
    default:
      break;
  }
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("    ESP HOST - Starte System...");
  Serial.println("========================================\n");
  
  for(int i=0;i<MAX_CLIENTS;i++){ 
    clients[i].name=""; 
    clients[i].ip=""; 
    clients[i].lastSeen=0; 
  }

  // WiFi Event Handler registrieren
  WiFi.onEvent(onWiFiEvent);
  
  IPAddress apIP(192,168,4,1);
  IPAddress netM(255,255,255,0);
  WiFi.softAPConfig(apIP, apIP, netM);
  
  Serial.println("[SETUP] Starte Access Point...");
  WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  
  delay(500);
  
  Serial.printf("\n[OK] AP gestartet!\n");
  Serial.printf("[OK] SSID: %s\n", AP_SSID);
  Serial.printf("[OK] Passwort: %s\n", AP_PASS);
  Serial.printf("[OK] IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("[OK] Max Clients: 4\n\n");

  server.on("/register", HTTP_POST, handleRegister);
  server.on("/clients", HTTP_GET, handleClients);
  server.on("/forward", HTTP_POST, handleForward);
  server.begin();
  
  Serial.println("[OK] HTTP Server gestartet!");
  Serial.println("\nEndpoints:");
  Serial.println("  POST /register - Client registrieren");
  Serial.println("  GET  /clients  - Liste aller Clients");
  Serial.println("  POST /forward  - Request weiterleiten");
  Serial.println("\n========================================");
  Serial.println("       System bereit!");
  Serial.println("========================================\n");
}

void loop(){
  server.handleClient();
  
  // Überwachung der verbundenen WiFi-Geräte
  int currentStationCount = WiFi.softAPgetStationNum();
  if(currentStationCount != lastStationCount){
    Serial.printf("[INFO] Anzahl verbundener WiFi-Geräte: %d\n", currentStationCount);
    lastStationCount = currentStationCount;
  }
  
  // Client-Timeout prüfen
  unsigned long now = millis();
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name.length() && now - clients[i].lastSeen > 120000){
      Serial.printf("[TIMEOUT] Client '%s' entfernt (keine Aktivität seit 2 Minuten)\n", 
        clients[i].name.c_str());
      clients[i].name=""; 
      clients[i].ip=""; 
      clients[i].lastSeen=0;
    }
  }
  
  delay(10); // Kleine Pause für Stabilität
}