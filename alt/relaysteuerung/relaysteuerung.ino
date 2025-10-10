#include <WiFi.h>
#include <WebServer.h>

const bool ACTIVE_LOW = true;   // LOW = an, HIGH = aus

struct RelayConfig { 
  uint8_t pin; 
  uint8_t relayNum; 
  const char* title; 
};

const uint8_t RELAY_COUNT = 8;
RelayConfig RELAYS[RELAY_COUNT] = {
  {19, 1, "Ventil - 1"},
  {21, 2, "Ventil - 2"},
  {22, 3, "Heizstab"},
  {23, 4, "Zünder"},
  {32, 5, "Gasventil"},
  {33, 6, "Kühler"},
  {25, 7, "MFC - fehlt noch"},
  {26, 8, "unbelegt"}
};

const char* ssid = "Fritz-Box-SG";
const char* password = "floppy1905Frettchen";

WebServer server(80);

int physToLogical(int physVal) {
  return ACTIVE_LOW ? (physVal == LOW ? 1 : 0)
                    : (physVal == HIGH ? 1 : 0);
}
int logicalToPhys(int logical) {
  return ACTIVE_LOW ? (logical == 1 ? LOW : HIGH)
                    : (logical == 1 ? HIGH : LOW);
}

String getStateJSON() {
  String s="{";
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    int phys = digitalRead(RELAYS[i].pin);
    s += "\"r"+String(i)+"\":"+String(physToLogical(phys));
    if(i<RELAY_COUNT-1) s+=",";
  }
  s+="}";
  return s;
}

// Root-Seite mit allen Slidern
void handleRoot() {
  String page = "<!doctype html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>ESP32 Relais</title>"
                "<style>"
                "body{font-family:Arial;padding:20px;max-width:800px;margin:auto}"
                ".slot{display:flex;align-items:center;justify-content:space-between;padding:12px;margin:10px 0;background:#f2f2f2;border-radius:8px}"
                ".label{font-weight:600}.switch{position:relative;display:inline-block;width:60px;height:34px}"
                ".switch input{display:none}"
                ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;transition:.3s;border-radius:34px}"
                ".slider:before{position:absolute;content:'';height:26px;width:26px;left:4px;top:4px;background:white;transition:.3s;border-radius:50%}"
                "input:checked + .slider{background:#4caf50}"
                "input:checked + .slider:before{transform:translateX(26px)}"
                ".state{margin-left:10px;font-size:14px;color:#333}"
                "</style></head><body><h2>ESP32 8 Relais</h2>";

  for(uint8_t i=0;i<RELAY_COUNT;i++){
    int phys = digitalRead(RELAYS[i].pin);
    int logical = physToLogical(phys);
    page += "<div class='slot'><div class='label'>Relay "+String(RELAYS[i].relayNum)+" — "+String(RELAYS[i].title)
            +" <span id='s"+String(i)+"' class='state'>"+String(logical==1?"AN":"AUS")+"</span></div>"
            +"<label class='switch'><input id='t"+String(i)+"' type='checkbox' "
            +(logical==1?"checked":"")+"><span class='slider'></span></label></div>";
  }

  page += "<script>"
          "const count="+String(RELAY_COUNT)+";"
          "for(let i=0;i<count;i++){"
          "document.getElementById('t'+i).addEventListener('change',function(){"
          "let v=this.checked?1:0;"
          "fetch('/set?idx='+i+'&val='+v).then(()=>setTimeout(refresh,100));"
          "});"
          "}"
          "function refresh(){"
          "fetch('/state').then(r=>r.json()).then(obj=>{"
          "for(let i=0;i<count;i++){"
          "let st=obj['r'+i];"
          "let cb=document.getElementById('t'+i);"
          "let s=document.getElementById('s'+i);"
          "if(cb) cb.checked=(st===1);"
          "if(s) s.textContent=(st===1?'AN':'AUS');"
          "}"
          "});"
          "}"
          "setInterval(refresh,1500);"
          "</script></body></html>";

  server.send(200,"text/html",page);
}

// /set?idx=0..7&val=0|1
void handleSet() {
  if(!server.hasArg("idx")||!server.hasArg("val")){
    server.send(400,"text/plain","missing args");
    return;
  }
  int idx = server.arg("idx").toInt();
  int val = server.arg("val").toInt();
  if(idx<0 || idx>=RELAY_COUNT || (val!=0 && val!=1)){
    server.send(400,"text/plain","invalid args");
    return;
  }
  digitalWrite(RELAYS[idx].pin, logicalToPhys(val));
  server.send(200,"text/plain","ok");
}

void setup(){
  Serial.begin(115200);
  for(uint8_t i=0;i<RELAY_COUNT;i++){
    pinMode(RELAYS[i].pin, OUTPUT);
    digitalWrite(RELAYS[i].pin, logicalToPhys(0));
  }

  WiFi.begin(ssid,password);
  Serial.print("Verbinde WLAN ");Serial.println(ssid);
  unsigned long start=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<20000){ delay(500); Serial.print("."); }
  if(WiFi.status()==WL_CONNECTED){ Serial.println("\nIP:"); Serial.println(WiFi.localIP()); }
  else { WiFi.softAP("ESP32_Fallback","12345678"); Serial.print("AP IP: "); Serial.println(WiFi.softAPIP()); }

  server.on("/",handleRoot);
  server.on("/set",handleSet);
  server.begin();
  Serial.println("HTTP-Server gestartet");
}

void loop(){
  server.handleClient();
}
