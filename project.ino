#include <Wire.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#define I2C_SDA 4
#define I2C_SCL 5
#define LM73_ADDR 0x4D

const char* ssid = "true_home2G_110";
const char* password = "bpsadgk4";

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
  server.on("/led", HTTP_POST, handlePost);
 
  // start server
  server.begin();
  server.enableCORS(true);
}

void handlePost() {
  if (server.hasArg("plain") == false) {
    Serial.println("error");
  }
  Serial.println("toggle LED");
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  
  // Get data
  String status = jsonDocument["led"];
  Serial.println("Get open led: " + status);

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

void setup() {     
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.begin(115200);  

  connectToWiFi();
  setup_routing();
}    
       
void loop() {    
  server.handleClient();
}
