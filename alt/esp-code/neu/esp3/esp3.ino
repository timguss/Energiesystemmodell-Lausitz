// esp3_dashboard_forwarder_final_no_rebuild.ino
// ESP3: SoftAP + Dashboard + Forwarder + lokale Steuerung (Zug PWM, Windrad Motor+LED)
// Verbesserte Dashboard-JS: aktualisiert DOM inkrementell, verhindert Fokusverlust / Eingabeverlust
// Benötigt: ArduinoJson (6.x)

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

const char* AP_SSID = "ESP-AP";
const char* AP_PASS = "espap123";
IPAddress apIP(192,168,4,1);

WebServer server(80);
const char* DEVICE_ID = "esp3";

// --- Hardware: Zug + Windrad ---
const int pwmPin = 25;
const uint32_t pwmFreq = 5000;
const uint8_t pwmRes = 8;
int pwmValue = 0;

const int motorPin = 32;
bool motorOn = false;

const int ledPin = 33;
bool ledOn = false;

const int LED_PIN = 2;

// --- Clients storage ---
struct ClientInfo {
  String id;
  String ip;
  String lastStatusJson;
  unsigned long lastSeen;
};
#define MAX_CLIENTS 8
ClientInfo clients[MAX_CLIENTS];

// --- Self update ---
const unsigned long SELF_UPDATE_INTERVAL = 4000;
unsigned long lastSelfUpdate = 0;

// --- Helpers ---
int findClientSlotById(const String &id){
  for(int i=0;i<MAX_CLIENTS;i++) if(clients[i].id == id) return i;
  return -1;
}

void printClients(){
  Serial.println("--- Clients ---");
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].id.length()){
      Serial.printf("[%d] id=%s ip=%s lastSeen=%lu\n", i, clients[i].id.c_str(), clients[i].ip.c_str(), clients[i].lastSeen);
    }
  }
  Serial.println("---------------");
}

void cleanupStaleClients(){
  unsigned long now = millis();
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].id.length()){
      if(clients[i].id == String(DEVICE_ID)) continue; // keep self
      if(now - clients[i].lastSeen > 30000UL){
        Serial.printf("Client %s stale (lastSeen %lu), removing\n", clients[i].id.c_str(), clients[i].lastSeen);
        clients[i].id=""; clients[i].ip=""; clients[i].lastStatusJson=""; clients[i].lastSeen=0;
      }
    }
  }
}

