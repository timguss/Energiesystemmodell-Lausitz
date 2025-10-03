#include <WiFi.h>
#include <WebServer.h>

// === Konfiguration ===
// Wenn dein Relais MODULE bei LOW einschaltet (typisch): true
// Wenn dein Relais MODULE bei HIGH einschaltet: false
const bool ACTIVE_LOW = true;

//const int RELAY_PINS[4] = {23, 22, 21, 19}; // GPIOs (logical: 1=ON, 0=OFF)
const int RELAY_PINS[4] = {19, 21, 22, 23}; // GPIOs (logical: 1=ON, 0=OFF)
// Station-Infos (oder benutze AP-Modus wie früher)
const char* ssid = "Fritz-Box-SG";
const char* password = "floppy1905Frettchen";

WebServer server(80);

const char PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 — 4 Relays</title>
<style>body{font-family:Arial;padding:20px;max-width:600px;margin:auto} .slot{display:flex;align-items:center;justify-content:space-between;padding:12px;border-radius:10px;margin:10px 0;background:#f2f2f2}.label{font-weight:600}.switch{position:relative;width:60px;height:34px}.switch input{display:none}.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;border-radius:34px;transition:.3s}.slider:before{position:absolute;content:"";height:26px;width:26px;left:4px;top:4px;border-radius:50%;background:white;transition:.3s}input:checked + .slider{background:#4caf50}input:checked + .slider:before{transform:translateX(26px)}.state{font-size:13px;color:#333;margin-left:10px}.note{font-size:12px;color:#666;margin-top:8px}</style>
</head><body>
<h1>ESP32 — 4 Relays</h1>
<div id="controls"></div>
<div class="note">Schalter: Ein = ON, Aus = OFF. Seite aktualisiert automatisch.</div>
<script>
const gpioNames=[23,22,21,19];
function createSlot(i){
  const slot=document.createElement('div'); slot.className='slot';
  slot.innerHTML=`<div class="label">Relay ${i+1} (GPIO ${gpioNames[i]}) <span id="s${i}" class="state">…</span></div>
  <label class="switch"><input id="t${i}" type="checkbox"><span class="slider"></span></label>`;
  slot.querySelector('input').addEventListener('change',function(){
    const v=this.checked?1:0;
    fetch('/set?idx='+i+'&val='+v).then(()=>refreshState());
  });
  return slot;
}
function refreshState(){
  fetch('/state').then(r=>r.json()).then(obj=>{
    for(let i=0;i<4;i++){
      const st=obj['r'+i];
      const cb=document.getElementById('t'+i);
      const s=document.getElementById('s'+i);
      if(cb) cb.checked = (st===1);
      if(s) s.textContent = (st===1)?'ON':'OFF';
    }
  }).catch(e=>console.log('state fetch error',e));
}
document.addEventListener('DOMContentLoaded',()=>{
  const cont=document.getElementById('controls');
  for(let i=0;i<4;i++) cont.appendChild(createSlot(i));
  refreshState();
  setInterval(refreshState,1500);
});
</script>
</body></html>
)rawliteral";

// Map physical pin value -> logical (1=ON,0=OFF)
int physToLogical(int physVal) {
  if (ACTIVE_LOW) {
    // phys LOW  => ON (1)
    return (physVal == LOW) ? 1 : 0;
  } else {
    // phys HIGH => ON (1)
    return (physVal == HIGH) ? 1 : 0;
  }
}

// Map logical (1=ON,0=OFF) -> physical pin value
int logicalToPhys(int logical) {
  if (ACTIVE_LOW) {
    return (logical == 1) ? LOW : HIGH; // ON -> LOW, OFF -> HIGH
  } else {
    return (logical == 1) ? HIGH : LOW; // ON -> HIGH, OFF -> LOW
  }
}

String getStateJSON() {
  String s = "{";
  for (int i = 0; i < 4; ++i) {
    int phys = digitalRead(RELAY_PINS[i]);
    int logical = physToLogical(phys);
    s += "\"r";
    s += i;
    s += "\":";
    s += String(logical);
    if (i < 3) s += ",";
  }
  s += "}";
  return s;
}

void handleRoot() { server.send_P(200, "text/html", PAGE); }
void handleState() { server.send(200, "application/json", getStateJSON()); }

void handleSet() {
  if (!server.hasArg("idx") || !server.hasArg("val")) {
    server.send(400, "text/plain", "missing args");
    return;
  }
  int idx = server.arg("idx").toInt();
  int val = server.arg("val").toInt(); // 1=ON,0=OFF logical
  if (idx < 0 || idx > 3) {
    server.send(400, "text/plain", "bad idx");
    return;
  }
  digitalWrite(RELAY_PINS[idx], logicalToPhys(val));
  server.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== ESP32 4-Relays ===");
  Serial.print("ACTIVE_LOW = "); Serial.println(ACTIVE_LOW ? "true" : "false");

  // init pins: set default = OFF (logical 0 -> physical)
  for (int i = 0; i < 4; ++i) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], logicalToPhys(0)); // set OFF
  }

  // connect WiFi (Station)
  Serial.print("Verbinde mit WiFi SSID: "); Serial.println(ssid);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  const unsigned long timeout = 30000;
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("Verbunden mit WLAN!");
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Verbindung fehlgeschlagen -> starte AP als Fallback");
    WiFi.softAP("ESP32_Fallback", "12345678");
    Serial.print("Fallback-AP SSID: ESP32_Fallback  IP: ");
    Serial.println(WiFi.softAPIP());
  }

  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("HTTP-Server gestartet");
}

void loop() {
  server.handleClient();
}
