#include <driver/dac.h>

const int dacPin = 25;       // GPIO25 = DAC1
const int maxValue = 255;    // DAC-Wert 0..255 -> 0..~3.3V
const int stepPercent = 10;  // +20% pro Schritt

const int buttonUpPin = 32;   // GPIO für "up" Button
const int buttonDownPin = 33; // GPIO für "down" Button

const unsigned long minInterval = 1000; // Mindestzeit zwischen Änderungen in ms

int currentPercent = 0;
int lastPercent = -1;         // Speichert letzten DAC-Prozentwert
unsigned long lastChangeTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starte DAC-Steuerung...");

  pinMode(buttonUpPin, INPUT_PULLUP);
  pinMode(buttonDownPin, INPUT_PULLUP);

  dacWrite(dacPin, 0); // Start bei 0 V
}

void loop() {
  unsigned long now = millis();
  bool changed = false;

  if (now - lastChangeTime >= minInterval) {
    // Button "Up" prüfen
    if (digitalRead(buttonUpPin) == LOW) {
      currentPercent += stepPercent;
      if (currentPercent > 100) currentPercent = 100;
      changed = true;
    }

    // Button "Down" prüfen
    if (digitalRead(buttonDownPin) == LOW) {
      currentPercent -= stepPercent;
      if (currentPercent < 0) currentPercent = 0;
      changed = true;
    }

    // Nur aktualisieren, wenn sich der Wert geändert hat
    if (changed && currentPercent != lastPercent) {
      updateDAC();
      lastPercent = currentPercent;
      lastChangeTime = now; // Zeitpunkt der letzten Änderung speichern
    }
  }
}

void updateDAC() {
  int dacValue = (currentPercent * maxValue) / 100;
  dacWrite(dacPin, dacValue);

  Serial.print("Spannung auf ");
  Serial.print(currentPercent);
  Serial.print("% gesetzt (DAC-Wert: ");
  Serial.print(dacValue);
  Serial.println(")");
}