// --- Dashboard HTML (improved JS: incremental update) ---
const char dashboard_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'/>
  <title>ESP Dashboard</title>
  <style>
    body{font-family:Arial,Helvetica,sans-serif;margin:12px}
    h1{font-size:20px}
    .dev{border:1px solid #ccc;padding:8px;margin:8px 0;border-radius:6px;background:#fafafa}
    .dev-head{display:flex;justify-content:space-between;align-items:center}
    .small{font-size:12px;color:#666}
    .controls{margin-top:8px}
    .label-inline{display:inline-block;margin-right:8px}
    .tip{font-size:11px;color:#666;margin-top:6px}
    input[type="text"]{width:60%}
  </style>
</head>
<body>
  <h1>ESP Dashboard</h1>
  <div id='main'>loading...</div>
<script>
/* incremental dashboard updater
   - builds device container once
   - updates values (temp, relay states, pwm, motor, led)
   - preserves RS232 input value & focus
*/

const POLL_MS = 1500;
let devicesMap = {}; // id -> { container, elems... }

function makeDeviceContainer(id, ip){
  const container = document.createElement('div');
  container.className = 'dev';
  container.id = 'dev_' + id;

  const head = document.createElement('div');
  head.className = 'dev-head';
  const title = document.createElement('div');
  title.innerHTML = '<b>' + id + '</b> <span class="small">(' + ip + ')</span>';
  const lastSeenEl = document.createElement('div');
  lastSeenEl.className = 'small';
  lastSeenEl.id = 'ls_' + id;
  head.appendChild(title);
  head.appendChild(lastSeenEl);
  container.appendChild(head);

  const body = document.createElement('div');
  body.className = 'controls';
  // placeholders
  const tempEl = document.createElement('div'); tempEl.id = 'temp_' + id;
  body.appendChild(tempEl);

  const relaysEl = document.createElement('div'); relaysEl.id = 'relays_' + id;
  body.appendChild(relaysEl);

  // RS232 area (only for esp1, will be toggled visible later)
  const rsDiv = document.createElement('div'); rsDiv.id = 'rsdiv_' + id;
  rsDiv.style.display = 'none';
  const rsInput = document.createElement('input'); rsInput.type='text'; rsInput.id = 'rs_in_' + id; rsInput.placeholder='Text to send';
  const rsBtn = document.createElement('button'); rsBtn.textContent='Send';
  const rsResp = document.createElement('span'); rsResp.id = 'rs_resp_' + id;
  rsBtn.addEventListener('click', ()=> sendRS(id));
  rsDiv.appendChild(rsInput); rsDiv.appendChild(rsBtn); rsDiv.appendChild(rsResp);
  body.appendChild(rsDiv);

  // ESP3-specific controls (pwm + motor + led)
  const localDiv = document.createElement('div'); localDiv.id = 'local_' + id;
  localDiv.style.display='none';
  // pwm
  const pwmLabel = document.createElement('label'); pwmLabel.className='label-inline'; pwmLabel.textContent='Zug PWM: ';
  const pwmSlider = document.createElement('input'); pwmSlider.type='range'; pwmSlider.min=0; pwmSlider.max=255; pwmSlider.id='pwm_'+id;
  const pwmVal = document.createElement('span'); pwmVal.id='pwmv_'+id; pwmVal.textContent='0';
  pwmSlider.addEventListener('input', ()=> {
    pwmVal.textContent = pwmSlider.value;
    sendCmd(id, 'pwm:' + pwmSlider.value);
  });
  pwmLabel.appendChild(pwmSlider); pwmLabel.appendChild(pwmVal);
  localDiv.appendChild(pwmLabel);
  // motor button
  const motorBtn = document.createElement('button'); motorBtn.id='motor_'+id;
  motorBtn.addEventListener('click', ()=> {
    const cur = motorBtn.getAttribute('data-on') === '1';
    sendCmd(id, '0:' + (cur ? 0 : 1));
  });
  localDiv.appendChild(document.createElement('div')).appendChild(motorBtn);
  // led button
  const ledBtn = document.createElement('button'); ledBtn.id='led_'+id;
  ledBtn.addEventListener('click', ()=> {
    const cur = ledBtn.getAttribute('data-on') === '1';
    sendCmd(id, '1:' + (cur ? 0 : 1));
  });
  localDiv.appendChild(document.createElement('div')).appendChild(ledBtn);

  body.appendChild(localDiv);

  // tip
  const tip = document.createElement('div'); tip.className='tip';
  tip.textContent = 'Tip: Relays/RS232 send raw text to device /cmd (e.g. 0:1 or rs232:HELLO)';
  body.appendChild(tip);

  container.appendChild(body);
  return { container, lastSeenEl, tempEl, relaysEl, rsDiv, rsInput, rsResp, localDiv, pwmSlider, pwmVal, motorBtn, ledBtn };
}

async function refreshOnce(){
  try {
    const res = await fetch('/status', {cache:'no-store'});
    const j = await res.json();
    const seen = new Set();
    for(const d of j.devices){
      seen.add(d.id);
      if(!devicesMap[d.id]){
        const el = makeDeviceContainer(d.id, d.ip);
        document.getElementById('main').appendChild(el.container);
        devicesMap[d.id] = el;
      }
      updateDevice(d);
    }
    // remove devices not seen
    for(const id in devicesMap){
      if(!seen.has(id)){
        const el = document.getElementById('dev_' + id);
        if(el) el.remove();
        delete devicesMap[id];
      }
    }
  } catch(e){
    console.error('refresh error', e);
  }
}

function updateDevice(d){
  const el = devicesMap[d.id];
  if(!el) return;
  // lastSeen
  el.lastSeenEl.textContent = 'lastSeen: ' + new Date(d.lastSeen).toLocaleTimeString();

  // TEMP
  if(d.status && d.status.temp !== undefined){
    el.tempEl.innerHTML = 'Temperatur: <b>' + (d.status.temp.toFixed(1)) + ' °C</b>';
  } else {
    el.tempEl.innerHTML = '';
  }

  // RELAYS: create checkboxes only once, then update checked state
  if(d.status && Array.isArray(d.status.relays)){
    // if not built yet, create them
    if(!el.relaysEl.hasChildNodes() || el.relaysEl.getAttribute('data-built') !== '1'){
      el.relaysEl.innerHTML = ''; // ensure empty
      const names = (d.status.relay_names && Array.isArray(d.status.relay_names)) ? d.status.relay_names : [];
      for(let i=0;i<d.status.relays.length;i++){
        const label = document.createElement('label');
        label.className='label-inline';
        const cb = document.createElement('input');
        cb.type='checkbox';
        cb.id = d.id + '_r_' + i;
        // attach change handler
        cb.addEventListener('change', (ev)=> {
          const val = ev.target.checked ? 1 : 0;
          sendCmd(d.id, i + ':' + val);
        });
        const txt = document.createTextNode((names[i] ? names[i] : ('R'+i)));
        label.appendChild(cb);
        label.appendChild(txt);
        el.relaysEl.appendChild(label);
      }
      el.relaysEl.setAttribute('data-built','1');
    }
    // update checked state (without triggering change event)
    for(let i=0;i<d.status.relays.length;i++){
      const cb = document.getElementById(d.id + '_r_' + i);
      if(cb){
        cb.checked = !!d.status.relays[i];
      }
    }
  } else {
    // no relays
    el.relaysEl.innerHTML = '';
    el.relaysEl.removeAttribute('data-built');
  }

  // RS232 visible only for esp1
  if(d.id === 'esp1'){
    el.rsDiv.style.display = '';
    // don't overwrite rsInput.value to preserve typing
  } else {
    el.rsDiv.style.display = 'none';
  }

  // ESP3 local controls
  if(d.id === 'esp3'){
    el.localDiv.style.display = '';
    // pwm
    const pwmValNum = (d.status && d.status.pwm !== undefined) ? d.status.pwm : 0;
    if(el.pwmSlider.value != pwmValNum){
      el.pwmSlider.value = pwmValNum;
      el.pwmVal.textContent = pwmValNum;
    }
    // motor button
    const motorState = (d.status && d.status.motor !== undefined) ? (d.status.motor==1) : false;
    el.motorBtn.setAttribute('data-on', motorState ? '1':'0');
    el.motorBtn.textContent = motorState ? 'Stop' : 'Start';
    // led button
    const ledState = (d.status && d.status.led !== undefined) ? (d.status.led==1) : false;
    el.ledBtn.setAttribute('data-on', ledState ? '1':'0');
    el.ledBtn.textContent = ledState ? 'LED Off' : 'LED On';
  } else {
    el.localDiv.style.display = 'none';
  }
}

async function sendCmd(id, cmd){
  try{
    const res = await fetch('/set', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify({id:id, cmd:cmd})
    });
    const j = await res.json();
    return j;
  } catch(e){
    console.error('sendCmd error', e);
  }
}

async function sendRS(id){
  const el = devicesMap[id];
  if(!el) return;
  const val = el.rsInput.value;
  if(!val) return;
  const r = await sendCmd(id, 'rs232:' + val);
  if(r && r.resp){
    el.rsResp.innerText = ' → ' + (r.resp ? r.resp.substr(0,200) : '');
    setTimeout(()=>{ el.rsResp.innerText = ''; }, 5000);
  }
}

let refreshTimer = null;
function startPolling(){
  if(refreshTimer) clearInterval(refreshTimer);
  refreshOnce(); // initial
  refreshTimer = setInterval(refreshOnce, POLL_MS);
}

window.onload = function(){
  document.getElementById('main').innerHTML = ''; // clear placeholder
  startPolling();
};
</script>
</body>
</html>
)rawliteral";

String getDashboardPage(){
  return String(dashboard_html);
}

// --- HTTP Handlers ---
void handleRoot(){
  Serial.println("HTTP GET / -> serving dashboard");
  server.send(200, "text/html", getDashboardPage());
}

void handleStatus(){
  Serial.println("HTTP GET /status");
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.createNestedArray("devices");
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].id.length()){
      DynamicJsonDocument sub(2048);
      DeserializationError derr = deserializeJson(sub, clients[i].lastStatusJson);
      JsonObject obj = arr.createNestedObject();
      obj["id"] = clients[i].id;
      obj["ip"] = clients[i].ip;
      obj["lastSeen"] = clients[i].lastSeen;
      if(!derr) obj["status"] = sub.as<JsonObject>();
      else obj["status"] = JsonObject();
    }
  }
  String out;
  serializeJson(doc, out);
  Serial.println("DEBUG /status JSON:");
  Serial.println(out);
  server.send(200, "application/json", out);
}

