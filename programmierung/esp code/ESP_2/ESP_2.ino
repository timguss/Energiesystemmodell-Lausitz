/*
 * ESP2 — 4x Relay + Temperatur (NTC)
 * Kommunikation: ESP-NOW (kein WiFi-Stack nötig)
 *
 * Beim ersten Start: Serial Monitor öffnen und die Zeile
 * "ESP-NOW MAC: AA:BB:CC:DD:EE:FF" ablesen → in ESP_Host.ino eintragen!
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <math.h>

// ============================================================================
// RELAY KONFIGURATION
// ============================================================================
const int relayPins[]       = {26, 25, 33, 32};
const char* relayNames[]    = {"Kühler-Kohle", "Ventil Turbine", "Ventil-Kohle", "Heizstab-Kohle"};
const int   RELAY_COUNT     = 4;

// Aktive-LOW Relays
void setRelay(int idx, int val) {
  if (idx < 0 || idx >= RELAY_COUNT) return;
  digitalWrite(relayPins[idx], val ? LOW : HIGH);
}
int getRelay(int idx) {
  if (idx < 0 || idx >= RELAY_COUNT) return 0;
  return digitalRead(relayPins[idx]) == LOW ? 1 : 0;
}

// ============================================================================
// TEMPERATUR (NTC)
// ============================================================================
const int   adcPin = 34;
const float Vcc = 3.3, Rf = 10000.0, R0 = 10000.0, T0_K = 298.15, beta = 3950.0;
float cachedTempC = NAN, cachedRntc = NAN;
unsigned long lastTempMillis = 0;

float readR_NTC() {
  float v = (analogRead(adcPin) / 4095.0f) * Vcc;
  if (v <= 0.0001f) return 1e9f;
  if (v >= Vcc - 0.0001f) return 1e-6f;
  return Rf * (v / (Vcc - v));
}
float ntcToCelsius(float R) { return 1.0f / ((1.0f/T0_K) + (1.0f/beta)*logf(R/R0)) - 273.15f; }

// ============================================================================
// ESP-NOW STRUKTUREN
// ============================================================================
typedef struct __attribute__((packed)) {
  char    cmd[12];
  int16_t idx;
  int16_t val;
  int16_t extra;
  char    payload[64];
} CmdMsg;

typedef struct __attribute__((packed)) {
  char    device[8];
  int16_t relays[8];
  float   sensors[5];
  float   temp;
  float   flow;
  int16_t pwm;
  int8_t  forward;
  int8_t  running;
  int8_t  relay_count;
  int8_t  sensor_count;
} StatusMsg;

// ============================================================================
// STATUS SENDEN
// ============================================================================
uint8_t BROADCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void sendStatus() {
  StatusMsg msg;
  memset(&msg, 0, sizeof(msg));
  strlcpy(msg.device, "esp2", sizeof(msg.device));
  msg.relay_count  = RELAY_COUNT;
  msg.sensor_count = 0;
  for (int i = 0; i < RELAY_COUNT; i++)
    msg.relays[i] = getRelay(i);
  msg.temp = cachedTempC;

  esp_now_send(BROADCAST, (uint8_t*)&msg, sizeof(msg));
}

// ============================================================================
// BEFEHLE EMPFANGEN
// ============================================================================
void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != sizeof(CmdMsg)) return;
  CmdMsg msg;
  memcpy(&msg, data, sizeof(msg));

  if (strcmp(msg.cmd, "RELAY") == 0) {
    setRelay(msg.idx, msg.val);
    Serial.printf("Relay %d (%s) → %s\n", msg.idx, relayNames[msg.idx], msg.val ? "AN" : "AUS");
    sendStatus();
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  // Relais initialisieren — alle AUS (Active LOW → HIGH)
  for (int i = 0; i < RELAY_COUNT; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }

  // ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  Serial.printf("ESP-NOW MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Init fehlgeschlagen!");
    return;
  }
  esp_now_register_recv_cb(onReceive);

  // Broadcast-Peer für Status-Meldungen an Host
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 1;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.println("ESP2 bereit (ESP-NOW)");
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();

  // Temperatur alle 1 Sekunde
  if (now - lastTempMillis >= 1000) {
    lastTempMillis = now;
    cachedRntc  = readR_NTC();
    cachedTempC = ntcToCelsius(cachedRntc);
  }

  // Heartbeat alle 2 Sekunden
  static unsigned long lastHeartbeat = 0;
  if (now - lastHeartbeat >= 2000) {
    lastHeartbeat = now;
    sendStatus();
  }
}