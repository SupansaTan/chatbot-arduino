#include <Wire.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <TridentTD_LineNotify.h>
#include <WiFi.h>
#include "time.h"

unsigned long buzz_lasttime = 0; //ระยะเวลาระพริบ buzzer
unsigned long last_time = 0; // ใช้สำหรับเทียบเวลาเพื่อ TempCheck
unsigned long Timer_lasttime = 0; // ใช้สำหรับเทียบเวลาแจ้งเตือน Timer

#define I2C_SDA 4
#define I2C_SCL 5
#define LM73_ADDR 0x4D

#define RTC_ADDR 0x6F
#define RTC_LOCATION 0x00

#define LINE_TOKEN  "aiMrxdEESK9zqof7STDcDcqbsLI7KSWBs92q9hqJ6Ik"   // token line notify

const char* ssid = "POM";
const char* password = "0800778528";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 25200;

// Assign output variables to GPIO pins
const int output26 = 26;
const int output27 = 27;
const int buzzer = 13;
const int ch = 0;

boolean buzStatus = false;
boolean buzz_sound = false;
boolean led_bool = false;
boolean Timer_bool = false;

int alarmTemp = 50;
int Timer_Time = 0;

// Web server running on port 80
// 192.168.1.43 (ตาม IP ที่ได้)
WebServer server(80);

// JSON data buffer
StaticJsonDocument<250> jsonDocument;
char buffer[250];

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
 
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void setup_routing() {
  server.on("/temp", HTTP_GET,getTemperature);
  server.on("/temp", HTTP_POST, setTemperature);
  server.on("/led", HTTP_POST, controlLED);
  server.on("/light", HTTP_GET, getLight);
  server.on("/datetime", HTTP_GET, getDateTime);
  server.on("/timer", HTTP_POST, setTimer);
 
  // start server
  server.begin();
  server.enableCORS(true);
}

void controlLED() {
  if (server.hasArg("plain") == false) {
    Serial.println("error");
  }
  Serial.println("toggle LED");
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  
  // Get data
  String led_status = jsonDocument["led"];
  Serial.println("LED Status: " + led_status);
  if (led_status == "ON"){
      digitalWrite(output26, HIGH);
      digitalWrite(output27, HIGH);
  }
  else if (led_status == "OFF"){
      digitalWrite(output26, LOW);
      digitalWrite(output27, LOW);
  }

  // Respond to the client
  server.send(200, "application/json", "{}");
}

void getTemperature() {
  Serial.println("Get temperature");

  // create json for response
  jsonDocument.clear();
  jsonDocument["title"] = "temperature";
  jsonDocument["value"] = getTempFromSensor();
  serializeJson(jsonDocument, buffer);

  server.send(200, "application/json", buffer);
}

