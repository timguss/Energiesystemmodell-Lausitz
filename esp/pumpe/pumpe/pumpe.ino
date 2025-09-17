#include <driver/dac.h>

const int dacPin = 25;       // GPIO25 = DAC1
const int delayTime = 30000; // 30 Sekunden in Millisekunden
const int maxValue = 255;    // DAC-Wert 0..255 -> 0..~3.3V
const int stepPercent = 20;  // +20% pro Schritt

int currentPercent = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starte DAC-Steuerung...");

  dacWrite(dacPin, 0); // Start bei 0 V
}

void loop() {
  // Berechne neuen DAC-Wert
  int dacValue = (currentPercent * maxValue) / 100;
  dacWrite(dacPin, dacValue);

  // Mitteilung ausgeben
  Serial.print("Spannung auf ");
  Serial.print(currentPercent);
  Serial.print("% gesetzt (DAC-Wert: ");
  Serial.print(dacValue);
  Serial.println(")");

  // Warte 30 Sekunden
  delay(delayTime);

  // Erhöhe Prozentwert
  currentPercent += stepPercent;
  if (currentPercent > 100) {
    currentPercent = 0; // zurück auf 0%
  }
}