// improved handleRegister: correctly serialize incoming status variant
void handleRegister(){
  Serial.println("HTTP POST /register");
  if(server.hasArg("plain")==false){ server.send(400,"text/plain","no body"); return; }
  String body = server.arg("plain");
  Serial.println("Body: " + body);

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, body);
  if(err){
    Serial.print("deserializeJson error: ");
    Serial.println(err.c_str());
    server.send(400,"text/plain","bad json");
    return;
  }

  String id = doc["id"].as<String>();
  String ip = doc["ip"].as<String>();
  JsonVariant status = doc["status"];

  // debug show parsed status
  {
    String tmp;
    serializeJson(status, tmp);
    Serial.println("Parsed status (serialized): " + tmp);
  }

  int slot = findClientSlotById(id);
  if(slot == -1){
    for(int i=0;i<MAX_CLIENTS;i++){
      if(clients[i].id.length()==0){ slot=i; break; }
    }
  }
  if(slot == -1){
    Serial.println(" -> no slots free");
    server.send(500,"text/plain","no slots");
    return;
  }

  clients[slot].id = id;
  clients[slot].ip = ip;

  // serialize status variant directly into string (this preserves object contents)
  String st;
  if(!status.isNull()){
    serializeJson(status, st);
  } else {
    st = "{}";
  }

  clients[slot].lastStatusJson = st;
  clients[slot].lastSeen = millis();

  server.send(200,"text/plain","ok");
  Serial.printf("Registered %s @ %s (slot %d)\n", id.c_str(), ip.c_str(), slot);
  Serial.println("Stored lastStatusJson: " + st);
  printClients();
}

