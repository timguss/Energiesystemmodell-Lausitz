from flask import Flask, render_template
import paho.mqtt.client as mqtt
import threading, time, json

# --- Config ---
MQTT_SERVER = "192.168.4.1"   # IP des Pi
MQTT_USER = "modelltisch1"
MQTT_PASS = "strukturwandel"
HEARTBEAT_TIMEOUT = 10  # Sekunden, um aktive ESPs zu erkennen

app = Flask(__name__)
esp_data = {}  # deviceID -> {relays: [...], temp: .., last_seen: timestamp}

# --- MQTT Callbacks ---
def on_connect(client, userdata, flags, rc):
    print("✅ Connected to MQTT server" if rc==0 else f"❌ MQTT connect failed {rc}")
    client.subscribe("esp/+/+")

def on_message(client, userdata, msg):
    topic = msg.topic.split('/')
    if len(topic) != 3: return
    _, device, field = topic
    payload = msg.payload.decode()
    
    if device not in esp_data:
        esp_data[device] = {}
    
    if field == "relays":
        try:
            esp_data[device]['relays'] = json.loads(payload)
        except:
            esp_data[device]['relays'] = payload
    elif field == "temp":
        esp_data[device]['temp'] = payload
    
    esp_data[device]['last_seen'] = time.time()

# --- MQTT Thread ---
def mqtt_thread():
    client = mqtt.Client("rpi_web")
    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_SERVER)
    client.loop_forever()

# --- Flask Routes ---
@app.route("/")
def index():
    now = time.time()
    # Nur ESPs, die innerhalb von HEARTBEAT_TIMEOUT gesendet haben
    active_esps = {k:v for k,v in esp_data.items() if now - v.get('last_seen',0) < HEARTBEAT_TIMEOUT}
    return render_template("index.html", devices=active_esps)

# --- Main ---
if __name__ == "__main__":
    threading.Thread(target=mqtt_thread, daemon=True).start()
    app.run(host='0.0.0.0', port=80)
