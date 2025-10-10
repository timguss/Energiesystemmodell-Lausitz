#include <WiFi.h>
#include <PubSubClient.h>
#include "Credentials.h"

// ----------------- MQTT / Netzwerk -----------------
WiFiClient espClient;
PubSubClient client(espClient);
const char* deviceID = "esp_wind_train";

// ----------------- Pins -----------------
const int WIND_MOTOR_PIN = 32;
const int WIND_LED_PIN   = 33;

const int TRAIN_PWM_PIN = 25;           // PWM für Zugmotor
const uint32_t PWM_FREQ = 20000;        // 20 kHz
const uint8_t PWM_RES_BITS = 8;         // 0..255
const uint32_t PWM_MAX = 255;

// ----------------- Windrad -----------------
bool windRunning = false;
unsigned long lastBlinkTime = 0;
bool ledState = false;
const unsigned long blinkOnTime = 300;
const unsigned long blinkOffTime = 1000;

// ----------------- Zugmotor -----------------
int currentDuty = 0;
int targetDuty  = 0;
const unsigned long rampMs = 4000;

// ----------------- WiFi / MQTT -----------------
void setup_wifi() {
  Serial.println("🔌 Starte WiFi-Verbindung...");
  Serial.print("   SSID: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int c = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    c++;
    if(c>20){ 
      Serial.println("\n❌ WiFi-Verbindung fehlgeschlagen!");
      Serial.println("🔄 ESP wird neu gestartet...\n");
      delay(1000);
      ESP.restart(); 
    }
  }
  
  Serial.println("\n✅ WiFi erfolgreich verbunden!");
  Serial.print("   IP-Adresse: ");
  Serial.println(WiFi.localIP());
  Serial.print("   Signal-Stärke: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.print("   MAC-Adresse: ");
  Serial.println(WiFi.macAddress());
  Serial.println();
}

void reconnect() {
  while(!client.connected()){
    Serial.print("🔄 MQTT Verbindungsversuch zu ");
    Serial.print(MQTT_SERVER);
    Serial.print(" als '");
    Serial.print(deviceID);
    Serial.print("'...");
    
    if(client.connect(deviceID, MQTT_USER, MQTT_PASS)){
      Serial.println(" ✅ ERFOLGREICH verbunden!");
      String topic = "rpi/" + String(deviceID) + "/#";
      Serial.print("📬 Abonniere Topic: ");
      Serial.println(topic);
      client.subscribe(topic.c_str());
      Serial.println("✅ MQTT vollständig bereit\n");
    }else{
      Serial.print(" ❌ FEHLGESCHLAGEN, rc=");
      Serial.print(client.state());
      Serial.println(" | Wiederhole in 2 Sekunden...");
      delay(2000);
    }
  }
}

// ----------------- PWM Setup (NEUE API) -----------------
void setupPWM() {
  // Neue API: ledcAttach(pin, freq, resolution) 
  // Gibt die Kanalnummer zurück (oder 0 bei Fehler)
  if (ledcAttach(TRAIN_PWM_PIN, PWM_FREQ, PWM_RES_BITS) == 0) {
    Serial.println("❌ PWM Setup fehlgeschlagen!");
  } else {
    Serial.println("✅ PWM Setup erfolgreich");
  }
  ledcWrite(TRAIN_PWM_PIN, 0);  // Start bei 0
}

// ----------------- Rampenfunktion -----------------
void rampToTarget() {
  if(currentDuty == targetDuty) return;

  int stepDir = (targetDuty > currentDuty) ? 1 : -1;
  int steps = abs(targetDuty - currentDuty);
  unsigned long stepDelay = rampMs / steps;
  if(stepDelay==0) stepDelay=1;

  for(int i=0;i<steps;i++){
    currentDuty += stepDir;
    ledcWrite(TRAIN_PWM_PIN, currentDuty);
    delay(stepDelay);
  }

  currentDuty = targetDuty;
  ledcWrite(TRAIN_PWM_PIN, currentDuty);
}

// ----------------- MQTT Callback -----------------
void callback(char* topic, byte* payload, unsigned int length){
  String t = String(topic);
  String msg = "";
  for(int i=0;i<length;i++) msg += (char)payload[i];

  if(t == "rpi/" + String(deviceID) + "/set_wind"){
    if(msg=="1"){ 
      windRunning=true; 
      Serial.println("Windrad EIN"); 
    }
    else{ 
      windRunning=false; 
      Serial.println("Windrad AUS"); 
      digitalWrite(WIND_MOTOR_PIN, LOW); 
      digitalWrite(WIND_LED_PIN, LOW); 
    }
  }
  else if(t == "rpi/" + String(deviceID) + "/set_train"){
    int perc = constrain(msg.toInt(), 0, 100);
    targetDuty = map(perc, 0, 100, 0, PWM_MAX);
    Serial.print("Zug Zielgeschwindigkeit: "); Serial.print(perc); Serial.println("%");
  }
}

// ----------------- Setup -----------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("🚂 ESP32 Wind & Train Controller");
  Serial.println("========================================\n");

  Serial.println("📍 Initialisiere Pins...");
  pinMode(WIND_MOTOR_PIN, OUTPUT);
  pinMode(WIND_LED_PIN, OUTPUT);
  digitalWrite(WIND_MOTOR_PIN, LOW);
  digitalWrite(WIND_LED_PIN, LOW);
  Serial.println("✅ Pins initialisiert\n");

  setup_wifi();
  
  Serial.print("🌐 Konfiguriere MQTT Server: ");
  Serial.println(MQTT_SERVER);
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
  Serial.println("✅ MQTT konfiguriert\n");

  setupPWM();
  
  Serial.println("\n========================================");
  Serial.println("✅ Setup abgeschlossen - System bereit!");
  Serial.println("========================================\n");
}

// ----------------- Loop -----------------
void loop() {
  if(!client.connected()) reconnect();
  client.loop();

  // --- Windrad Steuerung ---
  if(windRunning){
    digitalWrite(WIND_MOTOR_PIN, HIGH);
    unsigned long now = millis();
    unsigned long interval = ledState ? blinkOnTime : blinkOffTime;
    if(now - lastBlinkTime >= interval){
      lastBlinkTime = now;
      ledState = !ledState;
      digitalWrite(WIND_LED_PIN, ledState ? HIGH : LOW);
    }
  } else {
    digitalWrite(WIND_MOTOR_PIN, LOW);
    digitalWrite(WIND_LED_PIN, LOW);
  }

  // --- Zugmotor Ramp ---
  rampToTarget();

  delay(50);
}
