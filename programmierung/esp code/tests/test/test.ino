#include <FastLED.h>

#define LED_PIN 18
#define NUM_LEDS 240

CRGB leds[NUM_LEDS];

float phasePower = 0;
float phaseHeat  = 0;

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(120);
}

void loop() {

  fadeToBlackBy(leds, NUM_LEDS, 50);

  for(int i = 0; i < NUM_LEDS; i++) {

    float powerWave = sin((i * 0.20) + phasePower);
    float heatWave  = sin((i * 0.20) + phaseHeat);

    int powerBrightness = max(0, int(powerWave * 200));
    int heatBrightness  = max(0, int(heatWave  * 200));

    // Farben begrenzen damit sie sich nicht stark mischen
    leds[i] = CRGB(
      powerBrightness,
      0,
      heatBrightness
    );
  }

  phasePower += 0.18;   // Strom →
  phaseHeat  -= 0.12;   // Wärme ←

  FastLED.show();
  delay(20);
}