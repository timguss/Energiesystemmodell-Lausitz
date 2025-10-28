#!/usr/bin/env python3
# pi_ui.py — Flask UI: grouped sliders (esp1 / esp2) with relay names, uses Host (/forward)

from flask import Flask, jsonify, request, render_template_string
import requests, json, time
from functools import lru_cache

HOST = "http://192.168.4.1"   # Host-ESP
HTTP_TIMEOUT = 3  # Reduziert von 8 auf 3 Sekunden

# Mapping global relay index -> (device, device_idx)
GLOBAL_MAP = {
    1: ("esp1", 0),  2: ("esp1", 1),  3: ("esp1", 2),  4: ("esp1", 3),
    5: ("esp1", 4),  6: ("esp1", 5),  7: ("esp1", 6),  8: ("esp1", 7),
    9: ("esp2", 0), 10: ("esp2", 1), 11: ("esp2", 2), 12: ("esp2", 3),
}

app = Flask(__name__, static_folder=None)

def host_get(path, timeout=HTTP_TIMEOUT):
    try:
        url = HOST + path
        r = requests.get(url, timeout=timeout)
        r.raise_for_status()
        try:
            return r.json()
        except:
            return r.text
    except requests.exceptions.Timeout:
        raise Exception("Timeout: Host antwortet nicht")
    except requests.exceptions.RequestException as e:
        raise Exception(f"Verbindungsfehler: {str(e)}")

def host_post(path, data=None, timeout=HTTP_TIMEOUT):
    try:
        url = HOST + path
        headers = {"Content-Type":"application/json"}
        r = requests.post(url, data=data, headers=headers, timeout=timeout)
        r.raise_for_status()
        try:
            return r.json()
        except:
            return r.text
    except requests.exceptions.Timeout:
        raise Exception("Timeout: Host antwortet nicht")
    except requests.exceptions.RequestException as e:
        raise Exception(f"Verbindungsfehler: {str(e)}")

def host_forward(target, method, path, body_str="", timeout=HTTP_TIMEOUT):
    payload = {"target": target, "method": method, "path": path, "body": body_str}
    resp = host_post("/forward", json.dumps(payload), timeout=timeout)
    # resp expected {"code":int, "body":"..."} where body may be a JSON string from device
    if isinstance(resp, dict):
        code = resp.get("code", -1)
        body = resp.get("body", "")
        # try to parse body into JSON if possible
        try:
            parsed = json.loads(body)
            return {"code": code, "body": parsed}
        except:
            return {"code": code, "body": body}
    return {"code": -1, "body": str(resp)}

