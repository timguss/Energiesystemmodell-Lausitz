#include <FastLED.h>

// ==========================
// KONFIGURATION
// ==========================

// Pins (müssen konstant sein!)
#define PIN_0 23
#define PIN_1 22
#define PIN_2 21
#define PIN_3 19
#define PIN_4 18
#define PIN_5 5
#define PIN_6 4

// LED-Anzahlen
#define LEDS_0 40
#define LEDS_1 40
#define LEDS_2 40
#define LEDS_3 120
#define LEDS_4 120
#define LEDS_5 40
#define LEDS_6 40

// Helligkeit
#define BRIGHTNESS 40

// ==========================
// LED ARRAYS
// ==========================

CRGB leds0[LEDS_0];
CRGB leds1[LEDS_1];
CRGB leds2[LEDS_2];
CRGB leds3[LEDS_3];
CRGB leds4[LEDS_4];
CRGB leds5[LEDS_5];
CRGB leds6[LEDS_6];

// ==========================
// ANIMATION
// ==========================

float phasePower = 0;
float phaseHeat  = 0;

// ==========================
// SETUP
// ==========================

void setup() {
  FastLED.addLeds<WS2812B, PIN_0, GRB>(leds0, LEDS_0);
  FastLED.addLeds<WS2812B, PIN_1, GRB>(leds1, LEDS_1);
  FastLED.addLeds<WS2812B, PIN_2, GRB>(leds2, LEDS_2);
  FastLED.addLeds<WS2812B, PIN_3, GRB>(leds3, LEDS_3);
  FastLED.addLeds<WS2812B, PIN_4, GRB>(leds4, LEDS_4);
  FastLED.addLeds<WS2812B, PIN_5, GRB>(leds5, LEDS_5);
  FastLED.addLeds<WS2812B, PIN_6, GRB>(leds6, LEDS_6);

  FastLED.setBrightness(BRIGHTNESS);
}

// ==========================
// EFFEKT FUNKTION
// ==========================

void updateStrip(CRGB* leds, int count) {

  fadeToBlackBy(leds, count, 50);

  for (int i = 0; i < count; i++) {

    float powerWave = sin((i * 0.20) + phasePower);
    float heatWave  = sin((i * 0.20) + phaseHeat);

    int powerBrightness = max(0, int(powerWave * 200));
    int heatBrightness  = max(0, int(heatWave  * 200));

    leds[i] = CRGB(
      powerBrightness,
      0,
      heatBrightness
    );
  }
}

// ==========================
// LOOP
// ==========================

void loop() {

  // alle Strips synchron updaten
  updateStrip(leds0, LEDS_0);
  updateStrip(leds1, LEDS_1);
  updateStrip(leds2, LEDS_2);
  updateStrip(leds3, LEDS_3);
  updateStrip(leds4, LEDS_4);
  updateStrip(leds5, LEDS_5);
  updateStrip(leds6, LEDS_6);

  // globale Bewegung (entscheidend für Synchronität!)
  phasePower += 0.18;
  phaseHeat  -= 0.12;

  FastLED.show();
  delay(20);
}