#!/usr/bin/env python3
# pi_ui.py — zentrale Web-UI (Flask). UI sendet nur Befehle an Host-ESP (192.168.4.1).
# Pi zeigt Antworten/Messwerte an, führt keine Steuerlogik aus.

from flask import Flask, jsonify, request, render_template_string
import requests, json, time

HOST = "http://192.168.4.1"   # Host-ESP
HTTP_TIMEOUT = 8

# Mapping global relay index -> (device, device_idx)
GLOBAL_MAP = {
    1: ("esp1", 0),  2: ("esp1", 1),  3: ("esp1", 2),  4: ("esp1", 3),
    5: ("esp1", 4),  6: ("esp1", 5),  7: ("esp1", 6),  8: ("esp1", 7),
    9: ("esp2", 0), 10: ("esp2", 1), 11: ("esp2", 2), 12: ("esp2", 3),
}

app = Flask(__name__, static_folder=None)

def host_get(path):
    url = HOST + path
    r = requests.get(url, timeout=HTTP_TIMEOUT)
    r.raise_for_status()
    try:
        return r.json()
    except:
        return r.text

def host_post(path, data=None):
    url = HOST + path
    headers = {"Content-Type":"application/json"}
    r = requests.post(url, data=data, headers=headers, timeout=HTTP_TIMEOUT)
    r.raise_for_status()
    try:
        return r.json()
    except:
        return r.text

def host_forward(target, method, path, body_str=""):
    payload = {"target": target, "method": method, "path": path, "body": body_str}
    return host_post("/forward", json.dumps(payload))

# ---------------- API für Frontend ----------------

@app.route("/api/clients")
def api_clients():
    try:
        return jsonify(host_get("/clients"))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/relay/set", methods=["POST"])
def api_relay_set():
    # json: { "global_idx": int, "val": 0|1 }
    j = request.get_json(force=True)
    gi = int(j.get("global_idx"))
    val = 1 if int(j.get("val")) else 0
    if gi not in GLOBAL_MAP:
        return jsonify({"error":"invalid index"}), 400
    device, idx = GLOBAL_MAP[gi]
    path = f"/set?idx={idx}&val={val}"
    try:
        res = host_forward(device, "GET", path)
        return jsonify(res)
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/relay/state/<int:global_idx>")
def api_relay_state(global_idx):
    if global_idx not in GLOBAL_MAP:
        return jsonify({"error":"invalid index"}), 400
    device, _ = GLOBAL_MAP[global_idx]
    try:
        return jsonify(host_forward(device, "GET", "/state"))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/temp/<device>")
def api_temp(device):
    try:
        return jsonify(host_forward(device, "GET", "/temp"))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/rs232", methods=["POST"])
def api_rs232():
    # json: { "cmd": "...", "timeout": 500 }
    j = request.get_json(force=True)
    cmd = j.get("cmd","")
    timeout = int(j.get("timeout",500))
    if not cmd:
        return jsonify({"error":"no cmd"}), 400
    inner = json.dumps({"cmd":cmd, "timeout":timeout})
    try:
        res = host_forward("esp1", "POST", "/send", inner)
        return jsonify(res)
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/esp3/state")
def api_esp3_state():
    try:
        return jsonify(host_forward("esp3", "GET", "/state"))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/esp3/set_wind", methods=["POST"])
def api_esp3_set_wind():
    j = request.get_json(force=True)
    val = 1 if int(j.get("val",0)) else 0
    try:
        return jsonify(host_forward("esp3", "GET", f"/set?val={val}"))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/esp3/set_pwm", methods=["POST"])
def api_esp3_set_pwm():
    j = request.get_json(force=True)
    pwm = int(j.get("pwm",0))
    pwm = max(0, min(255, pwm))
    try:
        return jsonify(host_forward("esp3", "GET", f"/train?pwm={pwm}"))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

# generic forward (for advanced use)
@app.route("/api/forward", methods=["POST"])
def api_forward():
    j = request.get_json(force=True)
    target = j.get("target")
    method = j.get("method","GET")
    path = j.get("path","/")
    body = j.get("body","")
    if not target or not path:
        return jsonify({"error":"missing args"}), 400
    try:
        return jsonify(host_forward(target, method, path, body))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

# ---------------- Frontend ----------------

