#ifndef WIFICUSTODIAN_H
#define WIFICUSTODIAN_H

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

class wifiCustodian{
  public:
    wifiCustodian();
    bool autoConnect();
    bool connectWifi();
    bool connectWifi(bool storeConnection);
    bool tempAP();
    void setTimeout(int seconds);

    void readConnection(uint16 i);
    int addConnection();
    void deleteConnections();

  private:
    const char* broadcast_ssid = "espThing";
    char ssid[32] = {0};
    char password[32] = {0};
    int timeout = 15*1000;
    bool connected = 0;
    bool refreshAttempt = 0;
    bool ready = 0;
    ESP8266WebServer server;
    int storedConnectionCount = 0;

    void handleRoot();
    void handleUpdate();
    void handleConnect();
    void handleDelete();
};

wifiCustodian::wifiCustodian(): server(80){
  storedConnectionCount = EEPROM.read(0);
  if(storedConnectionCount>16 || storedConnectionCount<0){
    EEPROM.write(0, 0);
    storedConnectionCount = 0;
    EEPROM.commit();
  }
}

/*
 *
 */
bool wifiCustodian::autoConnect(){
  WiFi.disconnect();
  connected = (WiFi.status() == WL_CONNECTED);

  if(!connected){
    connected = connectWifi();
    while(!ready){
      tempAP();    // Temporary Access Point mode
    }
  }
  WiFi.softAPdisconnect();
  return connected;
}


/*
 *
 */
bool wifiCustodian::connectWifi(){
  int networkCount = WiFi.scanNetworks(1, 1);
  while(WiFi.scanComplete()<0) {
    delay(100);
  }
  networkCount = WiFi.scanComplete();

  for(int j=0; j < storedConnectionCount; j++){
    readConnection(j);
    for(int i=0; (i < networkCount) && !connected; i++){
      if(WiFi.SSID(i) == String(reinterpret_cast<const char*>(ssid))){
        /* Connect to WiFi network */
        WiFi.begin(ssid, password);

        int startMillis = millis();
        /* Wait for connection until timeout */
        while (!connected && (millis() - startMillis < timeout)){
          connected = WiFi.status() == WL_CONNECTED;
          delay(500);
        }
      }
    }
  }
  WiFi.scanDelete();

  return connected;
}

/*
 *
 */
bool wifiCustodian::connectWifi(bool storeConnection){
  int networkCount = WiFi.scanNetworks(1, 1);
  while(WiFi.scanComplete()<0) {
    delay(100);
  }
  networkCount = WiFi.scanComplete();
  connected = (WiFi.status() == WL_CONNECTED);
  for(int i=0; (i < networkCount) && !connected; i++){
    if(WiFi.SSID(i) == String(reinterpret_cast<const char*>(ssid))){
      /* Connect to WiFi network */
      WiFi.begin(ssid, password);

      int startMillis = millis();
      /* Wait for connection until timeout */
      while (!connected && (millis() - startMillis < timeout)){
        connected = WiFi.status() == WL_CONNECTED;
        delay(500);
      }
    }
  }
  WiFi.scanDelete();

  if(connected && storeConnection){
    addConnection();
  }
  return connected;
}

/*
 *
 */
bool wifiCustodian::tempAP(){
  WiFi.softAP(broadcast_ssid);
  IPAddress myIP = WiFi.softAPIP();

  server.on("/", std::bind(&wifiCustodian::handleRoot, this));
  server.on("/update", std::bind(&wifiCustodian::handleUpdate, this));
  server.on("/connect", std::bind(&wifiCustodian::handleConnect, this));
  server.on("/delete", std::bind(&wifiCustodian::handleDelete, this));
  server.begin();

  while(!ready){
    server.handleClient();
  }

}

/*
 *
 */
void wifiCustodian::setTimeout(int seconds){
  timeout = seconds*1000;
}

/*
 *
 */
void wifiCustodian::readConnection(uint16 i){
  for (int j = i*64+1; j < ((i+1)*64)-32+1; j++){
    ssid[i] = char(EEPROM.read(j));
    i++;
  }
  for (int j = (i*64)+32+1; j < ((i+1)*64)+32+1; j++){
    ssid[i] = char(EEPROM.read(j));
    i++;
  }
}

/*
 *
 */
int wifiCustodian::addConnection(){
  //Read wifi connection ssid and password pair count
  int connCount = EEPROM.read(0);

  if(connCount >=0 && connCount <= 16){
    EEPROM.write(1, connCount+1);
    for (int i = 0; i < i+sizeof(ssid); i++){
      EEPROM.write((i*connCount*64)+1, ssid[i]);
      EEPROM.write((i*connCount*64)+32+1, password[i]);
    }
    EEPROM.commit();
  }
  return connCount+1;
}

/*
 *
 */
void wifiCustodian::deleteConnections(){
  for(int i = 0; i <= 16 * 64; i++){
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

/*
 *
 */
void wifiCustodian::handleRoot() {
  String content = ""
"<html>"
  "<body>"
    "<h1>wifiCustodian v0.1</h1>"
    "<form action='update'>"
      "SSID:     <input type='text' name='ssid'></input><br>"
      "Password: <input type='password' name='password'></input><br>"
      "<input type='submit'>"
    "</form>"
    "<a href='/delete'>Wipe Connections</a>"
  "</body>"
"</html>";

	server.send(200, "text/html", content);
}

/*
 *
 */
void wifiCustodian::handleUpdate(){
  if (server.args() > 0 ){
    if(server.argName(0) == "ssid"){
      strncpy(ssid, server.arg(0).c_str(), sizeof(ssid));
    }
    if(server.argName(1) == "password"){
      strncpy(password, server.arg(1).c_str(), sizeof(password));
    }
  }
  String message;
  if(!connected){
    if(!refreshAttempt){
      refreshAttempt = 1;
      message = ""
      "<head>"
        "<meta http-equiv='refresh' content='15'>"
      "</head>"
      "<h1>Attempting to connect to Wifi. Please wait.</h1>";
      server.send(200, "text/html", message);
      connected = connectWifi(1);
    }else{
      refreshAttempt = 0;
      server.sendHeader("Location", String("/"), true);
      server.send(302, "text/html", "Failed.");
    }
  }else{
    server.sendHeader("Location", String("/connect"), true);
    server.send ( 302, "text/plain", "");
  }
}

/*
 *
 */
void wifiCustodian::handleConnect() {
  String content = "<h1>Connected</h1>";
	server.send(200, "text/html", content);
  ready = 1;
}

/*
 *
 */
void wifiCustodian::handleDelete() {
  String content = "<h1>SSID and Password Connection Pairs Deleted</h1>";
	server.send(200, "text/html", content);
  deleteConnections();
}

#endif
