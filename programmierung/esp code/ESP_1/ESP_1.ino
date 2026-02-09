// ESP1_api.ino - Komplett mit RS232 Fix
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <math.h>

const char* WIFI_SSID = "ESP-HOST";
const char* WIFI_PASS = "espHostPass";

const int UART2_RX_PIN = 16;
const int UART2_TX_PIN = 17;
const unsigned long DEFAULT_REPLY_TIMEOUT = 500;
HardwareSerial MySerial(2);
bool serial2Started = false;

const bool ACTIVE_LOW = true;
struct RelayConfig { uint8_t pin; uint8_t relayNum; const char* title; };
const uint8_t RELAY_COUNT = 8;
RelayConfig RELAYS[RELAY_COUNT] = {
  {19,1,"Ventil - 1"},{21,2,"Ventil - 2"},{22,3,"Heizstab"},{23,4,"Zünder"},
  {32,5,"Gasventil"},{33,6,"Kühler"},{25,7,"MFC - fehlt noch"},{26,8,"unbelegt"}
};

const int adcPin = 34;
const float Vcc = 3.3, Rf = 10000.0, R0 = 10000.0, T0_K = 298.15, beta = 3950.0;
float cachedTempC = NAN, cachedRntc = NAN;
unsigned long lastTempMillis = 0;
const unsigned long TEMP_INTERVAL = 1000;
bool rs232Enabled = true;

WebServer server(80);

inline int physToLogical(int physVal){ return ACTIVE_LOW ? (physVal==LOW?1:0) : (physVal==HIGH?1:0); }
inline int logicalToPhys(int logical){ return ACTIVE_LOW ? (logical==1?LOW:HIGH) : (logical==1?HIGH:LOW); }

float readVoltage(){ int adc=analogRead(adcPin); return (float)adc/4095.0*Vcc; }
float readR_NTC(){ float v=readVoltage(); if(v<=0.0001) return 1e9; if(v>=Vcc-0.0001) return 1e-6; return Rf*(v/(Vcc-v)); }
float ntcToCelsius(float Rntc){ float invT=(1.0/T0_K)+(1.0/beta)*log(Rntc/R0); return 1.0/invT - 273.15; }

String getRelayStateJSON(){
  String s="{";
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    s += "\"r"+String(i)+"\":"+String(physToLogical(digitalRead(RELAYS[i].pin)));
    if(i<RELAY_COUNT-1) s += ",";
  }
  s += "}";
  return s;
}

void handleState(){ 
  String s="{";
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    s += "\"r"+String(i)+"\":"+String(physToLogical(digitalRead(RELAYS[i].pin)));
    s += ",";
  }
  // Füge Temperaturdaten hinzu
  s += "\"temp\":" + (isnan(cachedTempC)?"null":String(cachedTempC,2)) + ",";
  s += "\"rntc\":" + (isnan(cachedRntc)?"null":String(cachedRntc,0));
  s += "}";
  server.send(200,"application/json",s); 
}

void handleSet(){
  if(!server.hasArg("idx")||!server.hasArg("val")){ server.send(400,"text/plain","missing args"); return; }
  int idx = server.arg("idx").toInt(); int val = server.arg("val").toInt();
  if(idx<0 || idx>=RELAY_COUNT || (val!=0 && val!=1)){ server.send(400,"text/plain","invalid args"); return; }
  digitalWrite(RELAYS[idx].pin, logicalToPhys(val));
  server.send(200,"text/plain","ok");
}

void handleTemp(){
  String j = "{\"temp\":" + (isnan(cachedTempC)?"null":String(cachedTempC,2)) +
             ",\"rntc\":" + (isnan(cachedRntc)?"null":String(cachedRntc,0)) + "}";
  server.send(200,"application/json", j);
}

