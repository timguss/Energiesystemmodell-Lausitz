#!/usr/bin/env python3
# app.py — Flask UI: Scenarios are executed on ESP Host (DEBUG VERSION)

from flask import Flask, jsonify, request, render_template
import requests
import json
import time

# ============================================================================
# CONFIGURATION
# ============================================================================

HOST = "http://192.168.4.1"   # Host-ESP
HTTP_TIMEOUT = 10  # Timeout in Sekunden
USE_MOCK_DATA = False # Set to False for real hardware connection
VERBOSE = False # Set to True for detailed debug logging

# region agent log
import os

DEBUG_SESSION_ID = "3ab92e"
# IMPORTANT: Workspace root (for this debug session) is the git project root.
# We hardcode the absolute path so logs always go to the correct location.
DEBUG_LOG_PATH = r"c:\Users\User\Documents\GitHub\Energiesystemmodell-Lausitz\debug-3ab92e.log"


def agent_log(run_id: str, hypothesis_id: str, location: str, message: str, data: dict) -> None:
    """
    Minimal NDJSON logger for debug session instrumentation.
    Writes one JSON object per line to DEBUG_LOG_PATH.
    """
    try:
        ts_ms = int(time.time() * 1000)
        entry = {
            "sessionId": DEBUG_SESSION_ID,
            "id": f"log_{ts_ms}",
            "timestamp": ts_ms,
            "location": location,
            "message": message,
            "data": data,
            "runId": run_id,
            "hypothesisId": hypothesis_id,
        }
        with open(DEBUG_LOG_PATH, "a", encoding="utf-8") as f:
            f.write(json.dumps(entry) + "\n")
    except Exception:
        # Instrumentation must never break main logic
        pass
# endregion

# Status tracking to reduce terminal spam
OFFLINE_DEVICES = set()

def report_status(device, is_online, error_msg=None):
    """Prints status only when it changes"""
    if is_online:
        if device in OFFLINE_DEVICES:
            print(f"[ONLINE] {device} is back online")
            OFFLINE_DEVICES.remove(device)
    else:
        if device not in OFFLINE_DEVICES:
            print(f"[OFFLINE] {device} is missing/unreachable: {error_msg}")
            OFFLINE_DEVICES.add(device)

# Mapping global relay index -> (device, device_idx)
GLOBAL_MAP = {
    1: ("esp1", 0),  2: ("esp1", 1),  3: ("esp1", 2),  4: ("esp1", 3),
    5: ("esp1", 4),  6: ("esp1", 5),  7: ("esp1", 6),  8: ("esp1", 7),
    9: ("esp2", 0), 10: ("esp2", 1), 11: ("esp2", 2), 12: ("esp2", 3),
    13: ("esp4", 0), 14: ("esp4", 1), 15: ("esp4", 2), 16: ("esp4", 3), 17: ("esp4", 4),
    18: ("esp3", 0), 19: ("esp3", 1), 20: ("esp3", 2), 21: ("esp3", 3),
}

# ============================================================================
# VERFÜGBARE SZENARIEN
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
        if VERBOSE: print(f"[DEBUG] GET {url}")
        r = requests.get(url, timeout=timeout)
        if VERBOSE: print(f"[DEBUG] Response status: {r.status_code}")
        r.raise_for_status()
        report_status("host", True)
        try:
            result = r.json()
            if VERBOSE: print(f"[DEBUG] Response JSON: {result}")
            return result
        except:
            if VERBOSE: print(f"[DEBUG] Response TEXT: {r.text}")
            return r.text
    except requests.exceptions.Timeout:
        report_status("host", False, "Timeout")
        raise Exception("Timeout: Host antwortet nicht")
    except requests.exceptions.RequestException as e:
        report_status("host", False, str(e))
        raise Exception(f"Verbindungsfehler: {str(e)}")

def host_post(path, data=None, timeout=HTTP_TIMEOUT):
    """POST request to Host-ESP"""
    try:
        url = HOST + path
        headers = {"Content-Type": "application/json"}
        if VERBOSE: print(f"[DEBUG] POST {url}")
        if VERBOSE: print(f"[DEBUG] Data: {data}")
        r = requests.post(url, data=data, headers=headers, timeout=timeout)
        if VERBOSE: print(f"[DEBUG] Response status: {r.status_code}")
        r.raise_for_status()
        report_status("host", True)
        try:
            result = r.json()
            if VERBOSE: print(f"[DEBUG] Response JSON: {result}")
            return result
        except:
            if VERBOSE: print(f"[DEBUG] Response TEXT: {r.text}")
            return r.text
    except requests.exceptions.Timeout:
        report_status("host", False, "Timeout (POST)")
        raise Exception("Timeout: Host antwortet nicht")
    except requests.exceptions.RequestException as e:
        report_status("host", False, f"Request Error (POST): {str(e)}")
        raise Exception(f"Verbindungsfehler: {str(e)}")

