// ESPHost.ino
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

WebServer server(80);

struct ClientInfo { String name; String ip; unsigned long lastSeen; };
#define MAX_CLIENTS 12
ClientInfo clients[MAX_CLIENTS];

const char* AP_SSID = "ESP-HOST";
const char* AP_PASS = "espHostPass";

void addOrUpdateClient(const String &name, const String &ip){
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name == name){
      clients[i].ip = ip; clients[i].lastSeen = millis(); return;
    }
  }
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name.length()==0){
      clients[i].name = name; clients[i].ip = ip; clients[i].lastSeen = millis(); return;
    }
  }
}

String listClientsJSON(){
  String s = "[";
  bool first=true;
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name.length()){
      if(!first) s += ",";
      s += "{\"name\":\""+clients[i].name+"\",\"ip\":\""+clients[i].ip+"\"}";
      first=false;
    }
  }
  s += "]";
  return s;
}

void handleRegister(){
  if(server.method() != HTTP_POST){ server.send(405,"text/plain","method"); return; }
  String body = server.arg("plain");
  int nidx = body.indexOf("\"name\"");
  int iidx = body.indexOf("\"ip\"");
  String name="", ip="";
  if(nidx>=0){
    int q1 = body.indexOf('"', nidx+6);
    int q2 = body.indexOf('"', q1+1);
    if(q1>=0 && q2>q1) name = body.substring(q1+1,q2);
  }
  if(iidx>=0){
    int q1 = body.indexOf('"', iidx+4);
    int q2 = body.indexOf('"', q1+1);
    if(q1>=0 && q2>q1) ip = body.substring(q1+1,q2);
  }
  if(name.length()==0) { server.send(400,"text/plain","missing name"); return; }
  if(ip.length()==0) ip = server.client().remoteIP().toString();
  addOrUpdateClient(name, ip);
  server.send(200,"application/json","{\"status\":\"ok\"}");
}

void handleClients(){ server.send(200,"application/json",listClientsJSON()); }

void handleForward(){
  if(server.method() != HTTP_POST){ server.send(405,"text/plain","method"); return; }
  String b = server.arg("plain");
  auto extract = [&](const char* key)->String{
    int idx = b.indexOf(String("\"")+String(key)+String("\""));
    if(idx<0) return "";
    int colon = b.indexOf(":", idx);
    if(colon<0) return "";
    int q1 = b.indexOf("\"", colon);
    if(q1<0) return "";
    int q2 = b.indexOf("\"", q1+1);
    if(q2<0) return "";
    return b.substring(q1+1,q2);
  };
  String target = extract("target");
  String method = extract("method");
  String path = extract("path");
  String body="";
  int bi = b.indexOf("\"body\"");
  if(bi>=0){
    int colon = b.indexOf(":",bi);
    int start = b.indexOf("\"",colon);
    int end = b.lastIndexOf("\"");
    if(start>=0 && end>start) body = b.substring(start+1,end);
  }

  if(target.length()==0 || method.length()==0 || path.length()==0){
    server.send(400,"text/plain","missing args");
    return;
  }

  String targetIp="";
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name==target){ targetIp = clients[i].ip; break; }
  }
  if(targetIp.length()==0){ server.send(404,"text/plain","target not registered"); return; }

  String url = "http://"+targetIp+path;
  HTTPClient http;
  http.begin(url);
  int code = 0;
  String resp="";
  if(method=="GET"){
    code = http.GET();
    if(code>0) resp = http.getString();
  } else {
    http.addHeader("Content-Type","text/plain");
    if(method=="POST"){ code = http.POST(body); if(code>0) resp = http.getString(); }
    else { server.send(400,"text/plain","unsupported method"); http.end(); return; }
  }
  String out = "{\"code\":"+String(code)+",\"body\":\"";
  for(size_t i=0;i<resp.length();++i){
    char c = resp[i];
    if(c=='\\') out += "\\\\";
    else if(c=='"') out += "\\\"";
    else if(c=='\n') out += "\\n";
    else if(c=='\r') out += "\\r";
    else out += c;
  }
  out += "\"}";
  http.end();
  server.send(200,"application/json",out);
}

void setup(){
  Serial.begin(115200);
  for(int i=0;i<MAX_CLIENTS;i++){ clients[i].name=""; clients[i].ip=""; clients[i].lastSeen=0; }

  IPAddress apIP(192,168,4,1);
  IPAddress netM(255,255,255,0);
  WiFi.softAPConfig(apIP, apIP, netM);
  WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  Serial.printf("AP gestartet SSID:%s IP:%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  server.on("/register", HTTP_POST, handleRegister);
  server.on("/clients", HTTP_GET, handleClients);
  server.on("/forward", HTTP_POST, handleForward);
  server.begin();
  Serial.println("HTTP Server gestartet (Host).");
}

void loop(){
  server.handleClient();
  unsigned long now = millis();
  for(int i=0;i<MAX_CLIENTS;i++){
    if(clients[i].name.length() && now - clients[i].lastSeen > 120000){
      clients[i].name=""; clients[i].ip=""; clients[i].lastSeen=0;
    }
  }
}
