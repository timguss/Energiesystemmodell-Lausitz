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
    return jsonify(result)


@debug_bp.route("/api/clients")
def api_clients():
    try:
        return jsonify(host_get("/clients", timeout=5))
    except Exception as e:
        print(f"[ERROR] /api/clients: {str(e)}")
        return jsonify({"error": str(e)}), 502
