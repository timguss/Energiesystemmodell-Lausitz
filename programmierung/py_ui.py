#!/usr/bin/env python3
# pi_ui.py — Flask UI: grouped sliders (esp1 / esp2) with relay names, uses Host (/forward)

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
    resp = host_post("/forward", json.dumps(payload))
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
        return jsonify(host_get("/clients"))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/device_meta/<device>")
def api_device_meta(device):
    try:
        # query device /meta via host
        res = host_forward(device, "GET", "/meta")
        return jsonify(res)
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/device_state/<device>")
def api_device_state(device):
    try:
        res = host_forward(device, "GET", "/state")
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
        return jsonify(host_forward(device, "GET", path))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

# RS232 and esp3 endpoints unchanged (kept minimal)
@app.route("/api/rs232", methods=["POST"])
def api_rs232():
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

# --- Frontend (grouped relays with names and slider/toggles) ---
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
.small{font-size:0.9em;color:#666}
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
async function fetchMeta(device){
  try{
    let r = await fetch('/api/device_meta/'+device); if(!r.ok) throw r;
    let j = await r.json(); if(j.error) throw j.error;
    return j.body || j; // host_forward returns {"code":..,"body":parsed}
  }catch(e){ console.error('meta err',device,e); return null; }
}
async function fetchState(device){
  try{
    let r = await fetch('/api/device_state/'+device); if(!r.ok) throw r;
    let j = await r.json(); if(j.error) throw j.error;
    return j.body || j;
  }catch(e){ console.error('state err',device,e); return null; }
}
function mkRelayRow(global_idx, name, state){
  const row = document.createElement('div'); row.className='relay-row';
  const label = document.createElement('div'); label.className='label';
  label.innerHTML = '<strong>'+name+'</strong><div class="small">#'+global_idx+'</div>';
  const sw = document.createElement('label'); sw.className='switch';
  const input = document.createElement('input'); input.type='checkbox'; input.id = 'g'+global_idx;
  input.checked = (state==1);
  input.onchange = ()=> toggleRelay(global_idx, input.checked?1:0);
  const span = document.createElement('span'); span.className='slider';
  sw.appendChild(input); sw.appendChild(span);
  row.appendChild(label); row.appendChild(sw);
  return row;
}
async function buildRelays(){
  // esp1
  const meta1 = await fetchMeta('esp1'); const state1 = await fetchState('esp1');
  const container1 = document.getElementById('esp1_relays'); container1.innerHTML='';
  if(meta1 && meta1.names){
    for(let i=0;i<meta1.count;i++){
      const name = meta1.names[i] || ('Relay '+(i+1));
      const st = (state1 && state1['r'+i]!==undefined)? state1['r'+i] : 0;
      container1.appendChild(mkRelayRow(i+1, name, st));
    }
  } else { container1.textContent = 'keine daten'; }

  // esp2
  const meta2 = await fetchMeta('esp2'); const state2 = await fetchState('esp2');
  const container2 = document.getElementById('esp2_relays'); container2.innerHTML='';
  if(meta2 && meta2.names){
    for(let i=0;i<meta2.count;i++){
      const name = meta2.names[i] || ('Relay '+(i+1));
      const global_idx = 9 + i; // mapping 9..12
      const st = (state2 && state2['r'+i]!==undefined)? state2['r'+i] : 0;
      container2.appendChild(mkRelayRow(global_idx, name, st));
    }
  } else { container2.textContent = 'keine daten'; }
}

async function toggleRelay(global_idx, val){
  try{
    await fetch('/api/relay/set', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({global_idx, val})});
    // visual state already updated by checkbox; optional: refresh state after small delay
    setTimeout(()=>buildRelays(),200);
  }catch(e){ alert('Fehler: '+e); }
}

// ESP3, RS232 helpers (unchanged)
async function refreshEsp3(){
  try{
    let r = await fetch('/api/esp3/state'); if(!r.ok) throw r;
    let j = await r.json();
    let body = j.body || j;
    document.getElementById('windState').textContent = body.running? 'AN':'AUS';
    document.getElementById('pwmVal').textContent = body.pwm;
    document.getElementById('pwmSlider').value = body.pwm;
  }catch(e){}
}
async function setWind(val){ await fetch('/api/esp3/set_wind',{method:'POST',headers:{'Content-Type':'application/json'},body: JSON.stringify({val})}); }
async function setPwm(v){ document.getElementById('pwmVal').textContent = v; await fetch('/api/esp3/set_pwm',{method:'POST',headers:{'Content-Type':'application/json'},body: JSON.stringify({pwm:parseInt(v)})}); }
async function stopTrain(){ await fetch('/api/esp3/set_pwm',{method:'POST',headers:{'Content-Type':'application/json'},body: JSON.stringify({pwm:0})}); document.getElementById('pwmVal').textContent='0'; document.getElementById('pwmSlider').value=0; }

// RS232
async function sendRs(){
  let cmd = document.getElementById('rs_cmd').value.trim();
  let timeout = parseInt(document.getElementById('rs_timeout').value) || 500;
  if(!cmd){ alert('Bitte Befehl'); return; }
  document.getElementById('rs_reply').textContent = 'sende...';
  try{
    let r = await fetch('/api/rs232', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({cmd, timeout})});
    let j = await r.json();
    let body = j.body || j;
    if(typeof body === 'object') body = JSON.stringify(body, null, 2);
    document.getElementById('rs_reply').textContent = (body || JSON.stringify(j));
  }catch(e){ document.getElementById('rs_reply').textContent = 'Fehler: '+e; }
}

async function refreshAll(){
  await Promise.all([buildRelays(), refreshEsp3(), refreshTemps()]);
}

async function refreshTemps(){
  try{
    let r = await fetch('/api/device_state/esp1'); if(!r.ok) return;
    let j = await r.json(); let body = j.body || j;
    if(body.temp !== undefined) document.getElementById('temp').textContent = (body.temp===null?'--':Number(body.temp).toFixed(2));
    if(body.rntc !== undefined) document.getElementById('rntc').textContent = (body.rntc===null?'--':Number(body.rntc).toFixed(0));
  }catch(e){}
}

setInterval(refreshAll,1500);
refreshAll();
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