def host_forward(target, method, path, body_str="", timeout=HTTP_TIMEOUT):
    """
    Mit ESP-NOW:
    - GET /state  → liest vom Host-Cache: GET /state/<target>
    - GET /meta   → liest vom Host-Cache: GET /state/<target> (Meta ist eingebettet)
    - GET /set    → sendet Befehl über Host: POST /forward
    - andere      → POST /forward wie bisher
    """
    if VERBOSE: print(f"[DEBUG] Forward to {target}: {method} {path}")

    try:
        # State-Abfragen: direkt aus Host-Cache lesen (ESP-NOW Push)
        if method == "GET" and path == "/state":
            resp = host_get(f"/state/{target}", timeout=timeout)
            if isinstance(resp, dict):
                return resp  # Host gibt bereits {code:200, body:{...}} zurück
            return {"code": 200, "body": resp}

        # Relay/Motor-Befehle: per /forward an Host senden
        payload = {"target": target, "method": method, "path": path, "body": body_str}
        resp = host_post("/forward", json.dumps(payload), timeout=timeout)
        if VERBOSE: print(f"[DEBUG] Forward response RAW: {repr(resp)}")

        if isinstance(resp, dict):
            code = resp.get("code", -1)
            body = resp.get("body", "")
            if isinstance(body, str) and body.strip():
                try:
                    return {"code": code, "body": json.loads(body)}
                except json.JSONDecodeError:
                    pass
            return {"code": code, "body": body}

        return {"code": -1, "body": str(resp)}
    except Exception as e:
        raise e

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
# DEBUG ROUTE
# ============================================================================

@app.route("/api/debug/host")
def api_debug_host():
    """Prüft ob Host erreichbar ist und welche Clients registriert sind"""
    result = {
        "host": HOST,
        "host_reachable": False,
        "clients": None,
        "error": None
    }
    
    try:
        clients = host_get("/clients", timeout=5)
        result["host_reachable"] = True
        result["clients"] = clients
    except Exception as e:
        result["error"] = str(e)
    
    return jsonify(result)

# ============================================================================
# API ROUTES
# ============================================================================

@app.route("/api/clients")
def api_clients():
    try:
        return jsonify(host_get("/clients", timeout=5))
    except Exception as e:
        print(f"[ERROR] /api/clients: {str(e)}")
        return jsonify({"error": str(e)}), 502

@app.route("/api/device_meta/<device>")
def api_device_meta(device):
    if VERBOSE: print(f"\n[INFO] === Request: /api/device_meta/{device} ===")
    
    # HARDCODED FALLBACKS (wenn ESP offline)
    fallback_meta = {}
    if device == "esp1":
        fallback_meta = {"count": 8, "names": ["Ventil - 1", "Ventil - 2", "Heizstab", "Zünder", "Gasventil", "Kühler", "MFC - Reserve", "Unbelegt"]}
    elif device == "esp2":
        fallback_meta = {"count": 4, "names": ["Reserve 1", "Reserve 2", "Reserve 3", "Reserve 4"]}
    elif device == "esp3":
        fallback_meta = {"count": 4, "names": ["Relais 2", "Relais 3", "Relais 4", "Relais 5"]}
    elif device == "esp4":
        fallback_meta = {
            "count": 5,
            "names": ["Relay 1", "Relay 2", "Relay 3", "Relay 4", "Relay 5"]
        }
    
    try:
        # Cache check
        now = time.time()
        if device in meta_cache and (now - meta_cache_time.get(device, 0)) < 60:
            return jsonify(meta_cache[device])
        
        if VERBOSE: print(f"[INFO] Fetching fresh meta for {device}")
        res = host_forward(device, "GET", "/meta", timeout=10)
        
        if res.get("code") == 200:
            report_status(device, True)
            meta_cache[device] = res
            meta_cache_time[device] = now
            return jsonify(res)
        else:
            raise Exception(f"Device returned code {res.get('code')}")

    except Exception as e:
        report_status(device, False, str(e))
        return jsonify({"code": 200, "body": fallback_meta, "offline": True})

# ============================================================================
# ESP4 SENSOR CALIBRATION (Einfach hier anpassen!)
# ============================================================================
# Formel: ((current_mA - I_ZERO) / (I_FULL - I_ZERO)) * MAX_VAL
ESP4_SENSOR_CALIBRATION = [
    {"type": "pressure",   "i_zero": 2.88, "i_full": 18.88, "max_val": 4.0},   # Sensor 1 (Index 0)
    {"type": "pressure",   "i_zero": 3.00, "i_full": 19.19, "max_val": 4.0},   # Sensor 2 (Index 1)
    {"type": "pressure",   "i_zero": 2.98, "i_full": 18.98, "max_val": 6.0},   # Sensor 3 (Index 2)
    {"type": "ultrasonic", "i_zero": 4.00, "i_full": 20.0, "min_val": 2.0, "max_val": 30.0}, # Sensor 4 (Index 3)
    {"type": "ultrasonic", "i_zero": 3.08, "i_full": 20.0, "min_val": 2.0, "max_val": 30.0}  # Sensor 5 (Index 4)
]

