// ESP32 – 4 Relais zyklisch schalten

const bool ACTIVE_LOW = true;

// GPIO-Pins der Relais (anpassen!)
const uint8_t RELAY_PINS[4] = {32, 33, 25,12};

// Zeiten (ms)
const unsigned long ON_TIME  = 1000; // Ventil EIN
const unsigned long OFF_TIME = 500;  // Pause zwischen Ventilen

inline uint8_t relayOn()  { return ACTIVE_LOW ? LOW  : HIGH; }
inline uint8_t relayOff() { return ACTIVE_LOW ? HIGH : LOW; }

void setup() {
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], relayOff());
  }
}

void loop() {
  for (uint8_t i = 0; i < 4; i++) {
    // EIN
    digitalWrite(RELAY_PINS[i], relayOn());
    delay(ON_TIME);

    // AUS
    digitalWrite(RELAY_PINS[i], relayOff());
    delay(OFF_TIME);
  }
}
