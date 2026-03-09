# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an energy system model (Energiesystemmodell-Lausitz) with:
- **ESP8266/ESP32 hardware controllers** (Arduino firmware)
- **Flask web application** for monitoring and control
- **Central ESP_Host** that routes commands to device ESPs

## Common Commands

### Flask Web Application

```bash
cd programmierung/website
pip install -r requirements.txt
python app.py
```

The web UI runs at `http://localhost:8000`. The dashboard shows all devices and sensors.

### Arduino/ESP Development

- Firmware files are in `programmierung/esp code/`
- Upload using Arduino IDE or PlatformIO
- Key ESP devices:
  - `ESP_Host.ino` — Central hub (IP: 192.168.4.1)
  - `ESP_1.ino` — Gas power plant controller (8 relays + temp)
  - `ESP_2.ino` — Coal power plant controller (4 relays + temp)
  - `ESP_3.ino` — Wind/additional controller (4 relays + PWM)
  - `ESP_4.ino` — Electrolyzer controller (5 relays + pressure/flow sensors)

## Architecture

```
[Flask Web App] <--HTTP--> [ESP_Host] <--Serial/Forward--> [ESP_1, ESP_2, ESP_3, ESP_4]
```

### Communication Flow

1. Flask sends HTTP requests to ESP_Host (`config.py:HOST = "http://192.168.4.1"`)
2. ESP_Host forwards commands to target ESP devices
3. Responses flow back through ESP_Host to Flask

### Key Files

| File | Purpose |
|------|---------|
| `config.py` | Central configuration: relay mappings, sensor configs, calibration, scenarios |
| `esp_client.py` | HTTP client for ESP Host communication |
| `routes/relays.py` | API endpoints for relay/sensor control |
| `routes/scenarios.py` | Execute predefined scenario sequences |
| `scenarios.json` | JSON definitions for automated actions |

### Relay Mapping (from config.py)

- **ESP_1** (relays 1-8): Gas plant — valves, heater, igniter, gas valve, cooler, MFC
- **ESP_2** (relays 9-12): Coal plant — cooler, turbine valve, coal valve, heater
- **ESP_3** (relays 13-16): Test/auxiliary
- **ESP_4** (relays 17-21): Electrolyzer — electrolyzer, tank drain/fill, fan

### Sensor IDs

- 101: Temperature (ESP_1 / gas)
- 102: Temperature (ESP_2 / coal)
- 105-110: Pressure, distance, flow (ESP_4 / electrolyzer)

## API Endpoints

- `GET /api/device_state/<device>` — Get ESP state (relays, sensors)
- `GET /api/device_meta/<device>` — Get device relay names
- `POST /api/relay/set` — Set relay state (`{"global_idx": N, "val": 0/1}`)
- `POST /api/scenario/run` — Execute scenario from scenarios.json

## Debug Mode

Set `VERBOSE = True` in `config.py` to enable detailed logging.

## Hardware Notes

- ESP_Host acts as WiFi access point (192.168.4.1)
- ESP4 uses 4-20mA pressure sensors with calibration in `ESP4_SENSOR_CALIBRATION`
- Some ESP code uses software serial for RS232 communication
