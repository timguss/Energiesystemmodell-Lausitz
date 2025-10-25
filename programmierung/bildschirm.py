#!/usr/bin/env python3
# pi_relay_ctrl.py
# Steuerung einzelner Relais (seriell, keine parallelen Schaltaktionen)
# Mapping global -> (device, device_idx)
# 1..8  -> esp1 r0..r7
# 9..12 -> esp2 r0..r3

import requests
import json
import sys
import time

HOST = "http://192.168.4.1"   # ESP-Host
TIMEOUT = 8

GLOBAL_MAP = {
    1: ("esp1", 0),  2: ("esp1", 1),  3: ("esp1", 2),  4: ("esp1", 3),
    5: ("esp1", 4),  6: ("esp1", 5),  7: ("esp1", 6),  8: ("esp1", 7),
    9: ("esp2", 0), 10: ("esp2", 1), 11: ("esp2", 2), 12: ("esp2", 3),
}

def forward(target, method, path, body=""):
    payload = {"target": target, "method": method, "path": path, "body": body}
    headers = {"Content-Type": "application/json"}
    r = requests.post(HOST + "/forward", data=json.dumps(payload), headers=headers, timeout=TIMEOUT)
    r.raise_for_status()
    return r.json()  # expects {"code":int,"body":"..."} per Host implementation

def set_relay_global(global_idx, val, retry=1):
    if global_idx not in GLOBAL_MAP:
        raise ValueError(f"Unknown relay index: {global_idx}")
    device, idx = GLOBAL_MAP[global_idx]
    path = f"/set?idx={idx}&val={1 if val else 0}"
    last_exc = None
    for attempt in range(retry+1):
        try:
            resp = forward(device, "GET", path)
            code = resp.get("code", -1)
            body = resp.get("body","")
            ok = (200 <= code < 300) or (body.strip()=="ok")
            return {"ok": ok, "code": code, "body": body}
        except Exception as e:
            last_exc = e
            time.sleep(0.2)
    raise last_exc

def get_state_global(global_idx):
    if global_idx not in GLOBAL_MAP:
        raise ValueError(f"Unknown relay index: {global_idx}")
    device, idx = GLOBAL_MAP[global_idx]
    resp = forward(device, "GET", "/state")
    return resp

def list_clients():
    r = requests.get(HOST + "/clients", timeout=TIMEOUT)
    r.raise_for_status()
    return r.json()

def print_usage():
    print("Usage:")
    print("  pi_relay_ctrl.py list-clients")
    print("  pi_relay_ctrl.py set <global_idx 1-12> <0|1>")
    print("  pi_relay_ctrl.py state <global_idx 1-12>")
    print("  pi_relay_ctrl.py bulk <file.json>   # sequential actions from json array")
    print("")
    print("bulk file format example:")
    print('[{"idx":1,"val":1},{"idx":1,"val":0},{"idx":10,"val":1}]')

def main(argv):
    if len(argv) < 2:
        print_usage(); return 1
    cmd = argv[1]
    try:
        if cmd == "list-clients":
            print(json.dumps(list_clients(), indent=2, ensure_ascii=False))
            return 0
        if cmd == "set":
            if len(argv) != 4: print_usage(); return 1
            g = int(argv[2]); v = int(argv[3])
            res = set_relay_global(g, 1 if v else 0)
            print(json.dumps(res, indent=2, ensure_ascii=False))
            return 0
        if cmd == "state":
            if len(argv) != 3: print_usage(); return 1
            g = int(argv[2])
            res = get_state_global(g)
            print(json.dumps(res, indent=2, ensure_ascii=False))
            return 0
        if cmd == "bulk":
            if len(argv) != 3: print_usage(); return 1
            fname = argv[2]
            with open(fname, "r", encoding="utf-8") as f:
                actions = json.load(f)
            results = []
            for a in actions:
                idx = int(a["idx"]); val = int(a["val"])
                # sequential execution, short delay to avoid collisions
                r = set_relay_global(idx, val)
                results.append({"action": a, "result": r})
                time.sleep(0.05)
            print(json.dumps(results, indent=2, ensure_ascii=False))
            return 0
    except Exception as e:
        print("error:", str(e))
        return 2

if __name__ == "__main__":
    sys.exit(main(sys.argv))
