// ESP32 -> L298N (IN1 = HIGH, IN2 = GND)
// Dauerbetrieb: wechsle sanft zwischen "langsam" und "schnell" (endlos)
// Benötigt ESP32-Core >= v3.x (ledcAttach / ledcWrite)

const int PIN_PWM = 25;                  // ENA am L298N
const uint32_t PWM_FREQ = 20000;         // 20 kHz
const uint8_t PWM_RES_BITS = 8;          // 8 bit Auflösung (0..255)
const uint32_t PWM_MAX = (1UL << PWM_RES_BITS) - 1; // 255

// Betriebsparameter (anpassbar)
const int slowPercent = 20;     // "langsam" in %
const int fastPercent = 100;    // "schnell" in %
const unsigned long rampTimeMs = 4000; // Zeit für Rampen (in ms) 0->Ziel
const unsigned long holdMs = 4000;     // Haltezeit auf Zielgeschwindigkeit (in ms)

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; } // optional warten, falls nötig
  Serial.println();
  Serial.println("Start: Wechsel zwischen langsam <-> schnell (sanft)");

  // LEDC (neue API)
  if (!ledcAttach(PIN_PWM, PWM_FREQ, PWM_RES_BITS)) {
    Serial.println("Fehler: ledcAttach fehlgeschlagen! Prüfe Core/Pin.");
    while (true) { delay(500); } // stoppen, damit du den Fehler siehst
  }

  // Initial 0 setzen
  ledcWrite(PIN_PWM, 0);
  delay(100);
}

void loop() {
  // Wechsel: 0 -> slow -> fast -> slow -> fast ...
  // Start von 0 auf slow
  rampToPercent(slowPercent, rampTimeMs);
  delay(holdMs);

  // slow -> fast
  rampToPercent(fastPercent, rampTimeMs);
  delay(holdMs);

  // fast -> slow
  rampToPercent(slowPercent, rampTimeMs);
  delay(holdMs);

  // repeat (loop macht das automatisch)
}

// Funktion: sanft von aktueller Duty auf targetPercent in rampMs Millisekunden
void rampToPercent(int targetPercent, unsigned long rampMs) {
  targetPercent = constrain(targetPercent, 0, 100);
  // aktuellen Duty lesen ist nicht direkt verfügbar -> wir merken uns via variable
  static int currentDuty = 0; // persistiert zwischen Aufrufen
  uint32_t targetDuty = map(targetPercent, 0, 100, 0, PWM_MAX);

  if ((int)targetDuty == currentDuty) {
    Serial.print("Bereits bei ");
    Serial.print(targetPercent);
    Serial.println("%");
    return;
  }

  long delta = (long)targetDuty - (long)currentDuty;
  long steps = abs(delta);
  if (steps == 0) return;

  // Schrittverzögerung (ms). Mindestens 1 ms, ansonsten so verteilen, dass rampMs eingehalten wird.
  unsigned long stepDelay = rampMs / (steps > 0 ? steps : 1);
  if (stepDelay == 0) stepDelay = 1;

  unsigned long startMs = millis();
  int lastPercentPrinted = -1;

  Serial.print("Starte Rampe: ");
  Serial.print((currentDuty * 100UL) / PWM_MAX);
  Serial.print("% -> ");
  Serial.print(targetPercent);
  Serial.println("%");

  int stepDir = delta > 0 ? 1 : -1;

  for (long s = 0; s < steps; ++s) {
    currentDuty += stepDir;
    if (!ledcWrite(PIN_PWM, currentDuty)) {
      Serial.println("Warnung: ledcWrite fehlgeschlagen");
    }

    int percent = (int)((currentDuty * 100UL) / PWM_MAX);
    if (percent != lastPercentPrinted) {
      unsigned long now = millis();
      Serial.print("t=");
      Serial.print(now - startMs);
      Serial.print(" ms, PWM=");
      Serial.print(currentDuty);
      Serial.print(" (");
      Serial.print(percent);
      Serial.println("%)");
      lastPercentPrinted = percent;
    }

    delay(stepDelay);
  }

  // Abschließende Statusmeldung
  unsigned long total = millis() - startMs;
  Serial.print("Rampe fertig: ");
  Serial.print(targetPercent);
  Serial.print("% (");
  Serial.print(total);
  Serial.println(" ms)");
  // currentDuty bleibt erhalten
}