void applyLocalCmd(const String &cmd){
  if(cmd.startsWith("pwm:")){
    int val = cmd.substring(4).toInt();
    val = constrain(val, 0, 255);
    pwmValue = val;
    ledcWrite(pwmPin, pwmValue);
    Serial.printf("Local PWM set to %d\n", pwmValue);
  } else {
    int colon = cmd.indexOf(':');
    if(colon>0){
      int idx = cmd.substring(0,colon).toInt();
      int val = cmd.substring(colon+1).toInt();
      if(idx == 0){
        motorOn = (val == 1);
        digitalWrite(motorPin, motorOn ? HIGH : LOW);
        Serial.printf("Motor -> %d\n", motorOn?1:0);
      } else if(idx == 1){
        ledOn = (val == 1);
        digitalWrite(ledPin, ledOn ? HIGH : LOW);
        Serial.printf("LED -> %d\n", ledOn?1:0);
      } else {
        Serial.printf("Unknown local idx %d\n", idx);
      }
    }
  }
}

void handleSet(){
  Serial.println("HTTP POST /set");
  if(server.hasArg("plain")==false){ server.send(400,"text/plain","no body"); return; }
  String body = server.arg("plain");
  Serial.println("Body: " + body);

  DynamicJsonDocument doc(512);
  if(deserializeJson(doc, body)){
    Serial.println(" -> bad json in /set");
    server.send(400,"text/plain","bad json");
    return;
  }
  String id = doc["id"].as<String>();
  String cmd = doc["cmd"].as<String>();

  if(id == String(DEVICE_ID)){
    Serial.println(" -> applying local cmd: " + cmd);
    applyLocalCmd(cmd);
    // update stored own status immediately
    DynamicJsonDocument sdoc(256);
    JsonObject s = sdoc.createNestedObject("status");
    s["pwm"] = pwmValue;
    s["motor"] = motorOn ? 1 : 0;
    s["led"] = ledOn ? 1 : 0;
    String st; serializeJson(s, st);
    int slot = findClientSlotById(String(DEVICE_ID));
    if(slot == -1){
      for(int i=0;i<MAX_CLIENTS;i++){ if(clients[i].id.length()==0){ slot=i; break; } }
    }
    if(slot!=-1){ clients[slot].id = String(DEVICE_ID); clients[slot].ip = apIP.toString(); clients[slot].lastStatusJson = st; clients[slot].lastSeen = millis(); }
    server.send(200,"application/json","{\"code\":200}");
    return;
  }

  // find client ip
  String targetIp = "";
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].id == id){ targetIp = clients[i].ip; break; }
  }
  if(targetIp == ""){
    Serial.println(" -> unknown id " + id);
    server.send(404,"text/plain","unknown id");
    return;
  }

  // forward http POST to http://<targetIp>/cmd with raw body = cmd
  HTTPClient http;
  String url = "http://" + targetIp + "/cmd";
  Serial.println("Forwarding to " + url + " cmd='" + cmd + "'");
  http.begin(url);
  http.addHeader("Content-Type","text/plain");
  int code = http.POST(cmd);
  String resp = http.getString();
  http.end();
  Serial.printf(" -> remote returned %d, resp len=%d\n", code, resp.length());

  String reply = "{\"code\":" + String(code) + ",\"resp\":\"" + String(resp) + "\"}";
  server.send(200,"application/json",reply);
  Serial.printf("Forwarded to %s: %s -> %d\n", targetIp.c_str(), cmd.c_str(), code);
}

