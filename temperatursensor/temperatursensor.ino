// Beispiel für ESP32 (Arduino Core)
// ADC an GPIO34 (ADC1). Rf oben, NTC zu GND.

const int adcPin = 34;
const float Vcc = 3.3;
const float Rf = 10000.0;     // fester Widerstand 10k
const float R0 = 10000.0;     // NTC Widerstand bei 25°C
const float T0_K = 298.15;  // 25 °C in Kelvin

const float beta = 3950.0;    // Beta-Wert (anpassen falls bekannt)

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);    // 12-bit -> 0..4095
  // Falls nötig: adcSetPinAttenuation(...) (je nach Core/Setup)
}

float readVoltage() {
  int adc = analogRead(adcPin);
  float v = (float)adc / 4095.0 * Vcc;
  return v;
}

float readR_NTC() {
  float v = readVoltage();
  // Schutz gegen Division durch 0
  if (v <= 0.0001) return 1e9;
  if (v >= Vcc-0.0001) return 1e-6;
  float r = Rf * (v / (Vcc - v));
  return r;
}

float ntcToCelsius(float Rntc) {
  float invT = (1.0f / T0_K) + (1.0f / beta) * log(Rntc / R0);
float T = 1.0f / invT;     // Kelvin
return T - 273.15f;        // Celsius

}

void loop() {
  float Rntc = readR_NTC();
  float tempC = ntcToCelsius(Rntc);
  Serial.print("R_ntc = "); Serial.print(Rntc); Serial.print(" ohm, ");
  Serial.print("T = "); Serial.print(tempC); Serial.println(" C");
  delay(1000);
}
