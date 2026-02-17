// 4-20mA Sensor Reading with Offset Calibration
const int SENSOR_PIN = 35;     
const float SHUNT_RESISTOR = 165.0;  

// Calibration
const float I_MIN = 3.19;    // mA at 0 bar
const float I_MAX = 20.0;    // mA at 4 bar
const float PRESSURE_MAX = 4.0; // bar

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);      
  analogSetAttenuation(ADC_11db); 
}

void loop() {
  int rawADC = analogRead(SENSOR_PIN);
  float voltage = (rawADC / 4095.0) * 3.3;

  float current_mA = (voltage / SHUNT_RESISTOR) * 1000.0;

  // Adjust for sensor offset
  float pressure_bar = ((current_mA - I_MIN) / (I_MAX - I_MIN)) * PRESSURE_MAX;

  // Clamp
  if (pressure_bar < 0) pressure_bar = 0;
  if (pressure_bar > PRESSURE_MAX) pressure_bar = PRESSURE_MAX;

  Serial.print("Current: ");
  Serial.print(current_mA, 2);
  Serial.print(" mA\t| Pressure: ");
  Serial.print(pressure_bar, 2);
  Serial.println(" bar");

  delay(1000);
}