void handleSend(){
  if(!rs232Enabled){ 
    server.send(400,"text/plain","RS232 deaktiviert"); 
    return; 
  }
  if(!serial2Started){ 
    server.send(500,"text/plain","Serial2 nicht initialisiert."); 
    return; 
  }
  if(!server.hasArg("plain")){ 
    server.send(400,"text/plain","Kein Body"); 
    return; 
  }

  String body = server.arg("plain");
  Serial.println("[RS232] Received body: " + body);
  
  String cmd = ""; 
  unsigned long timeout = DEFAULT_REPLY_TIMEOUT;
  
  // Suche nach "cmd" (kann escaped sein als \"cmd\" oder normal "cmd")
  int cmdKeyPos = body.indexOf("\"cmd\"");
  if(cmdKeyPos < 0) {
    // Versuche escaped Version
    cmdKeyPos = body.indexOf("\\\"cmd\\\"");
  }
  
  if(cmdKeyPos >= 0) {
    // Finde den Doppelpunkt nach "cmd"
    int colonPos = body.indexOf(":", cmdKeyPos + 5);
    if(colonPos < 0 && cmdKeyPos >= 0) {
      // Versuche nach escaped Version zu suchen
      colonPos = body.indexOf(":", cmdKeyPos + 7); // 7 für \"cmd\"
    }
    
    if(colonPos >= 0) {
      // Überspringe Leerzeichen nach dem Doppelpunkt
      int valueStart = colonPos + 1;
      while(valueStart < body.length() && body.charAt(valueStart) == ' ') {
        valueStart++;
      }
      
      // Prüfe ob es ein String ist (beginnt mit " oder \")
      bool isEscaped = false;
      if(valueStart < body.length() && body.charAt(valueStart) == '\\' && 
         valueStart + 1 < body.length() && body.charAt(valueStart + 1) == '"') {
        // Escaped quote: \"
        valueStart += 2; // Überspringe \"
        isEscaped = true;
      } else if(valueStart < body.length() && body.charAt(valueStart) == '"') {
        // Normal quote: "
        valueStart++; // Überspringe "
        isEscaped = false;
      }
      
      if(valueStart < body.length()) {
        // Finde schließendes " (beachte escaped quotes)
        int valueEnd = valueStart;
        while(valueEnd < body.length()) {
          if(isEscaped) {
            // Suche nach \" (escaped quote)
            if(valueEnd + 1 < body.length() && 
               body.charAt(valueEnd) == '\\' && 
               body.charAt(valueEnd + 1) == '"') {
              // Gefunden: \"
              break;
            }
            valueEnd++;
          } else {
            // Suche nach " (nicht escaped)
            if(body.charAt(valueEnd) == '"' && 
               (valueEnd == 0 || body.charAt(valueEnd - 1) != '\\')) {
              break;
            }
            valueEnd++;
          }
        }
        
        if(valueEnd > valueStart) {
          cmd = body.substring(valueStart, valueEnd);
          Serial.println("[RS232] Extracted cmd: '" + cmd + "'");
        }
      }
    }
  }
  
  // Parse timeout - ähnlich wie cmd
  int timeoutKeyPos = body.indexOf("\"timeout\"");
  if(timeoutKeyPos < 0) {
    timeoutKeyPos = body.indexOf("\\\"timeout\\\"");
  }
  
  if(timeoutKeyPos >= 0) {
    int colonPos = body.indexOf(":", timeoutKeyPos + 9);
    if(colonPos < 0) {
      colonPos = body.indexOf(":", timeoutKeyPos + 11); // 11 für \"timeout\"
    }
    
    if(colonPos >= 0) {
      int valueStart = colonPos + 1;
      while(valueStart < body.length() && (body.charAt(valueStart) == ' ' || body.charAt(valueStart) == '\t')) {
        valueStart++;
      }
      
      // Finde Ende des Zahlenwerts (Komma oder })
      int valueEnd = valueStart;
      while(valueEnd < body.length() && body.charAt(valueEnd) != ',' && body.charAt(valueEnd) != '}') {
        valueEnd++;
      }
      
      if(valueEnd > valueStart) {
        String tstr = body.substring(valueStart, valueEnd);
        tstr.trim();
        timeout = tstr.toInt();
        Serial.println("[RS232] Extracted timeout: " + String(timeout));
      }
    }
  }
  
  if(cmd.length()==0){ 
    Serial.println("[RS232] ERROR: Kein cmd gefunden in body: " + body);
    server.send(400,"text/plain","Kein cmd gefunden"); 
    return; 
  }
  
  // WICHTIG: Konvertiere Escape-Sequenzen in echte Steuerzeichen
  // Ersetze \r und \n (als Text) durch echte Carriage Return und Line Feed
  cmd.replace("\\r", "\r");
  cmd.replace("\\n", "\n");
  cmd.replace("\\t", "\t");
  
  // Stelle sicher dass cmd mit \r\n endet (ProPar Protokoll!)
  if(!cmd.endsWith("\r\n")) {
    // Prüfe ob nur \r oder nur \n am Ende
    if(cmd.endsWith("\r") || cmd.endsWith("\n")){
      // Entferne einzelnes Zeichen und füge beides hinzu
      cmd = cmd.substring(0, cmd.length()-1);
    }
    cmd += "\r\n";
  }
  
  // Debug-Ausgabe
  Serial.print("RS232 TX: ");
  for(size_t i=0; i<cmd.length(); i++){
    if(cmd[i] == '\r') Serial.print("\\r");
    else if(cmd[i] == '\n') Serial.print("\\n");
    else Serial.print(cmd[i]);
  }
  Serial.println();
  
  // WICHTIG: Leere den Serial-Buffer vor dem Senden
  while(MySerial.available()) {
    MySerial.read(); // Entferne alte Daten
  }
  
  // Sende Befehl an MFC
  MySerial.print(cmd);
  MySerial.flush(); // Stelle sicher, dass alles gesendet wurde
  
  // Kleine Pause damit das Gerät Zeit hat zu antworten
  delay(50);
  
  // Warte auf Antwort
  String resp = "";
  unsigned long start = millis();
  bool responseComplete = false;
  
  while(millis() - start < timeout && !responseComplete){
    while(MySerial.available()){
      char c = MySerial.read();
      resp += c;
      
      // ProPar Antwort endet mit \r\n
      if(resp.endsWith("\r\n")){
        responseComplete = true;
        break;
      }
    }
    
    if(!responseComplete) {
      delay(10); // Kleine Pause um CPU zu entlasten
    }
  }
  
  if(responseComplete){
    // Vollständige Antwort erhalten
    Serial.print("RS232 RX: ");
    for(size_t i=0; i<resp.length(); i++){
      if(resp[i] == '\r') Serial.print("\\r");
      else if(resp[i] == '\n') Serial.print("\\n");
      else if(resp[i] == '\t') Serial.print("\\t");
      else Serial.print(resp[i]);
    }
    Serial.println();
    
    server.send(200, "text/plain", resp);
  } else if(resp.length() > 0){
    // Timeout aber teilweise Antwort erhalten
    Serial.print("RS232 RX (Timeout, partial): ");
    for(size_t i=0; i<resp.length(); i++){
      if(resp[i] == '\r') Serial.print("\\r");
      else if(resp[i] == '\n') Serial.print("\\n");
      else if(resp[i] == '\t') Serial.print("\\t");
      else Serial.print(resp[i]);
    }
    Serial.println();
    
    // Clear buffer after timeout to prevent garbage affecting next command
    while(MySerial.available()) {
      MySerial.read();
    }
    
    server.send(200, "text/plain", resp);
  } else {
    Serial.println("RS232 RX: (keine Antwort)");
    
    // Clear buffer after no response to prevent garbage affecting next command
    while(MySerial.available()) {
      MySerial.read();
    }
    
    server.send(200, "text/plain", "(keine Antwort)");
  }
}