void setTemperature() {
  Serial.println("Set temperature");
  if (server.hasArg("plain") == false) {
    Serial.println("error");
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  
  // Get data
  Serial.print("old temperature");
  Serial.println(alarmTemp);
  alarmTemp = jsonDocument["temp"];
  Serial.print("Set temperature");
  Serial.println(alarmTemp);

  // Respond to the client
  server.send(200, "application/json", "{}");
}

void getLight() {
  Serial.println("Get light");

  // create json for response
  jsonDocument.clear();
  jsonDocument["title"] = "light";
  jsonDocument["value"] = getLightFromSensor();
  serializeJson(jsonDocument, buffer);

  server.send(200, "application/json", buffer);
}

void getDateTime(){
  Serial.println("Get DateTime");
  // create json for response
  jsonDocument.clear();
  jsonDocument["datetime"] = getDatetimeFromRTC();
  serializeJson(jsonDocument, buffer);

  server.send(200, "application/json", buffer);
}

void setTimer() {
  Serial.println("Set Timer");
  if (server.hasArg("plain") == false) {
    Serial.println("error");
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  
  // Get data
  Timer_Time = jsonDocument["time"];
  Timer_Time = Timer_Time *1000;
  Timer_lasttime = millis();          // set lasttime = now
  Timer_bool = true;
  // Respond to the client
  server.send(200, "application/json", "{}");
}

float getTempFromSensor() {
  byte TempBin[2];
  unsigned int tempVal;

  // write to the LM73
  Wire.beginTransmission(LM73_ADDR);
  Wire.write(0);
  Wire.endTransmission();

  // Reading from the LM73
  Wire.requestFrom(LM73_ADDR, 2);
  TempBin[1] = Wire.read();
  TempBin[0] = Wire.read();

  // temp value in 2 bytes
  tempVal = ((TempBin[1]<<8) | TempBin[0]) >> 5;
  Serial.print("Temp : ");
  Serial.print(tempVal * 0.25);
  Serial.println();

  // convert from binary to celsius (°C)   
  return tempVal * 0.25;
}

int getLightFromSensor(){
  int analog_value = analogRead(36);
  Serial.print("Light : ");
  Serial.print(analog_value);
  Serial.println();
  return analog_value;
}

void checkTemp(){
  float temp = getTempFromSensor();
  if (temp >= alarmTemp) {                    // if temp >= temp ที่กำหนดไว้ให้แจ้งเตือน
//    Serial.println("Temperature is over " + String(alarmTemp) + " C");
    LINE.notify("Temperature is over " + String(alarmTemp) + " C");
    buzStatus = true;
    ledcWrite(ch,25);
    ledcWriteNote(ch, NOTE_F, 4);
  }
}

// Set Current time //
byte DecToBcd(byte num){
 return ( (num/10*16) + (num%10) );
}

byte BcdToDec(byte bin){
 return ( (bin/16*10) + (bin%16) );
}

void setDateTime(byte sec, byte mint,byte hour, byte wday, byte date, byte month, byte year){
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(RTC_LOCATION);
  Wire.write(DecToBcd(sec));
  Wire.write(DecToBcd(mint));
  Wire.write(DecToBcd(hour));
  Wire.write(DecToBcd(wday));
  Wire.write(DecToBcd(date));
  Wire.write(DecToBcd(month));
  Wire.write(DecToBcd(year));
  Wire.endTransmission();

  // start clock
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(RTC_LOCATION);
  Wire.write(DecToBcd(sec)| 0b10000000);
  Wire.endTransmission();
}

String getDatetimeFromRTC(){
  byte dayOfWeek, date, month, year, hour, minute, sec;
  String mm, ss;

  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(RTC_ADDR,7);

  sec = BcdToDec(Wire.read() & 0x7F);
  minute = BcdToDec(Wire.read() & 0x7F);
  hour = BcdToDec(Wire.read() & 0x3F);
  dayOfWeek = BcdToDec(Wire.read() & 0x07);
  date = BcdToDec(Wire.read() & 0x3F);
  month = BcdToDec(Wire.read() & 0x1F);
  year = BcdToDec(Wire.read());

  if (minute < 10){
    mm = "0";
  }
  if (sec < 10){
    ss = "0";
  }
  mm += String(minute,DEC);
  ss += String(sec,DEC);

  return String(date,DEC) + "/" + String(month,DEC) + "/" + String(year,DEC) + " " + String(hour,DEC) + ":" + mm + ":" + ss;
}

void setup() {     
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.begin(4, 5);
  Serial.begin(115200);  
  pinMode(output26, OUTPUT);  // IO26
  pinMode(output27, OUTPUT);  // IO27

  //Buzzer
  ledcSetup(ch,0,8);
  ledcAttachPin(buzzer,ch);
  
  // Set outputs to LOW
  digitalWrite(output26, LOW);
  digitalWrite(output27, LOW);
  digitalWrite(buzzer, LOW);
  
  LINE.setToken(LINE_TOKEN);

  connectToWiFi();
  setup_routing();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);       // get current time from ntpserver

  int mday, mon, year, sec, min, hour, wday;
  
  struct tm timeinfo;
  while(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }
  
  mday = timeinfo.tm_mday;
  mon = timeinfo.tm_mon+1;
  year = timeinfo.tm_year+1900-2000;
  sec = timeinfo.tm_sec;
  min = timeinfo.tm_min;
  hour = timeinfo.tm_hour;
  wday = timeinfo.tm_wday+1;
  
  setDateTime(sec,min,hour,wday,mday,mon,year);
}    
       
void loop() {    
  server.handleClient();
  
  // เตือนอุณหภูมิสูงทุกๆ 10 วินาที (สำหรับเทส ตอนจริงเปลี่ยนเวลาได้)
  if( millis() - last_time > 10000) {
     last_time = millis();
     Serial.println("Check Temp");
     checkTemp();
  }
  
  // buzzer เตือนอุณหภูมิสูงดัง 4 วินาทีแล้วดับ
  if (buzStatus) {
    if( millis() - last_time > 4000) {
      buzStatus = false;
      ledcWriteTone(ch,0);
     Serial.println("Buzzer Alarm Finish");
    }
    else if( millis() - buzz_lasttime > 500) {
      buzz_lasttime = millis();
      if (buzz_sound){
        ledcWriteTone(ch,0);
      }
      else{
        ledcWriteNote(ch, NOTE_F, 4);
      }
    }
  }

  // check Timer finish
  if (Timer_bool){
    if( millis() - Timer_lasttime > Timer_Time) {
//     Serial.print("timediff ");
//     Serial.println(millis() - Timer_lasttime);
//     Serial.print("timer set ");
//     Serial.println(Timer_Time);
     ledcWriteNote(ch, NOTE_C, 5);
     Timer_bool = false;
     LINE.notify("Time is up");
     delay(500);
     ledcWriteTone(ch,0);
    }
  }
  
  
  
}
