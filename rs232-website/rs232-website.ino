#include <WiFi.h>
#include <WebServer.h>
#include <math.h>

///// BENUTZER KONFIGURATION /////
const char* WIFI_SSID = "Fritz-Box-SG";
const char* WIFI_PASS = "floppy1905Frettchen";

///// RELAIS KONFIGURATION /////
const bool ACTIVE_LOW = true; // LOW = an
struct RelayConfig {
  uint8_t pin;
  const char* title;
};
const uint8_t RELAY_COUNT = 8;
RelayConfig RELAYS[RELAY_COUNT] = {
  {19, "Ventil - 1"},
  {21, "Ventil - 2"},
  {22, "Heizstab"},
  {23, "Zünder"},
  {32, "Gasventil"},
  {33, "Kühler"},
  {25, "MFC - fehlt noch"},
  {26, "unbelegt"}
};

///// NTC SENSOR KONFIGURATION /////
const int adcPin = 34;
const float Vcc = 3.3;
const float Rf = 10000.0;     // fester Widerstand
const float R0 = 10000.0;     // NTC bei 25°C
const float T0_K = 298.15;    // 25°C in Kelvin
const float beta = 3950.0;

///// RS232 / SERIAL2 KONFIGURATION /////
const bool SERIAL2_ENABLED = false; // SAFE TEST, standardmäßig aus
const int UART2_RX_PIN = 16;
const int UART2_TX_PIN = 17;
HardwareSerial MySerial(2);

///// WEB SERVER /////
WebServer server(80);

///// HILFSFUNKTIONEN RELAIS /////
int physToLogical(int physVal) {
  return ACTIVE_LOW ? (physVal == LOW ? 1 : 0) : (physVal == HIGH ? 1 : 0);
}
int logicalToPhys(int logical) {
  return ACTIVE_LOW ? (logical == 1 ? LOW : HIGH) : (logical == 1 ? HIGH : LOW);
}

String getRelaysJSON() {
  String s="{";
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    int phys = digitalRead(RELEYS[i].pin);
    s += "\"r"+String(i)+"\":"+String(physToLogical(phys))+",";
    s += "\"t"+String(i)+"\":\""+String(RELAYS[i].title)+"\"";
    if(i<RELAY_COUNT-1) s+=",";
  }
  s+="}";
  return s;
}

///// HILFSFUNKTIONEN NTC /////
float readVoltage() {
  int adc = analogRead(adcPin);
  return (float)adc / 4095.0 * Vcc;
}

float readR_NTC() {
  float v = readVoltage();
  if(v <= 0.0001 || v >= Vcc-0.0001) return -1.0; // Sensor nicht angeschlossen oder Fehler
  return Rf * (v / (Vcc - v));
}

float ntcToCelsius(float Rntc) {
  if(Rntc <= 0) return NAN;
  float invT = (1.0f / T0_K) + (1.0f / beta) * log(Rntc / R0);
  float T = 1.0f / invT;
  return T - 273.15f;
}

///// WEB HANDLER /////
void handleRoot() {
  String page = "<!doctype html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>ESP32 Kombi</title>"
                "<style>"
                "body{font-family:Arial;padding:20px;max-width:800px;margin:auto}"
                ".slot{display:flex;align-items:center;justify-content:space-between;padding:12px;margin:10px 0;background:#f2f2f2;border-radius:8px}"
                ".label{font-weight:600}.switch{position:relative;display:inline-block;width:60px;height:34px}"
                ".switch input{display:none}"
                ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;transition:.3s;border-radius:34px}"
                ".slider:before{position:absolute;content:'';height:26px;width:26px;left:4px;top:4px;background:white;transition:.3s;border-radius:50%}"
                "input:checked + .slider{background:#4caf50}"
                "input:checked + .slider:before{transform:translateX(26px)}"
                ".state{margin-left:10px;font-size:14px;color:#333}"
                "</style></head><body><h2>ESP32 Kombi (Relais & Temperatur)</h2>";

  for(uint8_t i=0;i<RELAY_COUNT;i++){
    int phys = digitalRead(RELAYS[i].pin);
    int logical = physToLogical(phys);
    page += "<div class='slot'><div class='label'>"+String(RELAYS[i].title)
            +" <span id='s"+String(i)+"' class='state'>"+String(logical==1?"AN":"AUS")+"</span></div>"
            +"<label class='switch'><input id='t"+String(i)+"' type='checkbox' "
            +(logical==1?"checked":"")+"><span class='slider'></span></label></div>";
  }

  page += "<div style='margin-top:20px'><h3>Temperatur</h3>"
          "<div>Sensor: <span id='temp'>--</span> °C</div>"
          "<div>Rntc: <span id='rntc'>--</span> Ω</div></div>";

  page += "<script>"
          "const count="+String(RELAY_COUNT)+";"
          "for(let i=0;i<count;i++){"
          "document.getElementById('t'+i).addEventListener('change',function(){"
          "let v=this.checked?1:0;"
          "fetch('/set?idx='+i+'&val='+v).then(()=>setTimeout(refresh,100));"
          "});"
          "}"
          "function refresh(){"
          "fetch('/state').then(r=>r.json()).then(obj=>{"
          "for(let i=0;i<count;i++){"
          "let st=obj['r'+i];"
          "let cb=document.getElementById('t'+i);"
          "let s=document.getElementById('s'+i);"
          "if(cb) cb.checked=(st===1);"
          "if(s) s.textContent=(st===1?'AN':'AUS');"
          "}"
          "document.getElementById('temp').textContent=obj.temp;"
          "document.getElementById('rntc').textContent=obj.rntc;"
          "});"
          "}"
          "setInterval(refresh,1500);"
          "</script></body></html>";
  server.send(200,"text/html",page);
}

void handleSet() {
  if(!server.hasArg("idx")||!server.hasArg("val")){ server.send(400,"text/plain","missing args"); return; }
  int idx = server.arg("idx").toInt();
  int val = server.arg("val").toInt();
  if(idx<0 || idx>=RELAY_COUNT || (val!=0 && val!=1)){ server.send(400,"text/plain","invalid args"); return; }
  digitalWrite(RELAYS[idx].pin, logicalToPhys(val));
  server.send(200,"text/plain","ok");
}

void handleState() {
  float Rntc = readR_NTC();
  float tempC = ntcToCelsius(Rntc);
  String sTemp = isnan(tempC) ? "NO SENSOR" : String(tempC,2);
  String sRntc = (Rntc<0) ? "NO SENSOR" : String(Rntc,1);
  String s="{\"temp\":"+sTemp+",\"rntc\":"+sRntc;
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    s+=",\"r"+String(i)+"\":"+String(physToLogical(digitalRead(RELAYS[i].pin)));
  }
  s+="}";
  server.send(200,"application/json",s);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 Kombi (SAFE) starting ===");

  // Relais Pins
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    pinMode(RELAYS[i].pin, OUTPUT);
    digitalWrite(RELAYS[i].pin, logicalToPhys(0));
  }

  // WLAN
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi SSID: "); Serial.print(WIFI_SSID); Serial.println(" ... (non-blocking)");

  // Server Handlers
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/state", handleState);
  server.begin();
  Serial.println("HTTP server started.");

  // Serial2 nur optional
  if(SERIAL2_ENABLED){
    MySerial.begin(38400, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
    Serial.println("Serial2 initialized.");
  } else {
    Serial.println("Serial2 DISABLED (SAFE).");
  }

  Serial.println("Setup done.");
}

void loop() {
  server.handleClient();
}
