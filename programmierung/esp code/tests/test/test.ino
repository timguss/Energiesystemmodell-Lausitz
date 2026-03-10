#include <FastLED.h>

#define LED_PIN 18
#define NUM_LEDS 60

CRGB leds[NUM_LEDS];

float phasePower = 0;
float phaseHeat  = 0;

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(120);
}

void loop() {

  fadeToBlackBy(leds, NUM_LEDS, 40);

  for(int i = 0; i < NUM_LEDS; i++) {

    float powerWave = sin((i * 0.25) + phasePower);
    float heatWave  = sin((i * 0.25) + phaseHeat);

int powerBrightness = max(0, int(powerWave * 255));
int heatBrightness  = max(0, int(heatWave  * 255));

    leds[i] += CRGB(powerBrightness, 0, 0);
    leds[i] += CRGB(0, 0, heatBrightness);
  }

  phasePower += 0.20;   // Geschwindigkeit Strom →
  phaseHeat  -= 0.20;   // Geschwindigkeit Wärme ←

  FastLED.show();
  delay(20);
}