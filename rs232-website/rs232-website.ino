/*
  ESP32 ProPar Web-Interface - STA (kein AP)
  - verbindet mit normalem WLAN (Station mode)
  - startet WebUI auf Port 80
  - initialisiert Serial2 (UART2) NUR NACHDEM WLAN verbunden ist
  - sendet eingegebenen ProPar-ASCII-Strings an Serial2 und liest Antwort
  - Serielle Ausgabe: 115200 Baud
  Anpassungen: WIFI_SSID / WIFI_PASS, ggf. RX/TX Pins ändern
*/

#include <WiFi.h>
#include <WebServer.h>
#include <esp_event.h>

///// Benutzerkonfiguration /////
const char* WIFI_SSID = "Fritz-Box-SG";               
const char* WIFI_PASS = "floppy1905Frettchen";       

const int UART2_RX_PIN = 16; // ESP RX  <- Modul TX
const int UART2_TX_PIN = 17; // ESP TX  -> Modul RX
const unsigned long DEFAULT_REPLY_TIMEOUT = 500; // ms
/////////////////////////////////

WebServer server(80);
HardwareSerial MySerial(2); // UART2
bool serial2Started = false;

// HTML Web UI (einfach)
const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 ProPar RS232 (STA)</title>
<style>
body{font-family:system-ui,Arial; padding:12px; background:#f5f7fb}
.card{background:#fff;padding:12px;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,0.06)}
input,button{font-size:15px;padding:8px}
input[type=text]{width:100%;font-family:monospace}
pre{background:#111;color:#0f0;padding:10px;border-radius:6px;overflow:auto}
label{font-weight:600}
</style>
</head>
<body>
<div class="card">
  <h2>ProPar RS232 — Senden & Empfangen</h2>
  <p>Beispiel: <code>:06030401210120</code></p>

  <label for="cmd">Befehl (ASCII-Hex):</label><br>
  <input id="cmd" type="text" placeholder=":06030401210120">

  <div style="margin-top:8px">
    <label for="timeout">Timeout (ms):</label>
    <input id="timeout" type="number" value="500" style="width:120px">
  </div>

  <div style="margin-top:12px">
    <button id="sendBtn">Send</button>
  </div>

  <h3 style="margin-top:14px">Protokoll</h3>
  <div class="card">
    <div><strong>Sent:</strong><pre id="sent">—</pre></div>
    <div style="margin-top:8px"><strong>Reply:</strong><pre id="reply">—</pre></div>
  </div>
</div>

<script>
document.getElementById('sendBtn').addEventListener('click', async () => {
  const cmd = document.getElementById('cmd').value.trim();
  const timeout = parseInt(document.getElementById('timeout').value) || 500;
  if (!cmd) { alert('Bitte Befehl eingeben.'); return; }
  document.getElementById('sent').textContent = cmd;
  document.getElementById('reply').textContent = 'sende...';
  try {
    const r = await fetch('/send', {
      method: 'POST',
      headers: {'Content-Type': 'text/plain'},
      body: JSON.stringify({cmd: cmd, timeout: timeout})
    });
    const txt = await r.text();
    document.getElementById('reply').textContent = txt || '(keine Antwort)';
  } catch (e) {
    document.getElementById('reply').textContent = 'Fehler: ' + e;
  }
});
</script>
</body>
</html>
)rawliteral";

// Hilfsfunktion: liest verfügbare Bytes bis timeout_ms (blockierend)
String readSerialResponse(unsigned long timeout_ms = DEFAULT_REPLY_TIMEOUT) {
  unsigned long start = millis();
  String s = "";
  while (millis() - start < timeout_ms) {
    while (MySerial.available()) {
      char c = (char)MySerial.read();
      s += c;
    }
    // kein delay, sonst Zeichenverlust möglich
  }
  return s;
}

// HTTP: Index
void handleIndex() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "text/html", index_html);
}

// HTTP: /send (POST)
void handleSend() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Kein Body empfangen");
    return;
  }
  String body = server.arg("plain");
  String cmd = "";
  unsigned long timeout = DEFAULT_REPLY_TIMEOUT;

  int pCmd = body.indexOf("\"cmd\"");
  if (pCmd >= 0) {
    int col = body.indexOf(':', pCmd);
    int q1 = body.indexOf('"', col+1);
    int q2 = body.indexOf('"', q1+1);
    if (q1 >= 0 && q2 > q1) cmd = body.substring(q1+1, q2);
  }
  int pT = body.indexOf("\"timeout\"");
  if (pT >= 0) {
    int col = body.indexOf(':', pT);
    int endpos = body.indexOf('}', col+1);
    if (endpos > col) {
      String tstr = body.substring(col+1, endpos);
      tstr.trim();
      timeout = (unsigned long)tstr.toInt();
    }
  }

  if (cmd.length() == 0) {
    server.send(400, "text/plain", "Kein cmd im Body gefunden");
    return;
  }

  // CR+LF anhängen
  if (!cmd.endsWith("\r\n")) cmd += "\r\n";

  if (!serial2Started) {
    server.send(500, "text/plain", "Serial2 nicht initialisiert (WLAN noch nicht verbunden).");
    return;
  }

  // Senden & Debug
  Serial.print("[Web] Sent: "); Serial.println(cmd);
  MySerial.print(cmd);
  String resp = readSerialResponse(timeout);
  Serial.print("[Web] Reply: "); Serial.println(resp);

  if (resp.length() == 0) server.send(200, "text/plain", "(keine Antwort innerhalb Timeout)");
  else server.send(200, "text/plain", resp);
}

// WiFi Event
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WIFI] connected. IP: ");
      Serial.println(WiFi.localIP());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WIFI] disconnected. Reconnect...");
      WiFi.reconnect();
      break;
    default:
      break;
  }
}

void startServerHandlers() {
  server.on("/", HTTP_GET, handleIndex);
  server.on("/send", HTTP_POST, handleSend);
  server.begin();
  Serial.println("HTTP server started (handler registered).");
}

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("\n=== ESP32 ProPar RS232 (STA mode) starting ===");

  WiFi.onEvent(onWiFiEvent);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to WiFi SSID: %s ... (blocking wait up to 15s)\n", WIFI_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WARN: Nicht verbunden (Server startet trotzdem).");
  }

  startServerHandlers();

  // Serial2 erst jetzt starten
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Initializing Serial2 (UART2) NOW");
    MySerial.begin(38400, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
    serial2Started = true;
    Serial.println("Serial2 initialized (RX=" + String(UART2_RX_PIN) + ", TX=" + String(UART2_TX_PIN) + ")");
  } else {
    serial2Started = false;
    Serial.println("Serial2 deferred until WiFi connects.");
  }

  Serial.println("Setup finished.");
}

void loop() {
  server.handleClient();

  if (!serial2Started && WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected later - now initializing Serial2");
    MySerial.begin(38400, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
    serial2Started = true;
    Serial.println("Serial2 initialized (delayed).");
  }

  delay(5);
}
