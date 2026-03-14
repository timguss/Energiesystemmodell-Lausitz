#include <FastLED.h>

#define LED_PIN 18
#define NUM_LEDS 3

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
}

void loop() {
  // Weiß
  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
  delay(2000);

  // Rot
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
  delay(2000);

  // Blau
  fill_solid(leds, NUM_LEDS, CRGB::Blue);
  FastLED.show();
  delay(2000);
}