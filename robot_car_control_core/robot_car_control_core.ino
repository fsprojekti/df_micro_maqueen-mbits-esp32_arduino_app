#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DFRobot_MaqueenPlus.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <PN532.h> // lib for PN532 NFC/RFID reader
#include <PN532_I2C.h> // I2C communication for PN532 NFC/RFID reader
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_err.h>

// Include the config file
#include "config.h"


// **************** global app variables *********************
String dir = "stop";  // default direction
int motorSpeed = 50;  // default speed
String taskId = "0";  // default task Id

// **************** WiFi / Web parameters *********************
WebServer server(8000);
HTTPClient http;  // create HTTP client

// **************** Mbits I2C settings *********************
// define I2C data and clock pins used on Mbits ESP32 board
#define I2C_SDA 22
#define I2C_SCL 21

// **************** RFID parameters *********************
// #define PN532_I2C_ADDRESS (0x48 >> 1)  // Adafruit lib expects 7-bit addr
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

// create a pointer to DFRobot_MaqueenPlus object
// --> this is done because we need to manually set I2C pins and create the DFRobot_MaqueenPlus object inside the setup() function
// --> a DFRobot_MaqueenPlus object is accessed via a pointer throughout the program
DFRobot_MaqueenPlus* mp;
// a TwoWire object is needed for manual I2C pins definition
// TwoWire i2cCustomPins = TwoWire(0);
// NOTE: this is now not needed because we use the normal Wire class


// **************** web server handles *********************
void handleRoot() {

  Serial.println("received a request to /");
  server.send(200, "text/plain", "Hello from DF micro:Maqueen Plus v2!");
}

void handleMove() {

  Serial.println("received an HTTP request to /move");

  // Get the client IP address as an IPAddress object
  IPAddress clientIP = server.client().remoteIP();

  // Convert the IPAddress to a String
  String clientIPStr = clientIP.toString();

  // Copy the String to the studentAppIP char array
  clientIPStr.toCharArray(studentAppIP, sizeof(studentAppIP));

  // Print the IP address
  Serial.println(studentAppIP);

  StaticJsonDocument<100> JSONdocument;

  // check if all parameters are present
  if (server.arg(0) == "") {
    JSONdocument["status"] = "rejected, missing direction";
    String message;
    serializeJson(JSONdocument, message);
    server.send(200, "text/plain", message);
  } else if (server.arg(1) == "") {
    JSONdocument["status"] = "rejected, missing speed";
    String message;
    serializeJson(JSONdocument, message);
    server.send(200, "text/plain", message);
  } else if (server.arg(2) == "") {
    JSONdocument["status"] = "rejected, missing task id";
    String message;
    serializeJson(JSONdocument, message);
    server.send(200, "text/plain", message);
  }

  else {

    dir = server.arg(0);
    motorSpeed = server.arg(1).toInt();
    taskId = server.arg(2);

    Serial.println("received a task to move, direction: " + dir + ", motor speed: " + String(motorSpeed) + ", task id: " + String(taskId));

    if (dir != "forward" && dir != "backward" && dir != "left" && dir != "right" && dir != "stop") {
      JSONdocument["status"] = "reject, invalid direction";
      String message;
      serializeJson(JSONdocument, message);
      server.send(200, "text/plain", message);
    } else {
      JSONdocument["status"] = "accept";
      String message;
      serializeJson(JSONdocument, message);
      server.send(200, "text/plain", message);
    }
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

// Wiggle SCL to release SDA if PN532 or another device is holding it low
static void i2cBusUnstick(int sdaPin, int sclPin) {
  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, OUTPUT);

  // If SDA is stuck low, pulse SCL up to 16 times
  for (int i = 0; i < 16 && digitalRead(sdaPin) == LOW; i++) {
    digitalWrite(sclPin, LOW);
    delayMicroseconds(5);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
  }

  // Send a STOP condition
  pinMode(sdaPin, OUTPUT);
  digitalWrite(sdaPin, HIGH);
  delayMicroseconds(5);

  // Release pins
  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, INPUT_PULLUP);
}

