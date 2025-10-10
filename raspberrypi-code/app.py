# app_multi.py
from flask import Flask, jsonify, request, render_template_string
import paho.mqtt.client as mqtt
import threading, time, json

# --- Konfiguration ---
# Broker auf dem Pi: wenn dein Mosquitto lokal läuft, 127.0.0.1
MQTT_BROKER = "127.0.0.1"
MQTT_PORT = 1883
MQTT_USER = "modelltisch1"
MQTT_PASS = "strukturwandel"

app = Flask(__name__)

# Zustand aller Geräte:
# devices = {
#   "esp1": {"meta": {"relays": [...]}, "temp":..., "rntc":..., "relays": {"r0":0,...}, "last_rs232": "..."},
#   "esp2": {...}
# }
devices = {}

client = mqtt.Client("rpi_multi_webclient")
client.username_pw_set(MQTT_USER, MQTT_PASS)

def ensure_device(dev):
    if dev not in devices:
        devices[dev] = {"meta": {}, "temp": None, "rntc": None, "relays": {}, "last_rs232": ""}
    return devices[dev]

def on_connect(client, userdata, flags, rc):
    print("MQTT connected rc=", rc)
    # subscribe to all esp topics: esp/<device>/...
    client.subscribe("esp/+/temp")
    client.subscribe("esp/+/relays/state")
    client.subscribe("esp/+/meta")
    client.subscribe("esp/+/rs232/reply")
    # backward compatibility: older code used "esp32/..." — subscribe as well
    client.subscribe("esp32/#")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode('utf-8', errors='ignore')
    parts = topic.split('/')
    try:
        # new style: esp/<device>/...
        if len(parts) >= 3 and parts[0] == 'esp':
            dev = parts[1]
            sub = '/'.join(parts[2:])
            d = ensure_device(dev)
            if sub == 'temp':
                try:
                    j = json.loads(payload)
                    d['temp'] = j.get('temp')
                    d['rntc'] = j.get('rntc')
                except:
                    # fallback plain float
                    try:
                        d['temp'] = float(payload)
                    except:
                        pass
            elif sub == 'relays/state':
                try:
                    j = json.loads(payload)
                    for k,v in j.items():
                        d['relays'][k] = int(v)
                except:
                    pass
            elif sub == 'meta':
                try:
                    j = json.loads(payload)
                    d['meta'] = j
                    # if meta contains relays list, ensure keys r0.. exist
                    if isinstance(j.get('relays'), list):
                        for i,name in enumerate(j.get('relays')):
                            key = f"r{i}"
                            if key not in d['relays']:
                                d['relays'][key] = 0
                except:
                    pass
            elif sub == 'rs232/reply':
                d['last_rs232'] = payload
            else:
                # ignore others
                pass
        else:
            # legacy handling for esp32/...
            if parts[0] == 'esp32':
                dev = 'esp32'
                d = ensure_device(dev)
                # attempt similar mapping
                if len(parts) >= 2 and parts[1] == 'temp':
                    try:
                        j = json.loads(payload)
                        d['temp'] = j.get('temp')
                        d['rntc'] = j.get('rntc')
                    except:
                        pass
                elif len(parts) >= 3 and parts[1] == 'relays' and parts[2] == 'state':
                    try:
                        j = json.loads(payload)
                        for k,v in j.items():
                            d['relays'][k] = int(v)
                    except:
                        pass
                elif len(parts) >= 3 and parts[1] == 'rs232' and parts[2] == 'reply':
                    d['last_rs232'] = payload
    except Exception as e:
        print("on_message error:", e, topic, payload)

client.on_connect = on_connect
client.on_message = on_message

def mqtt_thread():
    # Connect with reconnect loop
    while True:
        try:
            client.connect(MQTT_BROKER, MQTT_PORT, 60)
            client.loop_forever()
        except Exception as e:
            print("MQTT connection error:", e)
            time.sleep(5)

# Start MQTT background thread
t = threading.Thread(target=mqtt_thread, daemon=True)
t.start()

