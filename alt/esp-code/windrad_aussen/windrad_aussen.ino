/* esp_windrad_simple.ino
   Windrad (no button)
   Motor -> GPIO32
   LED   -> GPIO33

   MQTT:
     publishes: esp/wind1/relays    -> JSON {"r0":0,"r1":1}
     publishes: esp/wind1/meta      -> {"relays":["Motor","LED"]} (retained)
     subscribes: rpi/wind1/set      -> payload "idx:val" e.g. "0:1" (motor on)
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include "Credentials.h"   // must exist in your Arduino libraries as discussed

// Device settings
const char* DEVICE_ID = "wind1";

// Hardware pins
const int motorPin = 32;
const int ledPin   = 33;

// Blink timing (while motor running)
const unsigned long blinkOnTime  = 300;
const unsigned long blinkOffTime = 1000;
unsigned long lastBlinkTime = 0;
bool ledBlinkState = false;

// MQTT / network
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 1000; // ms

bool motorRunning = false; // r0
bool ledOn        = false; // r1 (can be controlled directly or follow blink when motor running)

// --- helpers ---
void publishRelays(bool retained=false){
  String j = "{";
  j += "\"r0\":" + String(motorRunning?1:0) + ",";
  j += "\"r1\":" + String(ledOn?1:0);
  j += "}";
  String topic = String("esp/") + DEVICE_ID + "/relays";
  mqtt.publish(topic.c_str(), j.c_str(), retained);
}

void publishMeta(){
  String j = "{\"relays\":[\"Motor\",\"LED\"]}";
  String topic = String("esp/") + DEVICE_ID + "/meta";
  mqtt.publish(topic.c_str(), j.c_str(), true); // retained
}

// Apply outputs to pins
void applyOutputs(){
  digitalWrite(motorPin, motorRunning ? HIGH : LOW);
  digitalWrite(ledPin, ledOn ? HIGH : LOW);
}

// Handle incoming command "idx:val" where idx 0=Motor, 1=LED
void handleCommand(const String &cmd){
  int colon = cmd.indexOf(':');
  if(colon <= 0) return;
  int idx = cmd.substring(0,colon).toInt();
  int val = cmd.substring(colon+1).toInt();

  if(idx == 0){
    motorRunning = (val == 1);
    Serial.printf("CMD -> Motor %s\n", motorRunning ? "ON" : "OFF");
    // when motor turned off, turn led off unless explicitly set later
    if(!motorRunning) {
      // keep ledOn as-is (user might have set it) — comment/uncomment depending desired behavior
      // ledOn = false;
    } else {
      // start blink cycle
      lastBlinkTime = millis();
      ledBlinkState = true;
      ledOn = true; // immediate visual feedback
    }
  } else if(idx == 1){
    ledOn = (val == 1);
    Serial.printf("CMD -> LED %s\n", ledOn ? "ON" : "OFF");
  }

  applyOutputs();
  publishRelays(true); // retained state
}

// MQTT callback
void mqttCallback(char* topic, byte* payload, unsigned int length){
  String msg;
  for(unsigned int i=0;i<length;i++) msg += (char)payload[i];
  String t = String(topic);
  Serial.printf("MQTT in: %s -> %s\n", t.c_str(), msg.c_str());
  String expect = String("rpi/") + DEVICE_ID + "/set";
  if(t == expect) handleCommand(msg);
}

void connectMQTT(){
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  while(!mqtt.connected()){
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.reconnect();
      delay(500);
      continue;
    }
    Serial.print("Connecting to MQTT... ");
    if(mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS)){
      Serial.println("connected");
      // subscribe for commands
      String t = String("rpi/") + DEVICE_ID + "/set";
      mqtt.subscribe(t.c_str());
      // publish meta + retained state
      publishMeta();
      publishRelays(true);
    } else {
      Serial.print("failed rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

void setup_wifi(){
  Serial.print("WiFi connecting to: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempt = 0;
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
    attempt++;
    if(attempt > 40){
      Serial.println("\nWiFi timeout, restarting...");
      ESP.restart();
    }
  }
  Serial.println();
  Serial.print("WiFi connected. IP=");
  Serial.println(WiFi.localIP());
}

void setup(){
  Serial.begin(115200);
  delay(50);
  Serial.println("=== ESP Windrad (simple) starting ===");

  pinMode(motorPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(motorPin, LOW);
  digitalWrite(ledPin, LOW);

  setup_wifi();
  connectMQTT();

  motorRunning = false;
  ledOn = false;
  lastBlinkTime = millis();

  publishMeta();
  publishRelays(true);
  applyOutputs();
}

void loop(){
  mqtt.loop();
  if(!mqtt.connected()) connectMQTT();

  // Blink LED when motor running (unless LED explicitly forced)
  if(motorRunning){
    unsigned long now = millis();
    unsigned long interval = ledBlinkState ? blinkOnTime : blinkOffTime;
    if(now - lastBlinkTime >= interval){
      lastBlinkTime = now;
      ledBlinkState = !ledBlinkState;
      // Only allow blink to control LED if user didn't manually switch LED off/on.
      // Choice: let blink control ledOn state; here we set ledOn to blink state.
      ledOn = ledBlinkState;
      digitalWrite(ledPin, ledOn ? HIGH : LOW);
    }
  }

  // ensure motor output matches state
  digitalWrite(motorPin, motorRunning ? HIGH : LOW);

  // periodic publish (not retained) to indicate alive + current actual states
  if(millis() - lastPublish >= PUBLISH_INTERVAL){
    lastPublish = millis();
    publishRelays(false);
    // optional heartbeat
    String hbTopic = String("esp/") + DEVICE_ID + "/status";
    mqtt.publish(hbTopic.c_str(), "alive");
    Serial.printf("Status publish: Motor=%d LED=%d\n", motorRunning?1:0, ledOn?1:0);
  }
}
