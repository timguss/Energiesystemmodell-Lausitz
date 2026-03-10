#!/usr/bin/env python3
# routes/debug.py — Blueprint for debug and utility API endpoints

from flask import Blueprint, jsonify
from config import HOST
from esp_client import host_get

debug_bp = Blueprint('debug', __name__)


@debug_bp.route("/api/debug/host")
def api_debug_host():
    """Checks if the Host-ESP is reachable and returns registered clients."""
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

    # #region agent log
    try:
        import json, time
        with open("debug-fbab11.log", "a", encoding="utf-8") as f:
            f.write(json.dumps({
                "sessionId": "fbab11",
                "runId": "pre-fix",
                "hypothesisId": "H2",
                "location": "routes/debug.py:api_debug_host",
                "message": "Backend host debug check",
                "data": {
                    "host": result.get("host"),
                    "host_reachable": result.get("host_reachable"),
                    "has_clients": bool(result.get("clients")),
                    "error": result.get("error")
                },
                "timestamp": int(time.time() * 1000)
            }) + "\n")
    except Exception:
        pass
    # #endregion

    return jsonify(result)


@debug_bp.route("/api/clients")
def api_clients():
    try:
        return jsonify(host_get("/clients", timeout=5))
    except Exception as e:
        print(f"[ERROR] /api/clients: {str(e)}")
        return jsonify({"error": str(e)}), 502
