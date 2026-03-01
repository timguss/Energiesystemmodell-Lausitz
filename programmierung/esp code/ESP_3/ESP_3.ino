// ESP3_api_ledc_new.ino
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "ESP-HOST";
const char* WIFI_PASS = "espHostPass";
const IPAddress HOST_IP(192,168,4,1);

const int windPin = 27; // Logic for wind relay
// const int ledPin = 2; // Unused

// -------------------- RELAIS --------------------
struct RelayConfig {
  uint8_t pin;
  const char* name;
  bool activeLow;
};

// ESP3 uses the same relay config as ESP4 Relays 2-5
const int RELAY_COUNT = 4;
RelayConfig RELAYS[RELAY_COUNT] = {
  {19, "Relais 2", true}, 
  {21, "Relais 3", true},
  {22, "Relais 4", true},
  {23, "Relais 5", true}
};

// Polarity helpers
inline int logicalToPhys(int logical, bool activeLow){
  return activeLow ? (logical==1 ? LOW : HIGH) : (logical==1 ? HIGH : LOW);
}
inline int physToLogical(int phys, bool activeLow){
  return activeLow ? (phys==LOW ? 1 : 0) : (phys==HIGH ? 1 : 0);
}

bool isRunning = false; // Legacy "Wind" state
const unsigned long blinkOnTime = 300;
const unsigned long blinkOffTime = 1000;
unsigned long lastBlinkTime = 0;
bool ledState = false;

// -------------------- MOTOR (Train) --------------------
#define ENA 32
#define IN1 25
#define IN2 26

#define PWM_FREQ  1000
#define PWM_RES   8

int pwmValue = 0;
bool motorForward = true;

WebServer server(80);

void registerAtHost(){
  if(WiFi.status()!=WL_CONNECTED) return;
  HTTPClient http;
  String url = String("http://") + HOST_IP.toString() + "/register";
  http.begin(url);
  http.addHeader("Content-Type","application/json");
  http.setTimeout(2000); // Prevents hanging if host is missing
  String payload = "{\"name\":\"esp3\",\"ip\":\""+WiFi.localIP().toString()+"\"}";
  http.POST(payload);
  http.end();
}

// Motor control logic
void setMotor(int speed, bool forward) {
  pwmValue = constrain(speed, 0, (1 << PWM_RES) - 1);
  motorForward = forward;

  if (motorForward) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  }
  ledcWrite(ENA, pwmValue);
  Serial.printf("Motor %s, speed: %d/255\n", motorForward ? "FORWARD" : "BACKWARD", pwmValue);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  ledcWrite(ENA, 0);
  pwmValue = 0;
  Serial.println("Motor STOPPED");
}

void handleState(){
  String s = "{\"running\":" + String(isRunning?"true":"false") + ",";
  s += "\"pwm\":" + String(pwmValue) + ",";
  s += "\"forward\":" + String(motorForward?"true":"false") + ",";
  s += "\"relays\":[";
  for(int i=0; i<RELAY_COUNT; i++){
    s += String(physToLogical(digitalRead(RELAYS[i].pin), RELAYS[i].activeLow));
    if(i < RELAY_COUNT - 1) s += ",";
  }
  s += "]}";
  server.send(200,"application/json", s);
}

void handleSet(){
  // 1. Direct Relay Control (Dashboard Switches)
  if(server.hasArg("idx")){
    int idx = server.arg("idx").toInt();
    if(!server.hasArg("val")){ server.send(400,"text/plain","missing val"); return; }
    int val = server.arg("val").toInt();
    
    if(idx >= 0 && idx < RELAY_COUNT){
      digitalWrite(RELAYS[idx].pin, logicalToPhys(val, RELAYS[idx].activeLow));
      server.send(200,"text/plain","ok");
      return;
    }
  }

  // 2. Legacy Wind Control (Dashboard AN/AUS Buttons)
  if(server.hasArg("val")){
    int v = server.arg("val").toInt();
    isRunning = (v != 0);
    // motorPin disabled to prevent interference with Relais 4
    // digitalWrite(motorPin, logicalToPhys(isRunning ? 1 : 0, RELAYS[2].activeLow));
    server.send(200,"text/plain","ok");
    return;
  }
  
  server.send(400,"text/plain","missing args");
}

void handleMeta(){
  String s = "{\"count\":" + String(RELAY_COUNT) + ",\"names\":[";
  for(int i=0; i<RELAY_COUNT; i++){
    s += "\"" + String(RELAYS[i].name) + "\"";
    if(i < RELAY_COUNT - 1) s += ",";
  }
  s += "]}";
  server.send(200,"application/json", s);
}

void handleTrainSet(){
  if(!server.hasArg("pwm")){ server.send(400,"text/plain","missing pwm"); return; }
  int v = server.arg("pwm").toInt();
  bool dir = motorForward; // Keep current direction if not specified
  if(server.hasArg("dir")) dir = (server.arg("dir").toInt() != 0);
  
  setMotor(v, dir);
  server.send(200,"text/plain","ok");
}

void handleTrainStop(){
  stopMotor();
  server.send(200,"text/plain","ok");
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info){
  if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP){
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    // NOTE: Don't call registerAtHost here, loop handles it
  }
}

void setup(){
  Serial.begin(115200);

  // Initialize all relay pins to OFF
  for(int i=0; i<RELAY_COUNT; i++){
    pinMode(RELAYS[i].pin, OUTPUT);
    int physOff = logicalToPhys(0, RELAYS[i].activeLow);
    digitalWrite(RELAYS[i].pin, physOff); 
  }
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(windPin, OUTPUT);
  digitalWrite(windPin, LOW);

  if(!ledcAttach(ENA, PWM_FREQ, PWM_RES)){
    Serial.println("ledcAttach failed");
  }
  stopMotor();

  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // More stable than manual reconnect in event
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  server.on("/state", HTTP_GET, handleState);
  server.on("/set",   HTTP_GET, handleSet);
  server.on("/meta",  HTTP_GET, handleMeta);
  server.on("/train", HTTP_GET, handleTrainSet);
  server.on("/train/stop", HTTP_GET, handleTrainStop);
  server.begin();

  Serial.println("ESP3 API ready (Relays 2-5 & Legacy Wind)");
}

void loop(){
  server.handleClient();
  
  // Wind control on pin 27
  static bool lastWindState = false;
  if(isRunning != lastWindState){
    lastWindState = isRunning;
    digitalWrite(windPin, isRunning ? HIGH : LOW);
    Serial.printf("Wind (Pin 27) %s\n", isRunning ? "ON" : "OFF");
  }

  static unsigned long lastReg = 0;
  if(millis() - lastReg > 20000){ // Check every 20s
    lastReg = millis();
    if (WiFi.status() == WL_CONNECTED) {
      registerAtHost();
    } else {
      Serial.println("WiFi not connected, skipping reg...");
    }
  }
}
