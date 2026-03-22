#!/usr/bin/env python3
# routes/leds.py — Blueprint for LED synchronization

from flask import Blueprint, jsonify, request
from config import LED_MAPPING, VERBOSE
from esp_client import host_forward

leds_bp = Blueprint('leds', __name__)

@leds_bp.route("/api/leds/sync", methods=["POST"])
def api_leds_sync():
    """
    Synchronizes the LED strips on ESP5 with the current flow states.
    Expects JSON: {"flows": {"coal": true, "solar": false, ...}}
    """
    data = request.get_json(force=True)
    flows = data.get("flows", {})
    
    if VERBOSE: print(f"[LED] Syncing flows: {flows}")
    
    results = []
    
    for flow_id, state in flows.items():
        if flow_id in LED_MAPPING:
            configs = LED_MAPPING[flow_id]
            if not isinstance(configs, list):
                configs = [configs]
            
            for i, m in enumerate(configs):
                strip = m["strip"]
                start, end = m["range"]
                r, g, b = m["color"]
                
                # If frontend sends an array of flows (for multi-stripe), use the corresponding one
                is_active = False
                if isinstance(state, list):
                    is_active = state[i] if i < len(state) else state[0]
                else:
                    is_active = state
                
                val = 1 if is_active else 0
                path = f"/led?strip={strip}&start={start}&end={end}&val={val}&r={r}&g={g}&b={b}"
                
                try:
                    res = host_forward("esp5", "GET", path, timeout=5)
                    results.append({"flow": f"{flow_id}_{i}", "status": res.get("code")})
                except Exception as e:
                    results.append({"flow": f"{flow_id}_{i}", "error": str(e)})
                
    return jsonify({"success": True, "results": results})

@leds_bp.route("/api/leds/test", methods=["POST"])
def api_leds_test():
    """Sets all LEDs in the mapping to a specific color or off."""
    data = request.get_json(force=True)
    mode = data.get("mode", "off") # "green" or "off"
    
    results = []
    color = (0, 255, 0) if mode == "green" else (0, 0, 0)
    val = 1 if mode == "green" else 0
    
    # We want to clear/set all strips (0-6)
    for s in range(7):
        # We use a broad range to cover most strips
        path = f"/led?strip={s}&start=0&end=120&val={val}&r={color[0]}&g={color[1]}&b={color[2]}"
        try:
            res = host_forward("esp5", "GET", path, timeout=5)
            results.append({"strip": s, "status": res.get("code")})
        except Exception as e:
            results.append({"strip": s, "error": str(e)})
            
    return jsonify({"success": True, "results": results})
