#!/usr/bin/env python3
# app.py — Flask UI: Scenarios are executed on ESP Host

from flask import Flask, jsonify, request, render_template
import requests
import json
import time

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
# VERFÜGBARE SZENARIEN (nur UI-Beschreibungen)
# Die eigentliche Logik liegt auf dem ESP-Host!
# ============================================================================

SCENARIOS = {
    "kohlekraftwerk": {
        "name": "Kohlekraftwerk",
        "states": [
            {"id": 0, "name": "Aus", "description": "Kohlekraftwerk ausschalten"},
            {"id": 1, "name": "Ein", "description": "Kohlekraftwerk einschalten"},
        ]
    },
    "test": {
        "name": "Test-Sequenz",
        "states": [
            {"id": 0, "name": "Reset", "description": "Alles ausschalten"},
            {"id": 1, "name": "Sequenz 1", "description": "Test-Sequenz starten"},
            {"id": 2, "name": "Sequenz 2", "description": "Alternative Sequenz"},
        ]
    },
    "alles": {
        "name": "Alle Relays",
        "states": [
            {"id": 0, "name": "Alle Aus", "description": "Alle Relays ausschalten"},
            {"id": 1, "name": "Alle An", "description": "Alle Relays einschalten"},
        ]
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
# SZENARIEN API - Leitet an ESP-Host weiter
# ============================================================================

@app.route("/api/scenarios")
def api_scenarios():
    """Liste aller verfügbaren Szenarien (aus lokaler Config)"""
    return jsonify({"scenarios": SCENARIOS})

@app.route("/api/scenario/execute", methods=["POST"])
def api_scenario_execute():
    """
    Führt ein Szenario aus - wird an ESP-Host weitergeleitet
    Erwartet: {"scenario": "kohlekraftwerk", "state": 0}
    ESP-Host Endpoint: GET /scenario?name=kohlekraftwerk&state=0
    """
    j = request.get_json(force=True)
    scenario_name = j.get("scenario")
    state = int(j.get("state", 0))
    
    if not scenario_name:
        return jsonify({"error": "Kein Szenario angegeben"}), 400
    
    # Prüfe ob Szenario bekannt ist (optional)
    if scenario_name not in SCENARIOS:
        return jsonify({"error": f"Unbekanntes Szenario: {scenario_name}"}), 400
    
    # Sende an ESP-Host
    try:
        path = f"/scenario?name={scenario_name}&state={state}"
        result = host_get(path, timeout=10)  # Längeres Timeout für Szenarien
        
        return jsonify({
            "success": True,
            "scenario": scenario_name,
            "state": state,
            "host_response": result
        })
        
    except Exception as e:
        return jsonify({
            "error": str(e),
            "scenario": scenario_name,
            "state": state
        }), 502

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
    print("Szenarien werden auf ESP-Host ausgeführt:")
    print("  Endpoint: GET /scenario?name=<name>&state=<state>")
    app.run(host="0.0.0.0", port=8000, debug=False)