# --- API for frontend ---
@app.route("/api/clients")
def api_clients():
    try:
        return jsonify(host_get("/clients", timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

# Cache für Meta-Daten (ändern sich nicht)
meta_cache = {}
meta_cache_time = {}

@app.route("/api/device_meta/<device>")
def api_device_meta(device):
    try:
        # Cache für 60 Sekunden
        now = time.time()
        if device in meta_cache and (now - meta_cache_time.get(device, 0)) < 60:
            return jsonify(meta_cache[device])
        
        res = host_forward(device, "GET", "/meta", timeout=2)
        if res.get("code") == 200:
            meta_cache[device] = res
            meta_cache_time[device] = now
        return jsonify(res)
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/device_state/<device>")
def api_device_state(device):
    try:
        res = host_forward(device, "GET", "/state", timeout=2)
        return jsonify(res)
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/relay/set", methods=["POST"])
def api_relay_set():
    j = request.get_json(force=True)
    gi = int(j.get("global_idx"))
    val = 1 if int(j.get("val")) else 0
    if gi not in GLOBAL_MAP:
        return jsonify({"error":"invalid index"}), 400
    device, idx = GLOBAL_MAP[gi]
    path = f"/set?idx={idx}&val={val}"
    try:
        # Kürzerer Timeout für schnellere Rückmeldung
        return jsonify(host_forward(device, "GET", path, timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

# RS232 and esp3 endpoints
@app.route("/api/rs232", methods=["POST"])
def api_rs232():
    j = request.get_json(force=True)
    cmd = j.get("cmd","")
    timeout = int(j.get("timeout",500))
    if not cmd:
        return jsonify({"error":"no cmd"}), 400
    print(cmd)

    # Falls der Client versehentlich literal backslashes gesendet hat, konvertiere:
    # "\\r\\n" -> real "\r\n"
    if "\\r\\n" in cmd:
        cmd = cmd.replace("\\r\\n", "\r\n")

    # Wenn echte CR+LF noch fehlt, hänge sie an
    if not cmd.endswith("\r\n"):
        cmd = cmd + "\r\n"

    inner = json.dumps({"cmd":cmd, "timeout":timeout})
    try:
        res = host_forward("esp1", "POST", "/send", inner, timeout=5)
        return jsonify(res)
    except Exception as e:
        return jsonify({"error": str(e)}), 502


@app.route("/api/esp3/state")
def api_esp3_state():
    try:
        return jsonify(host_forward("esp3", "GET", "/state", timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/esp3/set_wind", methods=["POST"])
def api_esp3_set_wind():
    j = request.get_json(force=True)
    val = 1 if int(j.get("val",0)) else 0
    try:
        return jsonify(host_forward("esp3", "GET", f"/set?val={val}", timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/esp3/set_pwm", methods=["POST"])
def api_esp3_set_pwm():
    j = request.get_json(force=True)
    pwm = int(j.get("pwm",0))
    pwm = max(0, min(255, pwm))
    try:
        return jsonify(host_forward("esp3", "GET", f"/train?pwm={pwm}", timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

# --- Frontend (optimiert) ---
INDEX_HTML = r'''
<!doctype html>
<html>
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Central UI</title>
<style>
body{font-family:Arial;margin:12px;background:#f4f6f8;color:#111}
.card{background:#fff;padding:12px;border-radius:8px;box-shadow:0 1px 6px rgba(0,0,0,0.06);margin:10px 0}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.section-title{font-weight:600;margin-bottom:8px}
.relay-row{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid #eee}
.label{flex:1}
.switch{position:relative;width:54px;height:30px;display:inline-block;margin-left:12px}
.switch input{display:none}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ddd;border-radius:34px;transition:.2s}
.slider:before{content:'';position:absolute;height:24px;width:24px;left:3px;top:3px;background:white;border-radius:50%;transition:.2s}
.switch input:checked + .slider{background:#4caf50}
.switch input:checked + .slider:before{transform:translateX(24px)}
.switch input:disabled + .slider{opacity:0.5;cursor:not-allowed}
.small{font-size:0.9em;color:#666}
.error{color:#d32f2f;font-size:0.85em;margin-top:4px}
.loading{opacity:0.6}
</style>
</head>
<body>
<div class="card"><h2>Kontrolle & Anzeige</h2><div class="small">Host: {{host}}</div></div>

<div class="card">
  <div class="section-title">ESP1 — Relais</div>
  <div id="esp1_relays">lade...</div>
</div>

<div class="card">
  <div class="section-title">ESP2 — Relais</div>
  <div id="esp2_relays">lade...</div>
</div>

<div class="card">
  <div class="section-title">ESP3 — Wind & Zug</div>
  <div>Wind: <strong id="windState">—</strong></div>
  <div style="margin-top:8px"><button onclick="setWind(1)">AN</button><button onclick="setWind(0)">AUS</button></div>
  <div style="margin-top:12px">
    <label>Zug PWM: <span id="pwmVal">0</span></label>
    <input id="pwmSlider" type="range" min="0" max="255" value="0" oninput="setPwm(this.value)">
    <button onclick="stopTrain()">Stop</button>
  </div>
</div>

<div class="card">
  <div class="section-title">ESP1 — Temperatur & RS232</div>
  <div>Temp: <span id="temp">--</span> °C</div>
  <div>R_ntc: <span id="rntc">--</span> Ω</div>
  <div style="margin-top:8px">
    <input id="rs_cmd" style="width:60%" placeholder=":06030401210120">
    <input id="rs_timeout" type="number" value="500" style="width:80px">
    <button onclick="sendRs()">Senden</button>
  </div>
  <pre id="rs_reply">—</pre>
</div>

<script>
let metaCache = {}; // Cache für Meta-Daten
let pendingRequests = new Set(); // Verhindert doppelte Requests

async function fetchMeta(device){
  if(metaCache[device]) return metaCache[device]; // Aus Cache
  if(pendingRequests.has('meta-'+device)) return null; // Request läuft bereits
  
  pendingRequests.add('meta-'+device);
  try{
    let r = await fetch('/api/device_meta/'+device); 
    if(!r.ok) throw r;
    let j = await r.json(); 
    if(j.error) throw j.error;
    let body = j.body || j;
    metaCache[device] = body; // Cachen
    return body;
  }catch(e){ 
    console.error('meta err',device,e); 
    return null; 
  } finally {
    pendingRequests.delete('meta-'+device);
  }
}

async function fetchState(device){
  if(pendingRequests.has('state-'+device)) return null; // Request läuft bereits
  
  pendingRequests.add('state-'+device);
  try{
    let r = await fetch('/api/device_state/'+device);
    if(!r.ok) throw r;
    let j = await r.json(); 
    if(j.error) throw j.error;
    return j.body || j;
  }catch(e){ 
    console.error('state err',device,e); 
    return null; 
  } finally {
    pendingRequests.delete('state-'+device);
  }
}

function mkRelayRow(global_idx, name, state){
  const row = document.createElement('div'); row.className='relay-row';
  const label = document.createElement('div'); label.className='label';
  label.innerHTML = '<strong>'+name+'</strong><div class="small">#'+global_idx+'</div>';
  const sw = document.createElement('label'); sw.className='switch';
  const input = document.createElement('input'); 
  input.type='checkbox'; 
  input.id = 'g'+global_idx;
  input.checked = (state==1);
  input.onchange = ()=> toggleRelay(global_idx, input.checked?1:0, input);
  const span = document.createElement('span'); span.className='slider';
  sw.appendChild(input); sw.appendChild(span);
  row.appendChild(label); row.appendChild(sw);
  return row;
}

async function buildRelays(){
  // esp1
  const meta1 = await fetchMeta('esp1'); 
  const state1 = await fetchState('esp1');
  const container1 = document.getElementById('esp1_relays'); 
  container1.innerHTML='';
  if(meta1 && meta1.names && state1){
    for(let i=0;i<meta1.count;i++){
      const name = meta1.names[i] || ('Relay '+(i+1));
      const st = (state1['r'+i]!==undefined)? state1['r'+i] : 0;
      container1.appendChild(mkRelayRow(i+1, name, st));
    }
  } else { 
    container1.innerHTML = '<div class="error">keine daten</div>'; 
  }

  // esp2
  const meta2 = await fetchMeta('esp2'); 
  const state2 = await fetchState('esp2');
  const container2 = document.getElementById('esp2_relays'); 
  container2.innerHTML='';
  if(meta2 && meta2.names && state2){
    for(let i=0;i<meta2.count;i++){
      const name = meta2.names[i] || ('Relay '+(i+1));
      const global_idx = 9 + i;
      const st = (state2['r'+i]!==undefined)? state2['r'+i] : 0;
      container2.appendChild(mkRelayRow(global_idx, name, st));
    }
  } else { 
    container2.innerHTML = '<div class="error">keine daten</div>'; 
  }
}

async function toggleRelay(global_idx, val, inputElement){
  // Sofort visuelles Feedback
  inputElement.disabled = true;
  
  try{
    let r = await fetch('/api/relay/set', {
      method:'POST', 
      headers:{'Content-Type':'application/json'}, 
      body: JSON.stringify({global_idx, val})
    });
    if(!r.ok) throw new Error('Request failed');
    
    // Nach 100ms State neu laden (gibt Zeit für ESP zu reagieren)
    setTimeout(async ()=>{
      inputElement.disabled = false;
      // Nur den betroffenen Device neu laden
      const device = global_idx <= 8 ? 'esp1' : 'esp2';
      await refreshDeviceState(device);
    }, 100);
  }catch(e){ 
    alert('Fehler beim Schalten: '+e); 
    inputElement.checked = !inputElement.checked; // Zurücksetzen
    inputElement.disabled = false;
  }
}

// Nur einen Device-State neu laden
async function refreshDeviceState(device){
  const state = await fetchState(device);
  if(!state) return;
  
  const meta = metaCache[device];
  if(!meta) return;
  
  // Update nur die Checkboxen
  const startIdx = device === 'esp1' ? 1 : 9;
  for(let i=0; i<meta.count; i++){
    const globalIdx = startIdx + i;
    const checkbox = document.getElementById('g'+globalIdx);
    if(checkbox){
      const newState = state['r'+i] === 1;
      if(checkbox.checked !== newState) checkbox.checked = newState;
    }
  }
}

// ESP3, RS232 helpers
async function refreshEsp3(){
  try{
    let r = await fetch('/api/esp3/state'); 
    if(!r.ok) throw r;
    let j = await r.json();
    let body = j.body || j;
    document.getElementById('windState').textContent = body.running? 'AN':'AUS';
    document.getElementById('pwmVal').textContent = body.pwm;
    document.getElementById('pwmSlider').value = body.pwm;
  }catch(e){
    console.error('esp3 err',e);
  }
}

async function setWind(val){ 
  await fetch('/api/esp3/set_wind',{method:'POST',headers:{'Content-Type':'application/json'},body: JSON.stringify({val})}); 
  setTimeout(refreshEsp3, 100);
}

async function setPwm(v){ 
  document.getElementById('pwmVal').textContent = v; 
  await fetch('/api/esp3/set_pwm',{method:'POST',headers:{'Content-Type':'application/json'},body: JSON.stringify({pwm:parseInt(v)})}); 
}

async function stopTrain(){ 
  await fetch('/api/esp3/set_pwm',{method:'POST',headers:{'Content-Type':'application/json'},body: JSON.stringify({pwm:0})}); 
  document.getElementById('pwmVal').textContent='0'; 
  document.getElementById('pwmSlider').value=0; 
}

// RS232
// Ersetze vorhandene sendRs() im <script>
async function sendRs(){
  // Rohtext aus Feld, ohne trim() — wir müssen genaue Endung prüfen
  let raw = document.getElementById('rs_cmd').value;
  let timeout = parseInt(document.getElementById('rs_timeout').value) || 500;
  if(!raw){ alert('Bitte Befehl'); return; }

  // Wenn Nutzer literal "\r\n" eingegeben hat, ersetze durch echten CR+LF
  // (string contains backslash + r + backslash + n)
  raw = raw.replace(/\\r\\n$/, '\r\n');

  // Wenn jetzt noch kein echtes CR+LF am Ende, füge es an
  if(!raw.endsWith('\r\n')) raw = raw + '\r\n';

  document.getElementById('rs_reply').textContent = 'sende...';
  try{
    let r = await fetch('/api/rs232', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify({cmd: raw, timeout})
    });
    let j = await r.json();
    let body = j.body || j;
    if(typeof body === 'object') body = JSON.stringify(body, null, 2);
    document.getElementById('rs_reply').textContent = (body || JSON.stringify(j));
  }catch(e){ document.getElementById('rs_reply').textContent = 'Fehler: '+e; }
}


async function refreshTemps(){
  try{
    let r = await fetch('/api/device_state/esp1'); 
    if(!r.ok) return;
    let j = await r.json(); 
    let body = j.body || j;
    if(body.temp !== undefined) document.getElementById('temp').textContent = (body.temp===null?'--':Number(body.temp).toFixed(2));
    if(body.rntc !== undefined) document.getElementById('rntc').textContent = (body.rntc===null?'--':Number(body.rntc).toFixed(0));
  }catch(e){
    console.error('temp err',e);
  }
}

// Optimiertes Polling: nur States, nicht Meta
async function refreshAll(){
  await Promise.all([
    refreshDeviceState('esp1'),
    refreshDeviceState('esp2'),
    refreshEsp3(),
    refreshTemps()
  ]);
}

// Initial: Meta laden und Relays bauen
buildRelays();

// Danach nur noch States updaten (alle 3 Sekunden statt 1.5)
setInterval(refreshAll, 3000);
</script>
</body>
</html>
'''

@app.route("/")
def index():
    return render_template_string(INDEX_HTML, host=HOST)

if __name__ == "__main__":
    print("Starte Flask UI. Host:", HOST)
    app.run(host="0.0.0.0", port=8000, debug=False)