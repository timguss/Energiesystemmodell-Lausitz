// ESP3_api_ledc_new.ino
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "ESP-HOST";
const char* WIFI_PASS = "espHostPass";
const IPAddress HOST_IP(192,168,4,1);

const int ledPin = 26;
const int motorPin = 22;

bool isRunning = false;
const unsigned long blinkOnTime = 300;
const unsigned long blinkOffTime = 1000;
unsigned long lastBlinkTime = 0;
bool ledState = false;

const int pwmPin = 25;            // Pin direkt für neue LEDC-API
const int pwmFreq = 5000;         // Hz
const int pwmResolution = 8;      // bits (0-255)
int pwmValue = 0;

WebServer server(80);

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
  if(isRunning) digitalWrite(motorPin, HIGH);
  else { digitalWrite(motorPin, LOW); digitalWrite(ledPin, LOW); ledState=false; }
}

// Neue LEDC-API: attach pin with frequency and resolution, then write using pin
void setTrainPWM(int v){
  v = constrain(v, 0, (1<<pwmResolution)-1);
  pwmValue = v;
  ledcWrite(pwmPin, pwmValue); // new API: use pin as identifier
}

void handleState(){
  String s = "{\"running\":" + String(isRunning?"true":"false") + ",\"pwm\":" + String(pwmValue) + "}";
  server.send(200,"application/json", s);
}

void handleSet(){
  if(!server.hasArg("val")){ server.send(400,"text/plain","missing val"); return; }
  int v = server.arg("val").toInt();
  bool newState = (v!=0);
  if(newState!=isRunning){
    isRunning=newState;
    applyStateToHardware();
    registerAtHost();
  }
  server.send(200,"text/plain","ok");
}

void handleTrainSet(){
  if(!server.hasArg("pwm")){ server.send(400,"text/plain","missing pwm"); return; }
  int v = server.arg("pwm").toInt();
  setTrainPWM(v);
  server.send(200,"text/plain","ok");
}

void handleTrainStop(){
  setTrainPWM(0);
  server.send(200,"text/plain","ok");
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){
  if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP){
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    registerAtHost();
  } else if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
    WiFi.reconnect();
  }
}

void setup(){
  Serial.begin(115200);

  pinMode(motorPin, OUTPUT); digitalWrite(motorPin, LOW);
  pinMode(ledPin, OUTPUT); digitalWrite(ledPin, LOW);

  // Neue LEDC-API: ledcAttach(pin, freq, resolution)
  // Einige Cores implementieren ledcAttach(pin, freq, resolution)
  // Falls dein Core eine andere Signatur hat, ersetze durch passende API.
  if(!ledcAttach(pwmPin, pwmFreq, pwmResolution)){
    Serial.println("ledcAttach returned false or not supported on this core. Trying fallback.");
    // Fallback: try old API style (channel-based) if available
    // Note: keep compilation safe — the old API calls are behind compile-time checks in some cores.
    // If fallback isn't available on your core remove these lines and use the core's recommended API.
    // ledcSetup(0, pwmFreq, pwmResolution);
    // ledcAttachPin(pwmPin, 0);
  }
  // ensure 0 pwm at start
  setTrainPWM(0);

  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);

  server.on("/state", HTTP_GET, handleState);
  server.on("/set",   HTTP_GET, handleSet);
  server.on("/train", HTTP_GET, handleTrainSet);
  server.on("/train/stop", HTTP_GET, handleTrainStop);
  server.begin();

  applyStateToHardware();
  Serial.println("ESP3 API ready");
}

void loop(){
  server.handleClient();
  if(isRunning){
    unsigned long now = millis();
    unsigned long interval = ledState ? blinkOnTime : blinkOffTime;
    if(now - lastBlinkTime >= interval){
      lastBlinkTime = now;
      ledState = !ledState;
      digitalWrite(ledPin, ledState?HIGH:LOW);
    }
  } else {
    digitalWrite(motorPin, LOW);
    digitalWrite(ledPin, LOW);
  }

  static unsigned long lastReg = 0;
  if(millis() - lastReg > 60000){
    lastReg = millis();
    registerAtHost();
  }
}
