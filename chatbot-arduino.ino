#include <Wire.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <TridentTD_LineNotify.h>
#include <WiFi.h>
#include <time.h>

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
const int buzzer = 13;
const int ch = 0;

boolean buzStatus = false;
boolean buzz_sound = false;
boolean led_bool = false;
boolean led_com_bool = false;
boolean Timer_bool = false;
boolean setStatusLED = false;
boolean hasSetDatetimeLed = false;

int alarmTemp = 50;
int Timer_Time = 0;
String datetimeToggleLed = "";

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
  server.on("/led", HTTP_GET, getLedStatus);
  server.on("/led", HTTP_POST, controlLED);
  server.on("/setDateTimeLed", HTTP_POST, setTimeToggleLED);
  server.on("/light", HTTP_GET, getLight);
  server.on("/datetime", HTTP_GET, getDateTime);
  server.on("/timer", HTTP_POST, setTimer);
 
  // start server
  server.begin();
  server.enableCORS(true);
}

void getLedStatus(){
  Serial.println("Get LedStatus");

  // create json for response
  jsonDocument.clear();
  jsonDocument["title"] = "temperature";
  if (led_com_bool){
    jsonDocument["value"] = "ON";
  }
  else {
    jsonDocument["value"] = "OFF";
  }
  serializeJson(jsonDocument, buffer);

  server.send(200, "application/json", buffer);
}

void controlLED() {
  if (server.hasArg("plain") == false) {
    Serial.println("error");
    server.send(500, "application/json", "{}");
  }
  Serial.println("toggle LED");
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  
  // Get data
  String led_status = jsonDocument["led"];
  Serial.println("LED Status: " + led_status);
  if (led_status == "ON"){
    digitalWrite(output26, HIGH);
    led_com_bool = true;
    server.send(200, "application/json", "{}");
  }
  else if (led_status == "OFF"){
    digitalWrite(output26, LOW);
    led_com_bool = false;
    server.send(200, "application/json", "{}");
  }
  else {
    server.send(500, "application/json", "{}");
  }
}

void setTimeToggleLED() {
  if (server.hasArg("plain") == false) {
    Serial.println("error");
    server.send(500, "application/json", "{}");
  }
  Serial.println("toggle LED");
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  
  // Get data
  String led_status = jsonDocument["led"];
  String datetime = jsonDocument["datetime"];
  hasSetDatetimeLed = true;
  datetimeToggleLed = datetime;
  Serial.println(datetimeToggleLed);
  
  if (led_status == "ON"){
    setStatusLED = true;
    server.send(200, "application/json", "{}");
  }
  else if (led_status == "OFF"){
    setStatusLED = false;
    server.send(200, "application/json", "{}");
  }
  else {
    server.send(500, "application/json", "{}");
  }
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
    server.send(500, "application/json", "{}");
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  
  // Get data
  alarmTemp = jsonDocument["temp"];

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
    server.send(500, "application/json", "{}");
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
  String hh, mm, ss, MM, dd;

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

  if (month < 10){
    MM = "0";
  }
  if (date < 10){
    dd = "0";
  }
  if (hour < 0){
    hh = "0";
  }
  if (minute < 10){
    mm = "0";
  }
  if (sec < 10){
    ss = "0";
  }
  dd += String(date,DEC);
  MM += String(month,DEC);
  hh += String(hour,DEC);
  mm += String(minute,DEC);
  ss += String(sec,DEC);

  return dd + "/" + MM + "/" + String(year,DEC) + " " + hh + ":" + mm + ":" + ss;
}

void setup() {     
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.begin(4, 5);
  Serial.begin(115200);  
  pinMode(output26, OUTPUT);  // IO26

  //Buzzer
  ledcSetup(ch,0,8);
  ledcAttachPin(buzzer,ch);
  
  // Set outputs to LOW
  digitalWrite(output26, LOW);
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
  int lightIntensity = getLightFromSensor();
  
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
     ledcWriteNote(ch, NOTE_C, 5);
     Timer_bool = false;
     LINE.notify("Time is up");
     delay(500);
     ledcWriteTone(ch,0);
    }
  }

  // turn on led when dark & turn off when bright
  if(led_bool && !hasSetDatetimeLed){
    if (lightIntensity < 700){
      digitalWrite(output26, LOW);
      led_bool = false;
      led_com_bool = false;
    }
  }
  else{
    if (lightIntensity > 700){
      digitalWrite(output26, HIGH);
      led_bool = true;
      led_com_bool = true;
    }
  }
  
  if(hasSetDatetimeLed){
    String now = getDatetimeFromRTC();
    if(now == datetimeToggleLed){
      if(setStatusLED) {
        led_com_bool = true;
        digitalWrite(output26, HIGH);
        LINE.notify("The light is on.");
      }
      else {
        led_com_bool = false;
        digitalWrite(output26, LOW);
        LINE.notify("The light is off.");
      }
    }
  }
}
