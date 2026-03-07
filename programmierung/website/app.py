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

# Central configuration for all relays across all ESPs
RELAY_CONFIG = {
    1: {"device": "esp1", "idx": 0, "name": "Ventil - 1"},
    2: {"device": "esp1", "idx": 1, "name": "Ventil - 2"},
    3: {"device": "esp1", "idx": 2, "name": "Heizstab"},
    4: {"device": "esp1", "idx": 3, "name": "Zünder"},
    5: {"device": "esp1", "idx": 4, "name": "Gasventil"},
    6: {"device": "esp1", "idx": 5, "name": "Kühler"},
    7: {"device": "esp1", "idx": 6, "name": "MFC - Reserve"},
    8: {"device": "esp1", "idx": 7, "name": "Unbelegt"},
    
    9: {"device": "esp2", "idx": 0, "name": "Reserve 1"},
    10: {"device": "esp2", "idx": 1, "name": "Reserve 2"},
    11: {"device": "esp2", "idx": 2, "name": "Reserve 3"},
    12: {"device": "esp2", "idx": 3, "name": "Reserve 4"},
    
    13: {"device": "esp4", "idx": 0, "name": "Elekrolyseur"},
    14: {"device": "esp4", "idx": 1, "name": "Tank leeren"},
    15: {"device": "esp4", "idx": 2, "name": "Tank füllen"},
    16: {"device": "esp4", "idx": 3, "name": "Durchschalten"},
    17: {"device": "esp4", "idx": 4, "name": "Lüfter"},
    
    18: {"device": "esp3", "idx": 0, "name": "Relais 2"},
    19: {"device": "esp3", "idx": 1, "name": "Relais 3"},
    20: {"device": "esp3", "idx": 2, "name": "Relais 4"},
    21: {"device": "esp3", "idx": 3, "name": "Relais 5"},
}

# Compatibility mapping global relay index -> (device, device_idx)
GLOBAL_MAP = {k: (v["device"], v["idx"]) for k, v in RELAY_CONFIG.items()}

def get_relay_id(identifier):
    if isinstance(identifier, int):
        return identifier
    if isinstance(identifier, str):
        if identifier.isdigit():
            return int(identifier)
        for k, v in RELAY_CONFIG.items():
            if v["name"].lower() == identifier.lower():
                return k
    return None

# ============================================================================
# VERFÜGBARE SZENARIEN
# ============================================================================

import os
import threading

def load_scenarios():
    filepath = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'scenarios.json')
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception as e:
        print(f"[ERROR] Loading scenarios.json failed: {e}")
        return {}

SCENARIOS = load_scenarios()

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
        # Use strict separators without spaces so ESP_Host's simple indexOf based parser works
        resp = host_post("/forward", json.dumps(payload, separators=(',', ':')), timeout=timeout)
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
    
    # FALLBACKS generated from configuration
    fallback_meta = {"count": 0, "names": []}
    device_relays = [(k, v) for k, v in RELAY_CONFIG.items() if v["device"] == device]
    if device_relays:
        max_idx = max([v["idx"] for k, v in device_relays])
        fallback_meta["count"] = max_idx + 1
        names = ["Unbelegt"] * (max_idx + 1)
        for k, v in device_relays:
            names[v["idx"]] = v["name"]
        fallback_meta["names"] = names
    
    try:
        # Cache check
        now = time.time()
        if device in meta_cache and (now - meta_cache_time.get(device, 0)) < 60:
            return jsonify(meta_cache[device])
        
        if VERBOSE: print(f"[INFO] Using local meta for {device} (ESP-NOW doesn't transmit names)")
        # We rely on the /state endpoint to determine offline status in the UI
        # Here we just return the hardcoded names
        res = {"code": 200, "body": fallback_meta}
        meta_cache[device] = res
        meta_cache_time[device] = now
        return jsonify(res)

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
    """Liste aller verfügbaren Szenarien (aus dynamischer Config)"""
    global SCENARIOS
    SCENARIOS = load_scenarios() # Reload on fetch to easily pick up json changes
    return jsonify({"scenarios": SCENARIOS})

@app.route("/api/scenario/execute", methods=["POST"])
def api_scenario_execute():
    """Führt ein Szenario synchron aus, um Fehler sofort ans Frontend zu melden"""
    global SCENARIOS
    SCENARIOS = load_scenarios()  # Reload on execute
    
    j = request.get_json(force=True)
    scenario_name = j.get("scenario")
    
    if not scenario_name:
        return jsonify({"error": "Kein Szenario angegeben"}), 400
        
    if scenario_name not in SCENARIOS:
        # Fallback for old hardcoded logic if ESP expects it
        state = int(j.get("state", 0))
        print(f"[INFO] Fallback executing legacy scenario: {scenario_name}, state: {state}")
        try:
            path = f"/scenario?name={scenario_name}&state={state}"
            result = host_get(path, timeout=10)
            return jsonify({"success": True, "scenario": scenario_name, "state": state, "host_response": result})
        except Exception as e:
            return jsonify({"error": str(e)}), 502
            
    scenario_data = SCENARIOS[scenario_name]
    actions = scenario_data.get("actions", [])
    
    print(f"[INFO] Scenario started: {scenario_name}")
    for idx, action in enumerate(actions):
        try:
            atype = action.get("type")
            if atype == "delay":
                ms = int(action.get("ms", 1000))
                time.sleep(ms / 1000.0)
            elif atype == "relay":
                identifier = action.get("name") or action.get("relay") or action.get("global_idx")
                gi = get_relay_id(identifier)
                
                if gi is not None:
                    val = 1 if int(action.get("val")) else 0
                    if gi in GLOBAL_MAP:
                        device, dev_idx = GLOBAL_MAP[gi]
                        path = f"/set?idx={dev_idx}&val={val}"
                        print(f"[SCENARIO] Relay cmd -> {device} {path}")
                        host_forward(device, "GET", path, timeout=10)
                else:
                    raise Exception(f"Relay mapping not found for '{identifier}'.")
            elif atype == "wind":
                val = 1 if int(action.get("val", 0)) else 0
                print(f"[SCENARIO] Wind cmd -> {val}")
                host_forward("esp3", "GET", f"/set?val={val}", timeout=10)
            elif atype == "train":
                pwm = int(action.get("pwm", 0))
                dir_val = int(action.get("dir", 1))
                print(f"[SCENARIO] Train cmd -> pwm={pwm} dir={dir_val}")
                host_forward("esp3", "GET", f"/train?pwm={pwm}&dir={dir_val}", timeout=10)
        except Exception as e:
            err_msg = f"Failed at action {idx}: {e}"
            print(f"[ERROR] Scenario {scenario_name} {err_msg}")
            return jsonify({"error": err_msg, "scenario": scenario_name}), 502
            
    print(f"[INFO] Scenario {scenario_name} completed.")
    return jsonify({
        "success": True,
        "scenario": scenario_name,
        "actions_count": len(actions)
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