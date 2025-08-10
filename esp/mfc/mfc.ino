#include <HardwareSerial.h>

// UART2 für RS232 (Pins anpassen)
HardwareSerial RS232Serial(2);

const int RXD2 = 16; // RS232 RX
const int TXD2 = 17; // RS232 TX

void setup() {
  Serial.begin(115200);                        // USB-Debug
  RS232Serial.begin(38400, SERIAL_8N1, RXD2, TXD2); // RS232 (Baudrate anpassen!)
  Serial.println("RS232 Test gestartet");
}

void loop() {
  // Vom PC → RS232
  if (Serial.available()) {
    RS232Serial.write(Serial.read());
  }
  // Von RS232 → PC
  if (RS232Serial.available()) {
    Serial.write(RS232Serial.read());
  }
}
 
