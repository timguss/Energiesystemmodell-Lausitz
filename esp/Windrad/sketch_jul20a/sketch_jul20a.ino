const int ledPin = 26;    // nur eine LED
const int motorPin = 22;
const int buttonPin = 21;

bool isRunning = false;
bool lastStableButtonState = HIGH;
bool lastReadButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

const unsigned long blinkOnTime = 300;
const unsigned long blinkOffTime = 1000;

unsigned long lastBlinkTime = 0;
bool ledState = false;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
}

void loop() {
  // --- Button-Handling ---
  bool reading = digitalRead(buttonPin);

  if (reading != lastReadButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != lastStableButtonState) {
      lastStableButtonState = reading;

      if (reading == LOW) { // nur bei Druck
        isRunning = !isRunning;
        Serial.println(isRunning ? "Anlage EIN" : "Anlage AUS");
      }
    }
  }
  lastReadButtonState = reading;

  // --- Windradsteuerung ---
  if (isRunning) {
    digitalWrite(motorPin, HIGH);

    // Blinklogik
    unsigned long now = millis();
    unsigned long interval = ledState ? blinkOnTime : blinkOffTime;

    if (now - lastBlinkTime >= interval) {
      lastBlinkTime = now;
      ledState = !ledState;
      digitalWrite(ledPin, ledState ? HIGH : LOW);
    }
  } else {
    digitalWrite(motorPin, LOW);
    digitalWrite(ledPin, LOW);
  }
}
