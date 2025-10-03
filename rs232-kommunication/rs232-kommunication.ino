// Beispiel: ESP32 + ArduinoIDE - Serial2
HardwareSerial MySerial(2); // UART2

void setup() {
  Serial.begin(115200);             // Debug auf USB
  MySerial.begin(38400, SERIAL_8N1, 16, 17); // RX=16, TX=17
  delay(200);
  Serial.println("ProPar over RS232 via COM-TTL-RS232 ready");
}

String readResponse(unsigned long timeout_ms=300) {
  unsigned long start = millis();
  String s = "";
  while (millis() - start < timeout_ms) {
    while (MySerial.available()) {
      char c = MySerial.read();
      s += c;
    }
  }
  return s;
}

void sendSetpoint50pct_Node3() {
  // ProPar ASCII Write setpoint 50% (node 3, process 1, param 1) as in manual
  const char *cmd = ":06030101213E80\r\n";
  MySerial.print(cmd);
  Serial.print("Sent: "); Serial.println(cmd);
  String resp = readResponse(500);
  Serial.print("Reply: "); Serial.println(resp);
}

void loop() {
  // einmal senden, dann warten
  sendSetpoint50pct_Node3();
  delay(5000);
}