@app.route("/api/device_state/<device>")
def api_device_state(device):
    if VERBOSE: print(f"[INFO] Request: /api/device_state/{device}")
    
    # HARDCODED FALLBACKS (Empty/Offline State)
    fallback_state = {}
    if device == "esp1":
        fallback_state = {"r0":0,"r1":0,"r2":0,"r3":0,"r4":0,"r5":0,"r6":0,"r7":0,"temp":None,"rntc":None}
    elif device == "esp2":
        fallback_state = {"r0":0,"r1":0,"r2":0,"r3":0,"temp":None,"rntc":None}
    elif device == "esp3":
        fallback_state = {"running": False, "pwm": 0, "relays": [0, 0, 0, 0]}
    elif device == "esp4":
        fallback_state = {
            "relays": [0,0,0,0,0],
            "sensors": [
                {"current":0,"pressure":0},
                {"current":0,"pressure":0},
                {"current":0,"pressure":0},
                {"current":0,"distance":0}, # Modified for ultrasonic
                {"current":0,"distance":0}, # Modified for ultrasonic
            ],
            "flow": 0
        }
        
    try:
        res = host_forward(device, "GET", "/state", timeout=10)
        if res.get("code") == 200:
            report_status(device, True)
            
            # --- ESP4 SENSOR CALIBRATION CALCULATION ---
            from typing import Dict, Any, List, cast
            body_obj = res.get("body")
            if device == "esp4" and isinstance(body_obj, dict):
                body_dict = cast(Dict[str, Any], body_obj)
                sensors_obj = body_dict.get("sensors")
                relays_obj = body_dict.get("relays")

                # Agent instrumentation: capture esp4 relay array when state is fetched (H3)
                try:
                    agent_log(
                        run_id="pre-fix",
                        hypothesis_id="H3",
                        location="app.py:api_device_state",
                        message="esp4 state relays snapshot",
                        data={
                            "relays": list(relays_obj) if isinstance(relays_obj, list) else relays_obj,
                        },
                    )
                except Exception:
                    pass
                
                if isinstance(sensors_obj, list):
                    sensors = cast(List[Dict[str, Any]], sensors_obj)
                    
                    for i in range(min(5, len(sensors))):
                        cal = ESP4_SENSOR_CALIBRATION[i]
                        cal_i_zero = float(cal.get("i_zero", 3.19))
                        cal_i_full = float(cal.get("i_full", 20.0))
                        
                        sensor_data = sensors[i]
                        if isinstance(sensor_data, dict):
                            current_mA = float(sensor_data.get("current", 0))
                            
                            # Calculate percentage based on current
                            if current_mA <= cal_i_zero:
                                pct = 0.0
                            elif current_mA >= cal_i_full:
                                pct = 1.0
                            else:
                                pct = (current_mA - cal_i_zero) / (cal_i_full - cal_i_zero)
                                
                            # Apply specific type limits and labels
                            if cal.get("type") == "pressure":
                                max_val = float(cal.get("max_val", 4.0))
                                val = float(pct * max_val)
                                sensor_data["pressure"] = round(val, 2)
                            elif cal.get("type") == "ultrasonic":
                                max_val = float(cal.get("max_val", 30.0))
                                min_val = float(cal.get("min_val", 2.0))
                                val = float((pct * (max_val - min_val)) + min_val)
                                # We use 'pressure' key in backend to keep frontend app.js compatible without massive rewrites 
                                # but we can pass 'distance' for clarity if wanted, though keeping 'pressure' is safer
                                sensor_data["pressure"] = round(val, 2)
            
            if VERBOSE: print(f"[INFO] State response for {device}: {res}")
            return jsonify(res)
        else:
            raise Exception(f"Device returned code {res.get('code')}")
            
    except Exception as e:
        report_status(device, False, str(e))
        return jsonify({"code": 200, "body": fallback_state, "offline": True})

