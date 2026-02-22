// 3x 4–20mA Sensors with shared zero point, different pressure ranges

const int SENSOR_PINS[3] = {35, 34, 33};
const float SHUNT_RESISTOR = 165.0;

// Gemeinsamer Nullpunkt
const float I_ZERO = 3.19;   // mA = 0 bar
const float I_FULL = 20.0;   // mA = max pressure

// Druckbereiche pro Sensor
const float PRESSURE_MAX[3] = {
  4.0,  // Sensor 1
  4.0,  // Sensor 2
  6.0   // Sensor 3
};

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

void loop() {
  for (int i = 0; i < 3; i++) {

    int rawADC = analogRead(SENSOR_PINS[i]);
    float voltage = (rawADC / 4095.0) * 3.3;
    float current_mA = (voltage / SHUNT_RESISTOR) * 1000.0;

    float pressure_bar =
      ((current_mA - I_ZERO) / (I_FULL - I_ZERO)) * PRESSURE_MAX[i];

    // Clamp
    if (pressure_bar < 0) pressure_bar = 0;
    if (pressure_bar > PRESSURE_MAX[i]) pressure_bar = PRESSURE_MAX[i];

    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(current_mA, 2);
    Serial.print(" mA | ");
    Serial.print(pressure_bar, 2);
    Serial.println(" bar");
  }

  Serial.println("------------------------");
  delay(1000);
}
