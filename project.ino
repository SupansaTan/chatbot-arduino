#include <Wire.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <TridentTD_LineNotify.h>

unsigned long period = 10000; //ระยะเวลาที่ต้องการรอ TempCheck
unsigned long last_time = 0; // ใช้สำหรับเทียบเวลาเพื่อ TempCheck

#define I2C_SDA 4
#define I2C_SCL 5
#define LM73_ADDR 0x4D

#define RTC_ADDR 0x6F
#define RTC_LOCATION 0x00

#define LINE_TOKEN  "aiMrxdEESK9zqof7STDcDcqbsLI7KSWBs92q9hqJ6Ik"   // token line notify

const char* ssid = "true_home2G_110";
const char* password = "bpsadgk4";

// Assign output variables to GPIO pins
const int output26 = 26;
const int output27 = 27;
const int buzzer = 13;

boolean buzStatus = false;

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
  server.on("/led", HTTP_POST, controlLED);
  server.on("/light", HTTP_GET, getLight);
 
  // start server
  server.begin();
  server.enableCORS(true);
}

void controlLED() {
  Serial.println("Control LED");
  if (server.hasArg("plain") == false) {
    Serial.println("error");
  }
  Serial.println("toggle LED");
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  
  // Get data
  String led_status = jsonDocument["led"];
  Serial.println("Get open led: " + led_status);
  if (led_status == "ON"){
    digitalWrite(output26, HIGH);
    digitalWrite(output27, HIGH);
  }
  else {
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

void getLight() {
  Serial.println("Get light");

  // create json for response
  jsonDocument.clear();
  jsonDocument["title"] = "light";
  jsonDocument["value"] = getLightFromSensor();
  serializeJson(jsonDocument, buffer);

  server.send(200, "application/json", buffer);
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
  if (temp >= 32) {
    buzStatus = true;
    digitalWrite(buzzer,HIGH);
    Serial.println("Temperature is over 32 C");
    LINE.notify("Temperature is over 32 C");
  }
}

void setup() {     
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.begin(115200);  
  pinMode(output26, OUTPUT);  // IO26
  pinMode(output27, OUTPUT);  // IO27
  pinMode(buzzer, OUTPUT);    // Buzzer
  // Set outputs to LOW
  digitalWrite(output26, LOW);
  digitalWrite(output27, LOW);
  digitalWrite(buzzer, LOW);
  
  LINE.setToken(LINE_TOKEN);

  connectToWiFi();
  setup_routing();
}    
       
void loop() {    
  server.handleClient();
  // เตือนอุณหภูมิสูงทุกๆ 10 วินาที (สำหรับเทส ตอนจริงเปลี่ยนเวลาได้)
  if( millis() - last_time > period) {
     last_time = millis();
     Serial.println("Check Temp");
     checkTemp();
  }
  // buzzer เตือนอุณหภูมิสูงดัง 5 วินาทีแล้วดับ
  if (buzStatus == true) {
    if( millis() - last_time > 5000) {
     buzStatus = false;
     digitalWrite(buzzer, LOW);
     Serial.println("Buzzer Alarm Finish");
    }
  }
}