// --- Setup & Loop ---
void setup(){
  Serial.begin(115200);
  delay(50);
  Serial.println("\n=== ESP3 AP Server Starting ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(motorPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(motorPin, LOW);
  digitalWrite(ledPin, LOW);

  bool ledcOk = ledcAttach(pwmPin, pwmFreq, pwmRes);
  Serial.printf("ledcAttach(pin=%d,freq=%u,res=%u) -> %s\n", pwmPin, pwmFreq, pwmRes, ledcOk ? "OK":"FAIL");
  ledcWrite(pwmPin, pwmValue);

  WiFi.mode(WIFI_AP);
  bool confOk = WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));
  Serial.printf("softAPConfig returned: %s\n", confOk ? "true":"false");
  int apChannel = 1;
  bool apOk = WiFi.softAP(AP_SSID, AP_PASS, apChannel);
  Serial.printf("softAP returned: %s (channel %d)\n", apOk ? "true":"false", apChannel);
  Serial.print("AP gestartet: "); Serial.println(AP_SSID);
  Serial.print("AP-IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/register", handleRegister);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("Webserver gestartet on port 80");

  // prime own slot so dashboard shows esp3 at start
  DynamicJsonDocument sdoc(256);
  JsonObject s = sdoc.createNestedObject("status");
  s["pwm"] = pwmValue;
  s["motor"] = motorOn ? 1 : 0;
  s["led"] = ledOn ? 1 : 0;
  String st; serializeJson(s, st);
  int slot = findClientSlotById(String(DEVICE_ID));
  if(slot == -1){
    for(int i=0;i<MAX_CLIENTS;i++){ if(clients[i].id.length()==0){ slot=i; break; } }
  }
  if(slot != -1){
    clients[slot].id = String(DEVICE_ID);
    clients[slot].ip = apIP.toString();
    clients[slot].lastStatusJson = st;
    clients[slot].lastSeen = millis();
  }
  printClients();
  lastSelfUpdate = millis();
}

void loop(){
  server.handleClient();
  if(millis() - lastSelfUpdate >= SELF_UPDATE_INTERVAL){
    int slot = findClientSlotById(String(DEVICE_ID));
    if(slot != -1){ clients[slot].lastSeen = millis(); }
    lastSelfUpdate = millis();
  }
  cleanupStaleClients();
}