@app.route("/api/relay/set", methods=["POST"])
def api_relay_set():
    j = request.get_json(force=True)
    gi = int(j.get("global_idx"))
    val = 1 if int(j.get("val")) else 0
    if gi not in GLOBAL_MAP:
        return jsonify({"error": "invalid index"}), 400
    device, idx = GLOBAL_MAP[gi]
    path = f"/set?idx={idx}&val={val}"
    print(f"[INFO] Relay set: global={gi}, device={device}, idx={idx}, val={val}")

    # Agent instrumentation: log every esp4 relay toggle request (H1/H2)
    try:
        if device == "esp4":
            agent_log(
                run_id="pre-fix",
                hypothesis_id="H1",
                location="app.py:api_relay_set",
                message="esp4 relay toggle requested",
                data={
                    "global_idx": gi,
                    "device_idx": idx,
                    "val": val,
                    "path": path,
                },
            )
    except Exception:
        pass

    try:
        res = host_forward(device, "GET", path, timeout=10)

        # Agent instrumentation: capture esp4 relay toggle low-level result (H2)
        try:
            if device == "esp4":
                agent_log(
                    run_id="pre-fix",
                    hypothesis_id="H2",
                    location="app.py:api_relay_set",
                    message="esp4 relay toggle host_forward response",
                    data={
                        "response_code": res.get("code") if isinstance(res, dict) else None,
                    },
                )
        except Exception:
            pass

        return jsonify(res)
    except Exception as e:
        print(f"[ERROR] /api/relay/set: {str(e)}")
        return jsonify({"error": str(e)}), 502

# ============================================================================
# SZENARIEN API
# ============================================================================

@app.route("/api/scenarios")
def api_scenarios():
    """Liste aller verfügbaren Szenarien (aus lokaler Config)"""
    return jsonify({"scenarios": SCENARIOS})

@app.route("/api/scenario/execute", methods=["POST"])
def api_scenario_execute():
    """Führt ein Szenario aus - wird an ESP-Host weitergeleitet"""
    j = request.get_json(force=True)
    scenario_name = j.get("scenario")
    state = int(j.get("state", 0))
    
    print(f"[INFO] Executing scenario: {scenario_name}, state: {state}")
    
    if not scenario_name:
        return jsonify({"error": "Kein Szenario angegeben"}), 400
    
    if scenario_name not in SCENARIOS:
        return jsonify({"error": f"Unbekanntes Szenario: {scenario_name}"}), 400
    
    try:
        path = f"/scenario?name={scenario_name}&state={state}"
        result = host_get(path, timeout=10)
        
        return jsonify({
            "success": True,
            "scenario": scenario_name,
            "state": state,
            "host_response": result
        })
        
    except Exception as e:
        print(f"[ERROR] Scenario execution failed: {str(e)}")
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
    print(f"[INFO] RS232 command: {cmd}")
    try:
        res = host_forward("esp1", "POST", "/send", inner, timeout=5)
        return jsonify(res)
    except Exception as e:
        print(f"[ERROR] /api/rs232: {str(e)}")
        return jsonify({"error": str(e)}), 502

# ESP3 endpoints
@app.route("/api/esp3/state")
def api_esp3_state():
    try:
        res = host_forward("esp3", "GET", "/state", timeout=10)
        if res.get("code") == 200:
            report_status("esp3", True)
            return jsonify(res)
        else:
            raise Exception(f"ESP3 returned code {res.get('code')}")
    except Exception as e:
        report_status("esp3", False, str(e))
        return jsonify({"code": 200, "body": {"running": False, "pwm": 0}, "offline": True})

@app.route("/api/esp3/set_wind", methods=["POST"])
def api_esp3_set_wind():
    j = request.get_json(force=True)
    val = 1 if int(j.get("val", 0)) else 0
    try:
        return jsonify(host_forward("esp3", "GET", f"/set?val={val}", timeout=10))
    except Exception as e:
        print(f"[ERROR] /api/esp3/set_wind: {str(e)}")
        return jsonify({"error": str(e)}), 502

@app.route("/api/esp3/set_pwm", methods=["POST"])
def api_esp3_set_pwm():
    j = request.get_json(force=True)
    pwm = int(j.get("pwm", 0))
    pwm = max(0, min(255, pwm))
    direction = int(j.get("dir", 1)) # Default to Forward (1)

    try:
        return jsonify(host_forward("esp3", "GET", f"/train?pwm={pwm}&dir={direction}", timeout=10))
    except Exception as e:
        print(f"[ERROR] /api/esp3/set_pwm: {str(e)}")
        return jsonify({"error": str(e)}), 502

# ============================================================================
# FRONTEND ROUTE
# ============================================================================

@app.route("/")
def landing():
    return render_template("landing.html")

@app.route("/dashboard")
def dashboard():
    return render_template("dashboard.html", host=HOST)

# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    print("=" * 60)
    print("Flask UI gestartet (DEBUG MODE)")
    print("=" * 60)
    print(f"Host ESP: {HOST}")
    print(f"Debug Endpoint: http://localhost:8000/api/debug/host")
    print("=" * 60)
    app.run(host="0.0.0.0", port=8000, debug=True)