// Choose any LAA MAC you like; keep it unique on your LAN.
static const uint8_t FALLBACK_MAC[6] = { 0x02, 0xA1, 0xB2, 0xC3, 0xD4, 0x15 };

static void ensureWifiMac() {
  // Try to read factory MAC; if it fails, set our own base MAC before WiFi starts.
  uint8_t tmp[6];
  esp_err_t ok = esp_efuse_mac_get_default(tmp);  // just a probe
  if (ok != ESP_OK) {
    // Install base MAC for both STA/AP derivations
    esp_base_mac_addr_set(FALLBACK_MAC);
  }
}

void setupWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Maximum number of attempts to connect to WiFi
  const int maxRetries = 20;
  int retryCount = 0;  // Counter for retries

  // Start WiFi connection
  WiFi.begin(ssid, password);

  // Wait for connection with a timeout and retry mechanism
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");  // Indicate connection attempt
    retryCount++;

    if (retryCount > maxRetries) {  // If max retries reached, break the loop
      Serial.println("Failed to connect to WiFi after multiple attempts.");
      // Handle failure (e.g., restart ESP or go into a low-power mode)
      ESP.restart();  // Restart the ESP32 (optional, depending on desired behavior)
      return;         // Exit the function
    }
  }

  // Connected to WiFi
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());

  // Set up mDNS responder
  if (MDNS.begin("esp32")) {  // Set mDNS hostname
    Serial.println("MDNS responder started");
  } else {
    Serial.println("Error starting MDNS responder. Retrying...");
    // Retry mDNS setup if it fails
    for (int i = 0; i < 3; i++) {  // Retry 3 times
      if (MDNS.begin("esp32")) {
        Serial.println("MDNS responder started after retry.");
        break;
      }
      delay(1000);  // Wait 1 second before retrying
    }
  }
}

void setupI2C() {
  Serial.println("define I2C pins");

  // Try to release the bus before starting Wire
  i2cBusUnstick(I2C_SDA, I2C_SCL);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(50000);  // 50kHz for PN532 stability

  Serial.println("construct MaqueenPlus object");
  mp = new DFRobot_MaqueenPlus(&Wire, 0x10);

  Serial.println("initialize MaqueenPlus I2C communication");
  mp->begin();
}

void setupWebServer() {
  // Define web server API endpoints
  server.on("/", handleRoot);         // Route for root
  server.on("/move", handleMove);     // Route for move command
  server.onNotFound(handleNotFound);  // Route for 404 errors

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
}

void setupRFID() {
  Serial.println("Setting up PN532 (I2C) ...");

  //resetPN532();
  nfc.begin();
  delay(200);

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532 board");
    while (1); // halt if not found
  }

  Serial.print("Found PN532 firmware ver. ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print(".");
  Serial.println((versiondata >> 8) & 0xFF, DEC);

  nfc.SAMConfig();  // configure to read passive targets
  Serial.println("PN532 initialized (I2C). Scan an RFID/NFC tag...");
}

String readRFID() {
  uint8_t uid[7];
  uint8_t uidLength;

  // timeout 500ms to avoid blocking
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 500)) {
    String uidString = "";
    for (byte i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) uidString += "0";
      uidString += String(uid[i], HEX);
    }
    uidString.toUpperCase();
    return uidString;
  }
  return ""; // no card
}

// Function to move the robot car forward
// This function sets both motors to move forward (clockwise) at a given speed.
void moveForward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving forward at speed " + String(moveSpeed));
  //ShowChar('F', myRGBcolor_zyvx);
  mp->setRGB(mp->eALL, mp->eRED);
  mp->motorControl(mp->eALL, mp->eCW, moveSpeed);
}

