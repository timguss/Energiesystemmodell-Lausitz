#!/usr/bin/env python3
# esp_dashboard.py
# Flask + paho-mqtt dashboard: shows only active ESP devices and allows toggling relays
# Usage:
#   pip3 install flask paho-mqtt
#   python3 esp_dashboard.py
#
# You can override broker/settings via environment variables:
#   MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASS, HTTP_PORT, HEARTBEAT_TIMEOUT

from flask import Flask, jsonify, request, render_template_string
import paho.mqtt.client as mqtt
import threading, time, json, os

# ---------- CONFIG (can be overridden via env) ----------
MQTT_SERVER = os.environ.get('MQTT_SERVER', '127.0.0.1')   # broker IP
MQTT_PORT   = int(os.environ.get('MQTT_PORT', 1883))
MQTT_USER   = os.environ.get('MQTT_USER', 'modelltisch1')  # set to None if no auth
MQTT_PASS   = os.environ.get('MQTT_PASS', 'strukturwandel')
HEARTBEAT_TIMEOUT = int(os.environ.get('HEARTBEAT_TIMEOUT', 10))  # seconds
HTTP_PORT = int(os.environ.get('HTTP_PORT', 8080))
# -------------------------------------------------------

app = Flask(__name__)
devices = {}   # deviceID -> { temp, rntc, relays, meta, last_seen, ... }
lock = threading.Lock()

def ensure_device(dev):
    with lock:
        if dev not in devices:
            devices[dev] = {"temp": None, "rntc": None, "relays": {}, "meta": {}, "last_seen": 0}
        return devices[dev]

def parse_relays_payload(payload_str):
    """Parse various relay payload formats into dict {'r0':0,'r1':1,...}"""
    s = payload_str.strip()
    # try JSON first
    try:
        parsed = json.loads(s)
        if isinstance(parsed, dict):
            return {k: int(parsed[k]) for k in parsed}
        if isinstance(parsed, list):
            return {f"r{i}": int(parsed[i]) for i in range(len(parsed))}
    except Exception:
        pass
    # strip braces { ... }
    if s.startswith("{") and s.endswith("}"):
        s = s[1:-1].strip()
    # CSV
    if "," in s:
        parts = [p.strip() for p in s.split(",") if p.strip()!=""]
        return {f"r{i}": int(parts[i]) for i in range(len(parts))}
    # whitespace separated
    if " " in s:
        parts = [p.strip() for p in s.split() if p.strip()!=""]
        return {f"r{i}": int(parts[i]) for i in range(len(parts))}
    # single numeric
    try:
        v = int(s)
        return {"r0": v}
    except:
        return {}

def on_connect(client, userdata, flags, rc):
    print(f"[MQTT] connected rc={rc}")
    # subscribe broadly so we don't miss topics
    client.subscribe("esp/+/+")
    client.subscribe("esp/#")
    client.subscribe("esp32/#")

def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        payload = msg.payload.decode('utf-8', errors='ignore')
    except:
        payload = str(msg.payload)

    parts = topic.split('/')
    if len(parts) >= 3 and parts[0] == 'esp':
        dev = parts[1]
        field = "/".join(parts[2:])
        d = ensure_device(dev)
        with lock:
            d['last_seen'] = time.time()
        # handle known fields
        if field.endswith("relays") or field.endswith("relays/state"):
            r = parse_relays_payload(payload)
            with lock:
                d['relays'] = r
        elif field.endswith("meta"):
            try:
                j = json.loads(payload)
                with lock:
                    d['meta'] = j
            except:
                pass
        elif field.endswith("temp"):
            # payload can be JSON { "temp":xx, "rntc":yy } or plain number
            try:
                j = json.loads(payload)
                if isinstance(j, dict) and 'temp' in j:
                    with lock:
                        d['temp'] = float(j['temp']) if j['temp'] is not None else None
                        if 'rntc' in j: d['rntc'] = j.get('rntc')
                else:
                    with lock:
                        d['temp'] = float(payload)
            except:
                try:
                    with lock:
                        d['temp'] = float(payload)
                except:
                    pass
        elif field.endswith("rs232/reply") or field.endswith("reply"):
            with lock:
                d['last_rs232'] = payload
        else:
            with lock:
                d[field] = payload
    elif parts[0] == 'esp32':
        # legacy mapping -> treat as device 'esp32'
        dev = 'esp32'
        d = ensure_device(dev)
        with lock:
            d['last_seen'] = time.time()
        if len(parts) >= 2 and parts[1] == 'relays' and len(parts) > 2 and parts[2] == 'state':
            r = parse_relays_payload(payload)
            with lock:
                d['relays'] = r
        elif len(parts) >= 2 and parts[1] == 'temp':
            try:
                j = json.loads(payload)
                with lock:
                    d['temp'] = float(j.get('temp')) if isinstance(j, dict) else float(payload)
            except:
                try:
                    with lock:
                        d['temp'] = float(payload)
                except:
                    pass
        else:
            with lock:
                d['raw.' + '/'.join(parts[1:])] = payload