# ----------------- Flask UI & API -----------------
INDEX_HTML = '''
<!doctype html>
<html>
<head><meta charset="utf-8"><title>RPi - Multi ESP Control</title>
<style>
body{font-family:Arial;margin:12px;background:#f6f7fb}
.container{display:flex;gap:12px;flex-wrap:wrap}
.card{background:#fff;padding:12px;border-radius:8px;margin-bottom:12px;box-shadow:0 2px 6px rgba(0,0,0,0.06);width:340px}
.slot{display:flex;justify-content:space-between;align-items:center;padding:8px 0}
button{padding:8px 12px;border-radius:6px;border:1px solid #ccc;background:#fff;cursor:pointer}
</style>
</head>
<body>
<h1>RPi — Multi ESP Übersicht</h1>
<div id="devices" class="container"></div>

<script>
async function refresh(){
  try{
    const r = await fetch('/api/state'); const data = await r.json();
    const devDiv = document.getElementById('devices');
    devDiv.innerHTML = '';
    for(const dev in data){
      const obj = data[dev];
      const card = document.createElement('div'); card.className='card';
      let html = `<h3>${dev}</h3>`;
      html += `<div>Temp: <strong>${obj.temp===null? '--' : (Number(obj.temp).toFixed(2))}</strong> °C</div>`;
      html += `<div>R_ntc: <strong>${obj.rntc===null? '--' : Math.round(obj.rntc)}</strong> Ω</div>`;
      html += `<button onclick="requestTemp('${dev}')">Sofort Temp anfordern</button>`;
      html += `<hr><h4>Relais</h4><div id="${dev}_relays">`;

      // if meta.relays present, use names; else show keys
      let relays = obj.relays || {};
      let names = (obj.meta && obj.meta.relays) ? obj.meta.relays : null;

      // ensure at least r0..r7 if none exist
      if(Object.keys(relays).length===0 && names){
        for(let i=0;i<names.length;i++){ relays['r'+i] = relays['r'+i] || 0; }
      }
      let i = 0;
      for(const k in relays){
        const v = relays[k];
        const label = (names && names[i]) ? names[i] : k;
        html += `<div class="slot"><div>${label}</div><div><input type="checkbox" id="${dev}_${k}" ${v==1?'checked':''} onchange="setRelay('${dev}','${k}', this.checked)"></div></div>`;
        i++;
      }
      html += `</div><hr><h4>RS232</h4>
               <input id="${dev}_cmd" placeholder=":06030401210120" style="width:60%"/>
               <input id="${dev}_timeout" type="number" value="500" style="width:90px"/>
               <button onclick="sendRS('${dev}')">Send</button>
               <div style="margin-top:8px"><strong>Reply:</strong><pre id="${dev}_rs">`+(obj.last_rs232||'—')+`</pre></div>`;
      card.innerHTML = html;
      devDiv.appendChild(card);
    }
  }catch(e){ console.error(e); }
}

function setRelay(dev, relayKey, checked){
  const val = checked ? 1 : 0;
  fetch(`/api/set?dev=${encodeURIComponent(dev)}&relay=${encodeURIComponent(relayKey)}&val=${val}`)
    .then(()=>setTimeout(refresh,300));
}

function requestTemp(dev){
  fetch(`/api/request_temp?dev=${encodeURIComponent(dev)}`).then(()=>setTimeout(refresh,500));
}

function sendRS(dev){
  const cmd = document.getElementById(dev+'_cmd').value.trim();
  const timeout = parseInt(document.getElementById(dev+'_timeout').value) || 500;
  if(!cmd) return alert('Bitte Befehl eingeben');
  fetch('/api/rs232', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({dev:dev, cmd:cmd, timeout:timeout})})
    .then(()=>setTimeout(refresh,800));
}

setInterval(refresh,1500);
refresh();
</script>
</body>
</html>
'''

@app.route('/')
def index():
    return render_template_string(INDEX_HTML)

@app.route('/api/state')
def api_state():
    return jsonify(devices)

@app.route('/api/set')
def api_set():
    dev = request.args.get('dev')
    relay = request.args.get('relay')  # e.g. "r0"
    val = request.args.get('val')
    if not dev or relay is None or val is None:
        return "missing", 400
    # publish to rpi/<dev>/relay/set payload "r0:1"
    payload = f"{relay}:{val}"
    topic = f"rpi/{dev}/relay/set"
    client.publish(topic, payload)
    return "ok"

@app.route('/api/request_temp')
def api_request_temp():
    dev = request.args.get('dev')
    if not dev:
        return "missing", 400
    client.publish(f"rpi/{dev}/request/temp", "now")
    return "ok"

@app.route('/api/rs232', methods=['POST'])
def api_rs232():
    j = request.json or {}
    dev = j.get('dev')
    cmd = j.get('cmd','').strip()
    timeout = int(j.get('timeout',500))
    if not dev or not cmd:
        return "missing", 400
    payload = f"{cmd}|{timeout}"
    client.publish(f"rpi/{dev}/rs232/send", payload)
    return "ok"

if __name__ == '__main__':
    # kurz warten, damit MQTT-Thread starten kann
    time.sleep(1)
    app.run(host='0.0.0.0', port=8080, debug=False)