INDEX_HTML = r'''
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Central UI</title>
<style>
body{font-family:Arial;margin:12px;background:#f4f6f8;color:#111}
.header{display:flex;gap:12px;align-items:center}
.card{background:#fff;padding:12px;border-radius:8px;box-shadow:0 1px 6px rgba(0,0,0,0.06);margin:10px 0}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.small{font-size:0.9em;color:#555}
button{padding:6px 10px;border-radius:6px;border:1px solid #ccc;background:#eee;cursor:pointer}
input[type=range]{width:100%}
textarea{width:100%;height:140px}
</style>
</head>
<body>
<div class="header">
  <h2>Kontrolle & Anzeige</h2>
  <div class="small">Host: {{host}}</div>
</div>

<div class="card">
  <h3>Geräte (registriert)</h3>
  <pre id="clients">lade...</pre>
</div>

<div class="grid">
  <div class="card">
    <h3>Relais 1..12</h3>
    <div id="relays"></div>
  </div>

  <div class="card">
    <h3>ESP3 — Wind & Zug</h3>
    <div>Wind: <strong id="windState">—</strong></div>
    <div style="margin-top:8px">
      <button onclick="setWind(1)">AN</button>
      <button onclick="setWind(0)">AUS</button>
    </div>
    <div style="margin-top:12px">
      <label>Zug PWM: <span id="pwmVal">0</span></label>
      <input id="pwmSlider" type="range" min="0" max="255" value="0" oninput="setPwm(this.value)">
      <button onclick="stopTrain()">Stop</button>
    </div>
  </div>
</div>

<div class="card">
  <h3>ESP1 — Temperatur & RS232</h3>
  <div>Temp: <span id="temp">--</span> °C</div>
  <div>R_ntc: <span id="rntc">--</span> Ω</div>

  <div style="margin-top:10px">
    <label>RS232 Befehl:</label><br>
    <input id="rs_cmd" style="width:60%" placeholder=":06030401210120">
    <input id="rs_timeout" type="number" value="500" style="width:80px">
    <button onclick="sendRs()">Senden</button>
  </div>
  <div style="margin-top:8px">
    <strong>Antwort:</strong>
    <pre id="rs_reply">—</pre>
  </div>
</div>

<script>
const HOST = "{{host}}";
async function fetchClients(){
  try{
    let r = await fetch('/api/clients'); if(!r.ok) throw r;
    let j = await r.json();
    document.getElementById('clients').textContent = JSON.stringify(j, null, 2);
    buildRelays(j);
  }catch(e){ document.getElementById('clients').textContent = 'fehler: '+e; }
}
function buildRelays(clients){
  // create 12 controls
  let container = document.getElementById('relays');
  container.innerHTML = '';
  for(let i=1;i<=12;i++){
    let div = document.createElement('div');
    div.style.display='flex'; div.style.justifyContent='space-between'; div.style.margin='6px 0';
    let label = document.createElement('div'); label.textContent = 'Relais '+i;
    let btnOn = document.createElement('button'); btnOn.textContent='AN'; btnOn.onclick = ()=>setRelay(i,1);
    let btnOff = document.createElement('button'); btnOff.textContent='AUS'; btnOff.onclick = ()=>setRelay(i,0);
    div.appendChild(label); div.appendChild(btnOn); div.appendChild(btnOff);
    container.appendChild(div);
  }
}
async function setRelay(global_idx, val){
  try{
    await fetch('/api/relay/set', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({global_idx, val})});
  }catch(e){ alert('Fehler: '+e); }
}
async function refreshRelays(){
  // optional: could read states per-device; for simplicity we just refresh clients and temps
  await fetchClients();
}
async function refreshTemps(){
  try{
    let r = await fetch('/api/temp/esp1'); if(!r.ok) throw r;
    let j = await r.json();
    if(j.temp!==undefined) document.getElementById('temp').textContent = (j.temp===null?'--':Number(j.temp).toFixed(2));
    if(j.rntc!==undefined) document.getElementById('rntc').textContent = (j.rntc===null?'--':Number(j.rntc).toFixed(0));
  }catch(e){}
}
async function refreshEsp3(){
  try{
    let r = await fetch('/api/esp3/state'); if(!r.ok) throw r;
    let j = await r.json();
    document.getElementById('windState').textContent = j.running? 'AN':'AUS';
    document.getElementById('pwmVal').textContent = j.pwm;
    document.getElementById('pwmSlider').value = j.pwm;
  }catch(e){}
}
async function sendRs(){
  let cmd = document.getElementById('rs_cmd').value.trim();
  let timeout = parseInt(document.getElementById('rs_timeout').value) || 500;
  if(!cmd){ alert('Bitte Befehl'); return; }
  document.getElementById('rs_reply').textContent = 'sende...';
  try{
    let r = await fetch('/api/rs232', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({cmd, timeout})});
    let j = await r.json();
    if(j.body) {
      // host/forward returns {"code":..., "body":"..."} often
      let txt = (typeof j.body === 'string')? j.body : JSON.stringify(j.body);
      document.getElementById('rs_reply').textContent = txt.replace(/\\n/g,'\n').replace(/\\r/g,'\r');
    } else if(j.error) document.getElementById('rs_reply').textContent = 'error: '+j.error;
    else document.getElementById('rs_reply').textContent = JSON.stringify(j);
  }catch(e){ document.getElementById('rs_reply').textContent = 'Fehler: '+e; }
}
async function setWind(val){ await fetch('/api/esp3/set_wind', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({val})}); }
async function setPwm(v){ await fetch('/api/esp3/set_pwm', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({pwm:parseInt(v)})}); }
async function stopTrain(){ await fetch('/api/esp3/set_pwm', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({pwm:0})}); }

setInterval(()=>{ refreshClientsAndState(); }, 1500);
async function refreshClientsAndState(){ await Promise.all([fetchClients(), refreshTemps(), refreshEsp3()]); }
fetchClients(); refreshTemps(); refreshEsp3();
</script>
</body>
</html>
'''

@app.route("/")
def index():
    return render_template_string(INDEX_HTML, host=HOST)

# ---------------- Start ----------------
if __name__ == "__main__":
    print("Starte Flask UI. Host-ESP:", HOST)
    app.run(host="0.0.0.0", port=8000, debug=False)