def mqtt_loop():
    client = mqtt.Client("rpi_dashboard")
    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.on_connect = on_connect
    client.on_message = on_message
    print("[MQTT] connecting to", MQTT_SERVER, MQTT_PORT)
    client.connect(MQTT_SERVER, MQTT_PORT, 60)
    client.loop_forever()

# ----------------- Flask UI & API -----------------
INDEX_HTML = """
<!doctype html>
<html>
<head><meta charset="utf-8"><title>ESP Dashboard</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{font-family:Arial;margin:12px;background:#f6f7fb}
.container{display:flex;flex-wrap:wrap;gap:12px}
.card{background:#fff;padding:12px;border-radius:8px;box-shadow:0 2px 6px rgba(0,0,0,0.06);width:360px}
.slot{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #eee}
.small{font-size:0.9em;color:#666}
.relays{margin-top:8px}
.relay{display:flex;justify-content:space-between;padding:6px 0}
button{padding:6px 10px;border-radius:6px;border:1px solid #ccc;background:#fff;cursor:pointer}
</style>
</head>
<body>
<h1>ESP Dashboard (active devices)</h1>
<div id="devices" class="container"></div>
<script>
async function refresh(){
  try{
    const r = await fetch('/api/state');
    const data = await r.json();
    const cont = document.getElementById('devices');
    cont.innerHTML = '';
    for(const dev in data){
      const item = data[dev];
      const div = document.createElement('div'); div.className='card';
      let html = `<h3>${dev}</h3>`;
      html += `<div class="small">Last seen: ${item.last_seen_str}</div>`;
      html += `<div class="slot"><div>Temp</div><div>${item.temp===null? '--' : (Number(item.temp).toFixed(2) + ' °C')}</div></div>`;
      html += `<div class="relays"><strong>Relays</strong>`;
      const relays = item.relays || {};
      const keys = Object.keys(relays).sort((a,b)=> {
        const ia = parseInt(a.replace(/[^0-9]/g,''))||0;
        const ib = parseInt(b.replace(/[^0-9]/g,''))||0;
        return ia - ib;
      });
      if(keys.length===0){
        html += `<div class="small">no relays reported</div>`;
      } else {
        for(let i=0;i<keys.length;i++){
          const k = keys[i];
          const v = relays[k];
          const name = (item.meta && item.meta.relays && item.meta.relays[i]) ? item.meta.relays[i] : k;
          html += `<div class="relay"><div>${name}</div><div><input type="checkbox" id="${dev}_${k}" ${v==1?"checked":""} onchange="toggleRelay('${dev}','${k}', this.checked)"></div></div>`;
        }
      }
      html += `</div>`;
      div.innerHTML = html;
      cont.appendChild(div);
    }
  }catch(e){
    console.error(e);
  }
}

async function toggleRelay(dev, key, checked){
  const idx = parseInt(key.replace(/[^0-9]/g,'')) || 0;
  const val = checked ? 1 : 0;
  try{
    await fetch('/api/set', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({dev:dev, idx: idx, val: val})
    });
    setTimeout(refresh, 300);
  }catch(e){
    console.error('set failed', e);
    alert('Failed to send command');
    setTimeout(refresh, 500);
  }
}

setInterval(refresh,1500);
refresh();
</script>
</body>
</html>
"""

@app.route('/')
def index():
    return render_template_string(INDEX_HTML)

@app.route('/api/state')
def api_state():
    now = time.time()
    cutoff = now - HEARTBEAT_TIMEOUT
    res = {}
    with lock:
        for dev, info in devices.items():
            if info.get('last_seen', 0) >= cutoff:
                out = {
                    'temp': info.get('temp'),
                    'rntc': info.get('rntc'),
                    'relays': info.get('relays', {}),
                    'meta': info.get('meta', {}),
                    'last_seen': info.get('last_seen'),
                    'last_seen_str': time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(info.get('last_seen',0)))
                }
                res[dev] = out
    return jsonify(res)

@app.route('/api/set', methods=['POST'])
def api_set():
    j = request.json or {}
    dev = j.get('dev')
    idx = j.get('idx')
    val = j.get('val')
    if dev is None or idx is None or val is None:
        return "missing", 400
    try:
        topic = f"rpi/{dev}/set"
        payload = f"{int(idx)}:{int(val)}"
        pub = mqtt.Client("rpi_publisher")
        if MQTT_USER:
            pub.username_pw_set(MQTT_USER, MQTT_PASS)
        pub.connect(MQTT_SERVER, MQTT_PORT, 60)
        pub.loop_start()
        pub.publish(topic, payload)
        time.sleep(0.05)
        pub.loop_stop()
        pub.disconnect()
        return "ok"
    except Exception as e:
        print("publish error:", e)
        return "error", 500

if __name__ == "__main__":
    threading.Thread(target=mqtt_loop, daemon=True).start()
    print("[HTTP] starting flask on port", HTTP_PORT)
    app.run(host='0.0.0.0', port=HTTP_PORT)
