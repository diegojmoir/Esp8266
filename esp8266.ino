
//#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <SoftwareSerial.h>
#include "DHT.h"
#include "Sds011.h"

//SDS011 serial pins
#define SDS_PIN_RX D1
#define SDS_PIN_TX D2

//DHT pins
#define ONEWIRE_PIN D7
#define DHT_TYPE DHT22

unsigned long now;           // current time
unsigned long starttime = 0; // start time of the current measuring period
const unsigned long sending_interval = 60*5000;

//SDS011
bool sds_read = 1;
SoftwareSerial serialSDS(SDS_PIN_RX, SDS_PIN_TX, false, 128);
sds011::Sds011 sensorSDS(serialSDS); 

//DHT
DHT dht(ONEWIRE_PIN, DHT_TYPE);

//Wifi credentials
const char* ssid = "your wifi ssid ";
const char* password = "****";
const char* host = "host site";
const char fingerPrint[] PROGMEM  = "8b 48 5e 67 0e c9 16 47 32 f2 87 0c 1f c8 60 ad";
const char* path = "api path";
const int port = 443; 

struct {
  bool valid;
  float pm10; // 10
  float pm25; // 2.5
} sds011_result = {};

struct {
  bool valid;
  float t; // Temperature
  float h; // Humidity
} dht_result = {};

void initSDS(){
  sensorSDS.set_sleep(false);
  sensorSDS.set_mode(sds011::QUERY);
  sensorSDS.set_sleep(true);
}


String Float2String(const float value) {
  // Convert a float to String with two decimals.
  char temp[13];
  String s;

  dtostrf(value,9, 2, temp);
  s = String(temp);
  s.trim();
  return s;
}

String Value2Json(const String& type, const String& value) {
  String s = F("\"{t}\":\"{v}\",");
  s.replace("{t}",type);
  s.replace("{v}",value);
  return s;
}

String Value2Json(const String& type, float value) {
  String s = F("\"{t}\":\"{v}\",");
  s.replace("{t}",type);
  s.replace("{v}",Float2String(value));
  return s;
}

String Value2Json(const String& type, int value) {
  String s = F("\"{t}\":\"{v}\",");
  s.replace("{t}",type);
  s.replace("{v}", String(value));
  return s;
}

void readDHT() {
  int i = 0;
  double h;
  double t;
  dht_result.valid = 0; 
  
  Serial.println("reading DHT22");
  while ((i++ < 5) && (!dht_result.valid)) {
    h = dht.readHumidity(); //Read Humidity
    t = dht.readTemperature(); //Read Temperature
    if (isnan(t) || isnan(h)) {
      delay(100);
      h = dht.readHumidity(); //Read Humidity
      t = dht.readTemperature(false); //Read Temperature
      
      Serial.println("T: " + String(t) + " H: " + String(h));
    }
    if (isnan(t) || isnan(h)) {
      Serial.println("DHT22 couldn't be read");
    } else {
      Serial.println("Temperature" + String(t));
      Serial.println("Humidity" + String(h));
      dht_result.h = h; 
      dht_result.t = t;
      dht_result.valid = 1; 
    }
  }
}

void readSDS() {
  int pm25, pm10;

  Serial.println("Reading SDS011\n");
  sensorSDS.set_sleep(false);
  delay(1000);

  sds011_result.valid = sensorSDS.query_data_auto(&pm25, &pm10, 10);

  if (sds011_result.valid) {
    Serial.println("PM2.5: %d\n"+ String(pm10));
    Serial.println("PM10:  %d\n"+ String(pm25));

    sds011_result.pm10 = float(pm10)/10;
    sds011_result.pm25 = float(pm25)/10;
  } else {
    Serial.println("failed to read\n");
  }

  sensorSDS.set_sleep(true);
}

void connectWifi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); 

  while(WiFi.status() != WL_CONNECTED){
    delay(500); 
    Serial.println("Waiting for connection"); 
  }

  Serial.println("Connection succesfull"); 
}

void sendData(const String& body){
  if(WiFi.status() == WL_CONNECTED){
    Serial.println(body);
    WiFiClientSecure client;
    client.setInsecure(); 
    //client.setFingerprint(fingerPrint);

    if(!client.connect(host, 443)){
      Serial.println("ups");
      return;
    }
    client.println(String("POST ") + path + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" +
             "Content-Type: application/json\r\n" +
             "Content-Length: "+ body.length() + "\r\n");
    client.print(body);

    delay(10);
    
    while(client.available()){
      char c = client.read();
      Serial.print(c);
    }
    /*HTTPClient http; 
    http.begin(path, fingerPrint); 
    http.addHeader("Content-Type", "application/json"); 

    int httpCode = http.POST(body);
    String payload = http.getString();  

    Serial.println(httpCode);
    Serial.println(payload);
    http.end(); */
  }else{
    Serial.println("Error in wifi connection"); 
    connectWifi();
  }

  yield(); 
}

void readAndSend(){
  if(sds_read) readSDS(); 
  readDHT(); 
  
  String data = F("{");

  if (sds_read && sds011_result.valid) {
    data += Value2Json(F("Id"), 1000); 
    data += Value2Json(F("Location"), F("Bolivar")); 
    data += Value2Json(F("PM10"), sds011_result.pm10);
    data += Value2Json(F("PM2"), sds011_result.pm25);
  }

  if(dht_result.valid){
    data += Value2Json(F("Temperature"), dht_result.t);
    data += Value2Json(F("Humidity"), dht_result.h);
  }
  if (!data.endsWith(",")){
    Serial.println("No data available\n");
    return;
  }

  data.remove(data.length()-1);
  data += "}";
  sendData(data);
  
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  serialSDS.begin(9600); 

  connectWifi(); 

  
  
}

void loop() {
  // put your main code here, to run repeatedly:
  now = millis();
  bool send_now = starttime == 0 || (now-starttime) > sending_interval;
  if(send_now){
    readAndSend();
    starttime = now;   
  }
   
  yield();  

}
