// ESP3_windraeder_zug.ino
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// ===== Network (Host-AP) =====
const char* WIFI_SSID = "ESP-HOST";
const char* WIFI_PASS = "espHostPass";
const IPAddress HOST_IP(192,168,4,1); // Host (AP) IP

// ===== Windrad Pins & Timing =====
const int ledPin = 26;    // LED
const int motorPin = 22;  // Motor (HIGH = drehen)
const int buttonPin = 21; // Taster (INPUT_PULLUP)

bool isRunning = false;
bool lastStableButtonState = HIGH;
bool lastReadButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

const unsigned long blinkOnTime = 300;
const unsigned long blinkOffTime = 1000;

unsigned long lastBlinkTime = 0;
bool ledState = false;

// ===== Zug (PWM) =====
const int pwmPin = 25;
const int pwmFreq = 5000;     // Hz
const int pwmResolution = 8;  // bits (0-255)
const int pwmChannel = 0;
int pwmValue = 0;

// ===== Webserver =====
WebServer server(80);

// ===== Helpers =====
void registerAtHost(){
  if(WiFi.status()!=WL_CONNECTED) return;
  HTTPClient http;
  String url = String("http://") + HOST_IP.toString() + "/register";
  http.begin(url);
  http.addHeader("Content-Type","application/json");
  String payload = "{\"name\":\"esp3\",\"ip\":\""+WiFi.localIP().toString()+"\"}";
  http.POST(payload);
  http.end();
}

void applyStateToHardware(){
  if(isRunning){
    digitalWrite(motorPin, HIGH);
  } else {
    digitalWrite(motorPin, LOW);
    digitalWrite(ledPin, LOW);
    ledState = false;
  }
}

void setTrainPWM(int v){
  v = constrain(v, 0, (1<<pwmResolution)-1);
  pwmValue = v;
  ledcWrite(pwmChannel, pwmValue);
}

// ===== HTTP Handlers =====
// GET /state  -> {"running":true,"pwm":123}
void handleState(){
  String s = "{\"running\":" + String(isRunning ? "true":"false") + ",\"pwm\":" + String(pwmValue) + "}";
  server.send(200,"application/json", s);
}

// GET /set?val=1 or 0  -> windrad on/off
void handleSet(){
  if(!server.hasArg("val")) { server.send(400,"text/plain","missing val"); return; }
  int v = server.arg("val").toInt();
  bool newState = (v != 0);
  if(newState != isRunning){
    isRunning = newState;
    applyStateToHardware();
    registerAtHost(); // update host lastSeen
  }
  server.send(200,"text/plain","ok");
}

// POST /toggle  -> toggles windrad
void handleToggle(){
  isRunning = !isRunning;
  applyStateToHardware();
  registerAtHost();
  handleState();
}

// GET /train?pwm=0..255  -> set pwm value
void handleTrainSet(){
  if(!server.hasArg("pwm")){ server.send(400,"text/plain","missing pwm"); return; }
  int v = server.arg("pwm").toInt();
  setTrainPWM(v);
  server.send(200,"text/plain","ok");
}

// GET /train/stop  -> stop train (pwm=0)
void handleTrainStop(){
  setTrainPWM(0);
  server.send(200,"text/plain","ok");
}

// serve simple web UI for train control and wind state
String htmlPage(){
  String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>ESP3 - Wind & Zug</title>";
  html += "<style>body{font-family:Arial;padding:14px} .card{background:#fff;padding:12px;border-radius:8px;margin-bottom:10px;box-shadow:0 1px 4px rgba(0,0,0,0.08)}</style>";
  html += "</head><body>";
  html += "<div class='card'><h2>Windrad</h2>";
  html += "<div>State: <span id='wind'>--</span></div>";
  html += "<button onclick='toggleWind()'>Toggle</button>";
  html += "</div>";
  html += "<div class='card'><h2>Zug PWM</h2>";
  html += "<p>Wert: <span id='val'>"+String(pwmValue)+"</span></p>";
  html += "<input type='range' min='0' max='"+String((1<<pwmResolution)-1)+"' id='slider' value='"+String(pwmValue)+"' oninput='updatePWM(this.value)'>";
  html += "<div style='margin-top:8px'><button onclick='stopTrain()'>Stop</button></div>";
  html += "</div>";
  html += "<script>";
  html += "async function fetchState(){ let r = await fetch('/state'); let j = await r.json(); document.getElementById('wind').innerText = j.running? 'AN':'AUS'; document.getElementById('val').innerText = j.pwm; document.getElementById('slider').value = j.pwm; }";
  html += "function toggleWind(){ fetch('/toggle',{method:'POST'}).then(fetchState); }";
  html += "function updatePWM(v){ document.getElementById('val').innerText = v; fetch('/train?pwm='+v); }";
  html += "function stopTrain(){ fetch('/train/stop').then(()=>{document.getElementById('val').innerText='0'; document.getElementById('slider').value=0;}); }";
  html += "setInterval(fetchState,1500); fetchState();";
  html += "</script></body></html>";
  return html;
}

void handleRoot(){
  server.send(200,"text/html", htmlPage());
}

// ===== WiFi Events =====
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){
  if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP){
    Serial.print("WIFI connected, IP: "); Serial.println(WiFi.localIP());
    registerAtHost();
  }
  if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
    Serial.println("WIFI disconnected, reconnecting...");
    WiFi.reconnect();
  }
}

// ===== Setup / Loop =====
void setup(){
  Serial.begin(115200);

  // wind pins
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // train PWM setup (proper ledc API)
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(pwmPin, pwmChannel);
  setTrainPWM(0);

  // WiFi
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<8000){ delay(200); Serial.print("."); }
  Serial.println();
  if(WiFi.status()==WL_CONNECTED) registerAtHost();

  // server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/state", HTTP_GET, handleState);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/train", HTTP_GET, handleTrainSet);
  server.on("/train/stop", HTTP_GET, handleTrainStop);
  server.begin();

  applyStateToHardware();
}

void loop(){
  server.handleClient();

  // Button debounce + toggle (LOW = pressed)
  bool reading = digitalRead(buttonPin);
  if(reading != lastReadButtonState){
    lastDebounceTime = millis();
  }
  if((millis() - lastDebounceTime) > debounceDelay){
    if(reading != lastStableButtonState){
      lastStableButtonState = reading;
      if(reading == LOW){
        isRunning = !isRunning;
        applyStateToHardware();
        Serial.println(isRunning ? "Anlage EIN" : "Anlage AUS");
        registerAtHost();
      }
    }
  }
  lastReadButtonState = reading;

  // Blink & motor control (only when running)
  if(isRunning){
    unsigned long now = millis();
    unsigned long interval = ledState ? blinkOnTime : blinkOffTime;
    if(now - lastBlinkTime >= interval){
      lastBlinkTime = now;
      ledState = !ledState;
      digitalWrite(ledPin, ledState ? HIGH : LOW);
    }
    // motor stays HIGH
  } else {
    digitalWrite(motorPin, LOW);
    digitalWrite(ledPin, LOW);
  }

  // periodic re-register to update host lastSeen
  static unsigned long lastReg = 0;
  if(millis() - lastReg > 60000){
    lastReg = millis();
    registerAtHost();
  }
}
