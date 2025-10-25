#include <WiFi.h>
#include <WebServer.h>

// --------- AP & WebServer ---------
const char* ap_ssid = "ESP-Zug";       // WLAN-Name
const char* ap_password = "12345678";  // Passwort
WebServer server(80);

// --------- Pin & PWM ---------
const int pwmPin = 25;
const int freq = 5000;      // PWM-Frequenz in Hz
const int resolution = 8;   // 8-Bit (0-255)
int pwmValue = 0;

// --------- HTML Seite ---------
String htmlPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<title>ESP Zug Steuerung</title>";
  html += "<style>body{font-family:Arial;text-align:center;margin-top:50px} input[type=range]{width:80%;}</style>";
  html += "</head><body>";
  html += "<h1>ESP Zug PWM Steuerung</h1>";
  html += "<p>PWM Wert: <span id='val'>" + String(pwmValue) + "</span></p>";
  html += "<input type='range' min='0' max='255' value='" + String(pwmValue) + "' id='slider' oninput='updatePWM(this.value)'>";
  html += "<script>";
  html += "function updatePWM(v){";
  html += "document.getElementById('val').innerText=v;";
  html += "fetch('/set?pwm='+v);}";
  html += "</script></body></html>";
  return html;
}

// --------- Handler ---------
void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleSet() {
  if (server.hasArg("pwm")) {
    pwmValue = server.arg("pwm").toInt();
    pwmValue = constrain(pwmValue, 0, 255);
    ledcWrite(pwmPin, pwmValue);
  }
  server.send(200, "text/plain", "OK");
}

void setupPWM() {
  ledcAttach(pwmPin, freq, resolution); // neue API
  ledcWrite(pwmPin, pwmValue);
}

void setup() {
  Serial.begin(115200);
  setupPWM();

  // Access Point starten
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("AP gestartet. Verbinde mit dem WLAN!");

  // Webserver Routen
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("Webserver gestartet");
}

void loop() {
  server.handleClient();
}
