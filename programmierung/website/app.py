#!/usr/bin/env python3
# app.py — Flask UI: grouped sliders (esp1 / esp2) with relay names + Scenarios with delays

from flask import Flask, jsonify, request, render_template
import requests
import json
import time
import threading

# ============================================================================
# CONFIGURATION
# ============================================================================

HOST = "http://192.168.4.1"   # Host-ESP
HTTP_TIMEOUT = 3  # Timeout in Sekunden

# Mapping global relay index -> (device, device_idx)
GLOBAL_MAP = {
    1: ("esp1", 0),  2: ("esp1", 1),  3: ("esp1", 2),  4: ("esp1", 3),
    5: ("esp1", 4),  6: ("esp1", 5),  7: ("esp1", 6),  8: ("esp1", 7),
    9: ("esp2", 0), 10: ("esp2", 1), 11: ("esp2", 2), 12: ("esp2", 3),
}

# ============================================================================
# SZENARIEN: Definiere hier welche Relays geschaltet werden sollen
# ============================================================================

def scenario_kohlekraftwerk_ein():
    """
    Kohlekraftwerk einschalten
    Format: Liste von Tupeln (global_relay_index, wert, delay_sekunden)
    - wert: 1 = ein, 0 = aus
    - delay_sekunden: Wartezeit NACH dieser Aktion (optional, default=0)
    """
    return [
        (1, 1, 0),      # Relay 1 ein, sofort weiter
        (2, 1, 0),      # Relay 2 ein, sofort weiter
        (5, 1, 0),      # Relay 5 ein, sofort weiter
        (9, 1, 5),      # Relay 9 ein, dann 5 Sekunden warten
        (2, 0, 0),      # Relay 2 wieder aus (nach den 5 Sekunden)
    ]

def scenario_kohlekraftwerk_aus():
    """
    Kohlekraftwerk ausschalten
    """
    return [
        (1, 0, 0),
        (2, 0, 0),
        (5, 0, 0),
        (9, 0, 0),
    ]

def scenario_test_verzögerung():
    """
    Test-Szenario mit verschiedenen Delays
    """
    return [
        (1, 1, 2),      # Relay 1 ein, 2 Sek warten
        (2, 1, 3),      # Relay 2 ein, 3 Sek warten
        (3, 1, 2),      # Relay 3 ein, 2 Sek warten
        (1, 0, 0),      # Relay 1 aus
        (2, 0, 0),      # Relay 2 aus
        (3, 0, 0),      # Relay 3 aus
    ]

def scenario_alles_aus():
    """
    Alle Relays ausschalten
    """
    return [(i, 0, 0) for i in range(1, 13)]

# Verfügbare Szenarien registrieren
SCENARIOS = {
    "kohlekraftwerk_ein": {
        "name": "Kohlekraftwerk einschalten",
        "func": scenario_kohlekraftwerk_ein,
        "description": "Schaltet mehrere Relays ein, nach 5s wird Relay 2 wieder aus"
    },
    "kohlekraftwerk_aus": {
        "name": "Kohlekraftwerk ausschalten",
        "func": scenario_kohlekraftwerk_aus,
        "description": "Schaltet alle Kohlekraftwerk-Relays aus"
    },
    "test_verzoegerung": {
        "name": "Test Verzögerung",
        "func": scenario_test_verzögerung,
        "description": "Demo mit verschiedenen Delays"
    },
    "alles_aus": {
        "name": "Alles ausschalten",
        "func": scenario_alles_aus,
        "description": "Schaltet alle 12 Relays aus"
    }
}

# ============================================================================
# HTTP CLIENT FUNCTIONS
# ============================================================================

def host_get(path, timeout=HTTP_TIMEOUT):
    """GET request to Host-ESP"""
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
    """POST request to Host-ESP"""
    try:
        url = HOST + path
        headers = {"Content-Type": "application/json"}
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
    """Forward request through Host-ESP to target device"""
    payload = {"target": target, "method": method, "path": path, "body": body_str}
    resp = host_post("/forward", json.dumps(payload), timeout=timeout)
    
    # resp expected {"code":int, "body":"..."} where body may be a JSON string
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

def set_relay(global_idx, val):
    """Schaltet ein einzelnes Relay"""
    if global_idx not in GLOBAL_MAP:
        raise Exception(f"Ungültiger Relay-Index: {global_idx}")
    
    device, idx = GLOBAL_MAP[global_idx]
    path = f"/set?idx={idx}&val={val}"
    return host_forward(device, "GET", path, timeout=2)

def execute_scenario_thread(scenario_id, actions):
    """Führt Szenario in separatem Thread aus (mit Delays)"""
    print(f"[Scenario] Start: {scenario_id}")
    
    for action in actions:
        # Action kann 2 oder 3 Elemente haben
        if len(action) == 2:
            global_idx, val = action
            delay = 0
        else:
            global_idx, val, delay = action
        
        try:
            print(f"[Scenario] Relay {global_idx} -> {val}")
            set_relay(global_idx, val)
            
            # Delay nach der Aktion
            if delay > 0:
                print(f"[Scenario] Warte {delay} Sekunden...")
                time.sleep(delay)
            else:
                time.sleep(0.05)  # Minimale Pause zwischen Schaltungen
                
        except Exception as e:
            print(f"[Scenario] Error Relay {global_idx}: {e}")
    
    print(f"[Scenario] Fertig: {scenario_id}")

