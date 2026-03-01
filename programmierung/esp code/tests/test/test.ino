// ================== KONFIGURATION ==================
#define FLOW_PIN 17              // Blaues Kabel
#define MEASURE_INTERVAL 1000    // Messfenster in ms
float K_FACTOR = 1.0;            // Platzhalter! Wird später kalibriert
                                 // Einheit: Pulse pro Liter

// ================== VARIABLEN ======================
volatile uint32_t pulseCount = 0;

// ================== INTERRUPT ======================
void IRAM_ATTR onPulse() {
  pulseCount++;
}

// ================== SETUP ==========================
void setup() {
  Serial.begin(115200);

  pinMode(FLOW_PIN, INPUT_PULLUP);   // interner Pull-Up
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN),
                  onPulse,
                  FALLING);          // NPN → LOW-Puls

  Serial.println("Flowmeter gestartet");
}

// ================== LOOP ===========================
void loop() {
  static uint32_t lastMillis = 0;

  if (millis() - lastMillis >= MEASURE_INTERVAL) {
    lastMillis = millis();

    // Pulse atomar übernehmen
    noInterrupts();
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    // Berechnung
    float pulsesPerSecond = pulses / (MEASURE_INTERVAL / 1000.0);
    float flow_L_per_min  = (pulsesPerSecond * 60.0) / K_FACTOR;

    // Ausgabe
    Serial.print("Pulse/s: ");
    Serial.print(pulsesPerSecond);
    Serial.print(" | Durchfluss [L/min]: ");
    Serial.println(flow_L_per_min, 4);
  }
}