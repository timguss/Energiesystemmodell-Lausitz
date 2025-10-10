#include <WiFi.h>
#include <WebServer.h>
#include <math.h>

///// Benutzerkonfiguration /////
const char* WIFI_SSID = "Fritz-Box-SG";
const char* WIFI_PASS = "floppy1905Frettchen";

///// UART2 (ProPar) /////
const int UART2_RX_PIN = 16; // ESP RX  <- Modul TX
const int UART2_TX_PIN = 17; // ESP TX  -> Modul RX
const unsigned long DEFAULT_REPLY_TIMEOUT = 500; // ms
HardwareSerial MySerial(2); // UART2
bool serial2Started = false;

///// Relais /////
const bool ACTIVE_LOW = true;   // LOW = an, HIGH = aus
struct RelayConfig { uint8_t pin; uint8_t relayNum; const char* title; };
const uint8_t RELAY_COUNT = 8;
RelayConfig RELAYS[RELAY_COUNT] = {
  {19, 1, "Ventil - 1"}, {21, 2, "Ventil - 2"}, {22, 3, "Heizstab"}, {23, 4, "Zünder"},
  {32, 5, "Gasventil"}, {33, 6, "Kühler"}, {25, 7, "MFC - fehlt noch"}, {26, 8, "unbelegt"}
};

///// NTC /////
const int adcPin = 34;
const float Vcc = 3.3;
const float Rf = 10000.0;
const float R0 = 10000.0;
const float T0_K = 298.15;  // 25°C in Kelvin
const float beta = 3950.0;

float cachedTempC = NAN;
float cachedRntc = NAN;
unsigned long lastTempMillis = 0;
const unsigned long TEMP_INTERVAL = 1000;

bool rs232Enabled = true;

WebServer server(80);

// Hilfsfunktionen
int physToLogical(int physVal){ return ACTIVE_LOW ? (physVal==LOW?1:0) : (physVal==HIGH?1:0);}
int logicalToPhys(int logical){ return ACTIVE_LOW ? (logical==1?LOW:HIGH) : (logical==1?HIGH:LOW);}

float readVoltage(){ int adc=analogRead(adcPin); return (float)adc/4095.0*Vcc; }
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

// --- Webserver Handlers ---
String getRelayStateJSON(){
  String s="{";
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    s += "\"r"+String(i)+"\":"+String(physToLogical(digitalRead(RELAYS[i].pin)));
    if(i<RELAY_COUNT-1) s+=",";
  }
  s+="}";
  return s;
}

void handleIndex();
void handleState(){ server.send(200,"application/json",getRelayStateJSON()); }
void handleSet(){
  if(!server.hasArg("idx")||!server.hasArg("val")){server.send(400,"text/plain","missing args");return;}
  int idx=server.arg("idx").toInt();
  int val=server.arg("val").toInt();
  if(idx<0||idx>=RELAY_COUNT||(val!=0&&val!=1)){server.send(400,"text/plain","invalid args");return;}
  digitalWrite(RELAYS[idx].pin, logicalToPhys(val));
  server.send(200,"text/plain","ok");
}

void handleTemp(){
  String j = "{\"temp\":"+(isnan(cachedTempC)?"null":String(cachedTempC,2))+
             ",\"rntc\":"+(isnan(cachedRntc)?"null":String(cachedRntc,0))+"}";
  server.send(200,"application/json",j);
}

void handleSend(){
  if(!rs232Enabled){ server.send(400,"text/plain","RS232 deaktiviert"); return;}
  if(!serial2Started){ server.send(500,"text/plain","Serial2 nicht initialisiert."); return;}
  if(!server.hasArg("plain")){ server.send(400,"text/plain","Kein Body"); return;}
  
  String body = server.arg("plain"); // einfache String-Verarbeitung statt JSON-Library
  int idx1 = body.indexOf("\"cmd\"");
  int idx2 = body.indexOf("\"timeout\"");
  String cmd = "";
  unsigned long timeout = DEFAULT_REPLY_TIMEOUT;
  
  if(idx1 >=0){
    int q1 = body.indexOf("\"", idx1+5);
    int q2 = body.indexOf("\"", q1+1);
    if(q1>=0 && q2>q1) cmd = body.substring(q1+1,q2);
  }
  if(idx2 >=0){
    int colon = body.indexOf(":",idx2);
    int comma = body.indexOf("}",colon);
    if(colon>=0 && comma>colon){
      String tstr = body.substring(colon+1,comma);
      tstr.trim();
      timeout = tstr.toInt();
    }
  }
  
  if(cmd.length()==0){ server.send(400,"text/plain","Kein cmd gefunden"); return; }
  if(!cmd.endsWith("\r\n")) cmd += "\r\n";
  
  MySerial.print(cmd);
  String resp = "";
  unsigned long start = millis();
  while(millis()-start < timeout){
    while(MySerial.available()) resp += (char)MySerial.read();
  }
  
  server.send(200,"text/plain",resp.length()?resp:"(keine Antwort)");
}

// --- WiFi Event ---
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){
  switch(event){
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WIFI EVENT] Verbunden. IP: "); Serial.println(WiFi.localIP());
      if(!serial2Started){
        MySerial.begin(38400,SERIAL_8N1,UART2_RX_PIN,UART2_TX_PIN);
        serial2Started=true;
        Serial.println("Serial2 gestartet via Event.");
      }
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WIFI EVENT] getrennt. Versuche reconnect...");
      WiFi.reconnect();
      break;
    default: break;
  }
}

void startServerHandlers(){
  server.on("/",HTTP_GET,handleIndex);
  server.on("/state",HTTP_GET,handleState);
  server.on("/set",HTTP_GET,handleSet);
  server.on("/temp",HTTP_GET,handleTemp);
  server.on("/send",HTTP_POST,handleSend);
  server.begin();
  Serial.println("HTTP server gestartet");
}