// --- New: meta endpoint to provide relay names/count
void handleMeta(){
  String s = "{\"count\":" + String(RELAY_COUNT) + ",\"names\":[";
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    s += "\"" + String(RELAYS[i].title) + "\"";
    if(i<RELAY_COUNT-1) s += ",";
  }
  s += "]}";
  server.send(200,"application/json", s);
}

void registerAtHost(){
  if(WiFi.status()!=WL_CONNECTED) return;
  HTTPClient http; String regUrl="http://192.168.4.1/register";
  http.begin(regUrl); http.addHeader("Content-Type","application/json");
  String payload = "{\"name\":\"esp1\",\"ip\":\""+WiFi.localIP().toString()+"\"}";
  http.POST(payload); http.end();
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){
  if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP){
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    if(!serial2Started){ MySerial.begin(38400,SERIAL_8N1,UART2_RX_PIN,UART2_TX_PIN); serial2Started=true; }
    registerAtHost();
  } else if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){ WiFi.reconnect(); }
}

void startServer(){
  server.on("/state", HTTP_GET, handleState);
  server.on("/set",   HTTP_GET, handleSet);
  server.on("/temp",  HTTP_GET, handleTemp);
  server.on("/send",  HTTP_POST, handleSend);
  server.on("/meta",  HTTP_GET, handleMeta);
  server.begin();
}

void setup(){
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nESP1 startet...");
  
  // Relais initialisieren
  for(uint8_t i=0;i<RELAY_COUNT;i++){ 
    pinMode(RELAYS[i].pin, OUTPUT); 
    digitalWrite(RELAYS[i].pin, logicalToPhys(0)); 
  }
  
  // ADC initialisieren
  analogReadResolution(12); 
  analogSetAttenuation(ADC_11db);

  // WiFi initialisieren
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA); 
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.print("Verbinde mit WiFi");
  unsigned long startTime = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - startTime < 10000){
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if(WiFi.status() == WL_CONNECTED){
    Serial.print("WiFi verbunden! IP: ");
    Serial.println(WiFi.localIP());
    
    // Serial2 für RS232 initialisieren
    if(!serial2Started){ 
      MySerial.begin(38400, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN); 
      serial2Started = true;
      Serial.println("Serial2 (RS232) initialisiert auf 38400 baud");
    }
    
    registerAtHost();
  } else {
    Serial.println("WiFi Verbindung fehlgeschlagen!");
  }

  // HTTP Server starten
  startServer();
  Serial.println("HTTP Server gestartet");
  Serial.println("ESP1 bereit!\n");
}

void loop(){
  server.handleClient();
  
  unsigned long now = millis();
  
  // Temperatur alle 1 Sekunde messen
  if(now - lastTempMillis >= TEMP_INTERVAL){
    lastTempMillis = now;
    float Rntc = readR_NTC();
    float tempC = ntcToCelsius(Rntc);
    cachedTempC = tempC; 
    cachedRntc = Rntc;
    Serial.printf("Temp: %.2f C | R: %.0f\n", tempC, Rntc);
  }
  
  // Host-Registrierung separat alle 60 Sekunden
  static unsigned long lastReg = 0;
  if(now - lastReg > 60000){
    lastReg = now;
    registerAtHost();
  }
}