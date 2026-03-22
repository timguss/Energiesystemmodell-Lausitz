#include <WiFi.h>
#include <esp_now.h>
#include <FastLED.h>

// ==========================
// KONFIGURATION
// ==========================

#define PIN_0 23
#define PIN_1 22
#define PIN_2 21
#define PIN_3 19
#define PIN_4 18
#define PIN_5 5
#define PIN_6 4

#define LEDS_0 40
#define LEDS_1 40
#define LEDS_2 40
#define LEDS_3 120
#define LEDS_4 120
#define LEDS_5 40
#define LEDS_6 40

#define BRIGHTNESS 60

CRGB leds0[LEDS_0];
CRGB leds1[LEDS_1];
CRGB leds2[LEDS_2];
CRGB leds3[LEDS_3];
CRGB leds4[LEDS_4];
CRGB leds5[LEDS_5];
CRGB leds6[LEDS_6];

struct StripPtr {
  CRGB* leds;
  int count;
};

StripPtr strips[7] = {
  {leds0, LEDS_0},
  {leds1, LEDS_1},
  {leds2, LEDS_2},
  {leds3, LEDS_3},
  {leds4, LEDS_4},
  {leds5, LEDS_5},
  {leds6, LEDS_6}
};

// ==========================
// STATE
// ==========================

struct Segment {
  bool active;
  int start;
  int end;
  CRGB color;
};

// Max 4 segments per strip for now
Segment segments[7][4];

float phasePower = 0;
float phaseHeat  = 0;

// ==========================
// ESP-NOW
// ==========================

typedef struct __attribute__((packed)) {
  char  cmd[12];
  int16_t idx;
  int16_t val;
  int16_t extra;
  char  payload[64];
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
  uint8_t rs232_seq;
  char    last_rs232_res[64];
} StatusMsg;

void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != sizeof(CmdMsg)) return;
  
  CmdMsg msg;
  memcpy(&msg, data, sizeof(msg));
  
  if (strcmp(msg.cmd, "LED") == 0) {
    // Payload: strip|start|end|val|r|g|b
    int s=0, start=0, end=0, val=0, r=0, g=0, b=0;
    sscanf(msg.payload, "%d|%d|%d|%d|%d|%d|%d", &s, &start, &end, &val, &r, &g, &b);
    
    if (s >= 0 && s < 7) {
      // Find empty slot or matching range
      int slot = -1;
      for (int i=0; i<4; i++) {
        if (segments[s][i].start == start && segments[s][i].end == end) {
          slot = i;
          break;
        }
      }
      if (slot == -1) {
        for (int i=0; i<4; i++) {
          if (!segments[s][i].active) {
            slot = i;
            break;
          }
        }
      }
      
      if (slot != -1) {
        segments[s][slot].active = (val > 0);
        segments[s][slot].start  = start;
        segments[s][slot].end    = end;
        segments[s][slot].color  = CRGB(r, g, b);
      }
    }
  }
}

// ==========================
// RENDER
// ==========================

void updateStrips() {
  for (int s=0; s<7; s++) {
    fadeToBlackBy(strips[s].leds, strips[s].count, 40);
    
    for (int seg=0; seg<4; seg++) {
      if (!segments[s][seg].active) continue;
      
      int start = segments[s][seg].start;
      int end   = segments[s][seg].end;
      CRGB color = segments[s][seg].color;
      
      for (int i = start; i < end && i < strips[s].count; i++) {
        float wave = sin((i * 0.20) + phasePower);
        int brightness = max(0, int(wave * 200));
        
        // Blend color with wave
        CRGB finalColor = color;
        finalColor.nscale8_video(brightness);
        
        // Add to existing (additive blending)
        strips[s].leds[i] += finalColor;
      }
    }
  }
}

// ==========================
// SETUP
// ==========================

void setup() {
  Serial.begin(115200);
  
  FastLED.addLeds<WS2812B, PIN_0, GRB>(leds0, LEDS_0);
  FastLED.addLeds<WS2812B, PIN_1, GRB>(leds1, LEDS_1);
  FastLED.addLeds<WS2812B, PIN_2, GRB>(leds2, LEDS_2);
  FastLED.addLeds<WS2812B, PIN_3, GRB>(leds3, LEDS_3);
  FastLED.addLeds<WS2812B, PIN_4, GRB>(leds4, LEDS_4);
  FastLED.addLeds<WS2812B, PIN_5, GRB>(leds5, LEDS_5);
  FastLED.addLeds<WS2812B, PIN_6, GRB>(leds6, LEDS_6);
  
  FastLED.setBrightness(BRIGHTNESS);
  
  WiFi.mode(WIFI_STA);
  Serial.print("ESP-NOW MAC: ");
  Serial.println(WiFi.macAddress());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(onReceive);
  
  // Clear segments
  for (int s=0; s<7; s++) {
    for (int seg=0; seg<4; seg++) {
      segments[s][seg].active = false;
    }
  }
}

void loop() {
  updateStrips();
  
  phasePower += 0.18;
  
  FastLED.show();
  delay(20);
  
  // Send heartbeat status to host
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 2000) {
    lastStatus = millis();
    // Host logic expects these status messages to track online status
    StatusMsg sm;
    memset(&sm, 0, sizeof(sm));
    strcpy(sm.device, "esp5");
    // Send to host (broadcast or specific MAC if known, for now broadcast)
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    // Note: registerPeer would be needed here too if sending to specific, 
    // but for status reporting, host just needs to receive.
  }
}