// --- HTML Web UI ---
const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Kombi</title>
<style>
body{font-family:Arial;padding:14px;background:#f5f7fb}
.card{background:#fff;padding:12px;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,0.06);margin-bottom:14px}
.slot{display:flex;justify-content:space-between;padding:12px;margin:8px 0;background:#f2f2f2;border-radius:8px}
.switch{position:relative;display:inline-block;width:60px;height:34px}
.switch input{display:none}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;transition:.3s;border-radius:34px}
.slider:before{position:absolute;content:'';height:26px;width:26px;left:4px;top:4px;background:white;transition:.3s;border-radius:50%}
input:checked + .slider{background:#4caf50}
input:checked + .slider:before{transform:translateX(26px)}
.state{margin-left:10px;font-size:14px;color:#333}
</style>
</head>
<body>
<div class="card">
<h2>Relais</h2>
<div id="relays"></div>
</div>

<div class="card">
<h2>Temperatur</h2>
<div>Temperatur: <strong id="temp">--</strong> °C</div>
<div>R_ntc: <strong id="rntc">--</strong> Ω</div>
</div>

<div class="card">
<h2>RS232</h2>
<label><input type="checkbox" id="rs232toggle" checked> RS232 aktiv</label><br><br>
<label for="cmd">Befehl:</label><br>
<input id="cmd" type="text" placeholder=":06030401210120">
<div style="margin-top:8px">
<label for="timeout">Timeout (ms):</label>
<input id="timeout" type="number" value="500" style="width:120px">
</div>
<div style="margin-top:12px">
<button id="sendBtn">Send</button>
</div>
<div class="card">
<div><strong>Sent:</strong><pre id="sent">—</pre></div>
<div style="margin-top:8px"><strong>Reply:</strong><pre id="reply">—</pre></div>
</div>
</div>

<script>
const RELAY_COUNT = 8;
async function refreshState(){
  try{
    let r = await fetch('/state'); let obj = await r.json();
    let html=""; for(let i=0;i<RELAY_COUNT;i++){
      let st=obj['r'+i]; html+="<div class='slot'><div>"+(i+1)+"</div><div><label class='switch'><input id='t"+i+"' type='checkbox' "+(st===1?"checked":"")+"><span class='slider'></span></label></div></div>";
    }
    document.getElementById('relays').innerHTML = html;
    for(let i=0;i<RELAY_COUNT;i++){ document.getElementById('t'+i).addEventListener('change',function(){ fetch('/set?idx='+i+'&val='+(this.checked?1:0));});}
  }catch(e){}
  try{
    let rt = await fetch('/temp'); let jo = await rt.json();
    document.getElementById('temp').textContent = (jo.temp!==null?jo.temp.toFixed(2):'--');
    document.getElementById('rntc').textContent = (jo.rntc!==null?jo.rntc.toFixed(0):'--');
  }catch(e){}
}
setInterval(refreshState,1500); refreshState();

document.getElementById('sendBtn').addEventListener('click', async () => {
  if(!document.getElementById('rs232toggle').checked){alert("RS232 deaktiviert"); return;}
  const cmd = document.getElementById('cmd').value.trim();
  const timeout = parseInt(document.getElementById('timeout').value) || 500;
  if (!cmd) { alert('Bitte Befehl eingeben.'); return; }
  document.getElementById('sent').textContent = cmd;
  document.getElementById('reply').textContent = 'sende...';
  try{
    const r = await fetch('/send',{method:'POST',headers:{'Content-Type':'text/plain'},body:JSON.stringify({cmd:cmd,timeout:timeout})});
    const txt = await r.text();
    document.getElementById('reply').textContent = txt||'(keine Antwort)';
  }catch(e){document.getElementById('reply').textContent = 'Fehler: '+e;}
});
</script>
</body>
</html>
)rawliteral";

void handleIndex() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "text/html", index_html);
}


// --- Setup/Loop ---
void setup(){
  Serial.begin(115200); delay(50);
  Serial.println("\n=== ESP32 Kombi ===");

  for(uint8_t i=0;i<RELAY_COUNT;i++){ pinMode(RELAYS[i].pin,OUTPUT); digitalWrite(RELAYS[i].pin,logicalToPhys(0)); }

  analogReadResolution(12); analogSetAttenuation(ADC_11db);

  WiFi.onEvent(onWiFiEvent); WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASS);

  Serial.printf("Verbinde mit WLAN SSID: %s ...\n", WIFI_SSID);
  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<15000){ delay(500); Serial.print("."); } Serial.println();

  if(WiFi.status()==WL_CONNECTED){
    Serial.print("[Setup] WLAN verbunden. IP: "); Serial.println(WiFi.localIP());
    MySerial.begin(38400,SERIAL_8N1,UART2_RX_PIN,UART2_TX_PIN); serial2Started=true; Serial.println("Serial2 gestartet.");
  } else Serial.println("[Setup] WARN: WLAN nicht verbunden.");

  startServerHandlers();
  Serial.println("Setup fertig.");
}

void loop(){
  server.handleClient();

  // Temperatur alle TEMP_INTERVAL aktualisieren
  unsigned long now = millis();
  if(now-lastTempMillis >= TEMP_INTERVAL){
    lastTempMillis = now;
    int adcRaw = analogRead(adcPin);
    float Rntc = readR_NTC();
    float tempC = ntcToCelsius(Rntc);
    cachedTempC = tempC; cachedRntc = Rntc;
    Serial.print("ADC: "); Serial.print(adcRaw);
    Serial.print(" | R_ntc = "); Serial.print(Rntc);
    Serial.print(" Ω | T = "); Serial.print(tempC); Serial.println(" °C");
  }
}
