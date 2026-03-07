#!/usr/bin/env python3
# routes/scenarios.py — Blueprint for scenario listing and execution

import time
from flask import Blueprint, jsonify, request
from config import GLOBAL_MAP, get_relay_id, load_scenarios
from esp_client import host_get, host_forward

scenarios_bp = Blueprint('scenarios', __name__)

# Module-level scenarios dict, reloaded on each request
_scenarios = load_scenarios()


@scenarios_bp.route("/api/scenarios")
def api_scenarios():
    """Returns all available scenarios. Reloads scenarios.json on every call."""
    global _scenarios
    _scenarios = load_scenarios()
    return jsonify({"scenarios": _scenarios})


@scenarios_bp.route("/api/scenario/execute", methods=["POST"])
def api_scenario_execute():
    """Executes a scenario synchronously so errors are reported immediately."""
    global _scenarios
    _scenarios = load_scenarios()

    j = request.get_json(force=True)
    scenario_name = j.get("scenario")

    if not scenario_name:
        return jsonify({"error": "Kein Szenario angegeben"}), 400

    if scenario_name not in _scenarios:
        # Legacy fallback: forward directly to Host-ESP
        state = int(j.get("state", 0))
        print(f"[INFO] Fallback executing legacy scenario: {scenario_name}, state: {state}")
        try:
            path = f"/scenario?name={scenario_name}&state={state}"
            result = host_get(path, timeout=10)
            return jsonify({"success": True, "scenario": scenario_name, "state": state, "host_response": result})
        except Exception as e:
            return jsonify({"error": str(e)}), 502

    scenario_data = _scenarios[scenario_name]
    actions = scenario_data.get("actions", [])

    print("\n" + "="*50)
    print(f"[SCENARIO START] '{scenario_name}' ({len(actions)} Aktionen)")
    print("="*50)

    for idx, action in enumerate(actions):
        try:
            atype = action.get("type")
            prefix = f"[Aktion {idx+1}/{len(actions)} - {atype.upper()}]"

            if atype == "delay":
                ms = int(action.get("ms", 1000))
                print(f"{prefix} Warte {ms}ms ... ", end="", flush=True)
                time.sleep(ms / 1000.0)
                print("OK")

            elif atype == "relay":
                identifier = action.get("name") or action.get("relay") or action.get("global_idx")
                gi = get_relay_id(identifier)
                val = 1 if int(action.get("val")) else 0
                state_str = "AN" if val else "AUS"
                print(f"{prefix} Schalte '{identifier}' -> {state_str} ... ", end="", flush=True)

                if gi is not None:
                    if gi in GLOBAL_MAP:
                        device, dev_idx = GLOBAL_MAP[gi]
                        path = f"/set?idx={dev_idx}&val={val}"
                        host_forward(device, "GET", path, timeout=10)
                        print("OK")
                    else:
                        print("FEHLER")
                        raise Exception(f"Device mapping not found for global index {gi}.")
                else:
                    print("FEHLER")
                    raise Exception(f"Relay mapping not found for '{identifier}'.")

            elif atype == "wind":
                val_w = 1 if int(action.get("val", 0)) else 0
                state_str = "AN" if val_w else "AUS"
                print(f"{prefix} Wind -> {state_str} ... ", end="", flush=True)
                host_forward("esp3", "GET", f"/set?val={val_w}", timeout=10)
                print("OK")

            elif atype == "train":
                pwm = int(action.get("pwm", 0))
                dir_val = int(action.get("dir", 1))
                print(f"{prefix} Zug -> PWM:{pwm} DIR:{dir_val} ... ", end="", flush=True)
                host_forward("esp3", "GET", f"/train?pwm={pwm}&dir={dir_val}", timeout=10)
                print("OK")

        except Exception as e:
            err_msg = f"Failed at action {idx}: {e}"
            print(f"FEHLER: {str(e)}")
            print(f"[SCENARIO ERROR] Abbruch bei Aktion {idx+1}.")
            return jsonify({"error": err_msg, "scenario": scenario_name}), 502

    print(f"[SCENARIO ERFOLG] '{scenario_name}' abgeschlossen.\n")
    return jsonify({
        "success": True,
        "scenario": scenario_name,
        "actions_count": len(actions)
    })
