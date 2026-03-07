#!/usr/bin/env python3
# esp_client.py — Low-level HTTP client for communicating with the ESP Host

import requests
import json
from config import HOST, HTTP_TIMEOUT, VERBOSE, report_status

def host_get(path, timeout=HTTP_TIMEOUT):
    """GET request to Host-ESP."""
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
        except Exception:
            if VERBOSE: print(f"[DEBUG] Response TEXT: {r.text}")
            return r.text
    except requests.exceptions.Timeout:
        report_status("host", False, "Timeout")
        raise Exception("Timeout: Host antwortet nicht")
    except requests.exceptions.RequestException as e:
        report_status("host", False, str(e))
        raise Exception(f"Verbindungsfehler: {str(e)}")


def host_post(path, data=None, timeout=HTTP_TIMEOUT):
    """POST request to Host-ESP."""
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
        except Exception:
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
    Routes a command through the Host-ESP to a target ESP device.
    - GET /state  → reads from Host cache: GET /state/<target>
    - GET /set    → sends command via Host: POST /forward
    """
    if VERBOSE: print(f"[DEBUG] Forward to {target}: {method} {path}")

    try:
        if method == "GET" and path == "/state":
            resp = host_get(f"/state/{target}", timeout=timeout)
            if isinstance(resp, dict):
                return resp
            return {"code": 200, "body": resp}

        payload = {"target": target, "method": method, "path": path, "body": body_str}
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