// Function to move the robot car backward
// This function sets both motors to move backward (counterclockwise) at a given speed.
void moveBackward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving backward at speed " + String(moveSpeed));
  //ShowChar('B', myRGBcolor_zyvx);
  mp->setRGB(mp->eALL, mp->eRED);
  mp->motorControl(mp->eALL, mp->eCCW, moveSpeed);
}

// Function to turn the robot car to the left
// This function sets the left motor to move backward (counterclockwise)
// and the right motor to move forward (clockwise) to achieve a left turn.
void turnLeft() {
  int turnSpeed = constrain(motorSpeed, 0, 50);  // Limit speed to
  Serial.println("turning left at speed " + String(turnSpeed));
  //ShowChar('L', myRGBcolor_zyvx);
  mp->setRGB(mp->eLEFT, mp->eRED);
  mp->setRGB(mp->eRIGHT, mp->eNO);
  mp->motorControl(mp->eLEFT, mp->eCCW, turnSpeed);
  mp->motorControl(mp->eRIGHT, mp->eCW, turnSpeed);
}

// Function to turn the robot car to the right
// This function sets the right motor to move backward (counterclockwise)
// and the left motor to move forward (clockwise) to achieve a right turn.
void turnRight() {
  int turnSpeed = constrain(motorSpeed, 0, 50);  // Limit speed to 50
  Serial.println("turning right at speed " + String(turnSpeed));
  //ShowChar('R', myRGBcolor_zyvx);
  mp->setRGB(mp->eRIGHT, mp->eRED);
  mp->setRGB(mp->eLEFT, mp->eNO);
  mp->motorControl(mp->eLEFT, mp->eCW, turnSpeed);
  mp->motorControl(mp->eRIGHT, mp->eCCW, turnSpeed);
}

// Function to stop both motors of the robot car
// This function stops all movement by setting both motors to 0 speed.
void stop() {
  //Serial.println("stopping");
  //ShowChar('S', myRGBcolor_zyvx);
  mp->setRGB(mp->eALL, mp->eNO);
  mp->motorControl(mp->eALL, mp->eCW, 0);
  mp->motorControl(mp->eALL, mp->eCCW, 0);
}

void sendUIDToServer(String uid) {
  // Construct the URL
  String url = String(studentAppIP) + String(endpoint) + "?uid=" + uid;
  Serial.println("Sending UID to server: " + url);

  http.begin(url);                    // Specify the URL
  int httpResponseCode = http.GET();  // Send the request

  if (httpResponseCode > 0) {
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    String payload = http.getString();
    Serial.println("Response from server: " + payload);
  } else {
    Serial.println("Error on sending GET: " + String(httpResponseCode));
  }
  http.end();  // Free resources
}

void setup() {
  // initialize serial print
  Serial.begin(115200);

  ensureWifiMac();

  // Step 1: Set up WiFi connection
  setupWiFi();

  // Step 2: Initialize I2C communication
  setupI2C();

  // Step 3: Initialize RFID reader
  delay(250);  
  setupRFID();

  // Step 4: Set up the web server
  setupWebServer();

  // Step 5: Initialize the LED matrix
  //setupLEDMatrix();

  // Additional initialization for the MaqueenPlus robot car
  mp->setRGB(mp->eALL, mp->eNO);
  // enable MaqueenPlus PID operation control
  mp->PIDSwitch(mp->eON);
}

void loop() {

  // handle incoming requests
  server.handleClient();

  String uuid = readRFID();
  if (uuid != "") {  // Check if a valid UID is read
    Serial.println("Detected RFID Tag UID: " + uuid);
    //sendUIDToServer(uuid);  // Send UID to the server
  }

  if (dir == "forward") {

    moveForward();
  } else if (dir == "backward") {
    moveBackward();
  } else if (dir == "left") {
    turnLeft();
  } else if (dir == "right") {
    turnRight();
  } else if (dir == "stop") {
    stop();
  }
}