# ============================================================================
# FLASK APP
# ============================================================================

import os

# Explizite Pfade für templates und static
base_dir = os.path.dirname(os.path.abspath(__file__))
template_dir = os.path.join(base_dir, 'templates')
static_dir = os.path.join(base_dir, 'static')

app = Flask(__name__, template_folder=template_dir, static_folder=static_dir)

# Cache für Meta-Daten (ändern sich nicht)
meta_cache = {}
meta_cache_time = {}

# ============================================================================
# API ROUTES
# ============================================================================

@app.route("/api/clients")
def api_clients():
    try:
        return jsonify(host_get("/clients", timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

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
        return jsonify({"error": "invalid index"}), 400
    device, idx = GLOBAL_MAP[gi]
    path = f"/set?idx={idx}&val={val}"
    try:
        return jsonify(host_forward(device, "GET", path, timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

# ============================================================================
# SZENARIEN API
# ============================================================================

@app.route("/api/scenarios")
def api_scenarios():
    """Liste aller verfügbaren Szenarien"""
    return jsonify({
        "scenarios": [
            {
                "id": key, 
                "name": val["name"],
                "description": val.get("description", "")
            } 
            for key, val in SCENARIOS.items()
        ]
    })

@app.route("/api/scenario/execute", methods=["POST"])
def api_scenario_execute():
    """Führt ein Szenario aus (in separatem Thread bei Delays)"""
    j = request.get_json(force=True)
    scenario_id = j.get("scenario_id")
    
    if scenario_id not in SCENARIOS:
        return jsonify({"error": "Unbekanntes Szenario"}), 400
    
    scenario = SCENARIOS[scenario_id]
    actions = scenario["func"]()
    
    # Prüfe ob Delays vorhanden sind
    has_delays = any(len(a) >= 3 and a[2] > 0 for a in actions)
    
    if has_delays:
        # Starte in separatem Thread
        thread = threading.Thread(
            target=execute_scenario_thread,
            args=(scenario_id, actions),
            daemon=True
        )
        thread.start()
        
        return jsonify({
            "scenario": scenario["name"],
            "status": "started",
            "message": "Szenario läuft im Hintergrund",
            "has_delays": True
        })
    else:
        # Sofortige Ausführung ohne Thread
        results = []
        errors = []
        
        for action in actions:
            if len(action) == 2:
                global_idx, val = action
            else:
                global_idx, val, _ = action
            
            try:
                result = set_relay(global_idx, val)
                results.append({
                    "relay": global_idx,
                    "value": val,
                    "success": True
                })
                time.sleep(0.05)
            except Exception as e:
                errors.append({
                    "relay": global_idx,
                    "value": val,
                    "error": str(e)
                })
        
        return jsonify({
            "scenario": scenario["name"],
            "status": "completed",
            "results": results,
            "errors": errors,
            "success": len(errors) == 0,
            "has_delays": False
        })

# RS232 endpoint
@app.route("/api/rs232", methods=["POST"])
def api_rs232():
    j = request.get_json(force=True)
    cmd = j.get("cmd", "")
    timeout = int(j.get("timeout", 500))
    if not cmd:
        return jsonify({"error": "no cmd"}), 400
    inner = json.dumps({"cmd": cmd, "timeout": timeout})
    try:
        res = host_forward("esp1", "POST", "/send", inner, timeout=5)
        return jsonify(res)
    except Exception as e:
        return jsonify({"error": str(e)}), 502

# ESP3 endpoints
@app.route("/api/esp3/state")
def api_esp3_state():
    try:
        return jsonify(host_forward("esp3", "GET", "/state", timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/esp3/set_wind", methods=["POST"])
def api_esp3_set_wind():
    j = request.get_json(force=True)
    val = 1 if int(j.get("val", 0)) else 0
    try:
        return jsonify(host_forward("esp3", "GET", f"/set?val={val}", timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

@app.route("/api/esp3/set_pwm", methods=["POST"])
def api_esp3_set_pwm():
    j = request.get_json(force=True)
    pwm = int(j.get("pwm", 0))
    pwm = max(0, min(255, pwm))
    try:
        return jsonify(host_forward("esp3", "GET", f"/train?pwm={pwm}", timeout=2))
    except Exception as e:
        return jsonify({"error": str(e)}), 502

# ============================================================================
# FRONTEND ROUTE
# ============================================================================

@app.route("/")
def index():
    return render_template("index.html", host=HOST)

# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    print("Starte Flask UI. Host:", HOST)
    app.run(host="0.0.0.0", port=8000, debug=False)