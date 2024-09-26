#include <FastLED.h>
#include "Dots5x5font.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DFRobot_MaqueenPlus.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
//#include <SPI.h>
//#include <MFRC522.h>
// Include the config file
#include "config.h"

// **************** global app variables *********************

String dir = "stop";  // default direction
int motorSpeed = 50;  // default speed
String taskId = "0";  // default task Id

// **************** WiFi / Web parameters *********************
WebServer server(8000);
HTTPClient http;  // create HTTP client

// **************** RFID parameters *********************
// RFID pin definitions for Mbits (ESP32-based)
#define RST_PIN 26  // Configurable, see typical pin layout above
#define SS_PIN 32   // Configurable, see typical pin layout above

//MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

// **************** Mbits I2C settings *********************
// define I2C data and clock pins used on Mbits ESP32 board
#define I2C_SDA 22
#define I2C_SCL 21

// create a pointer to DFRobot_MaqueenPlus object
// --> this is done because we need to manually set I2C pins and create the DFRobot_MaqueenPlus object inside the setup() function
// --> a DFRobot_MaqueenPlus object is accessed via a pointer throughout the program
DFRobot_MaqueenPlus* mp;
// a TwoWire object is needed for manual I2C pins definition
TwoWire i2cCustomPins = TwoWire(0);

// **************** Mbits LED matrix settings *********************
#define NUM_ROWS 5
#define NUM_COLUMNS 5
#define NUM_LEDS (NUM_ROWS * NUM_COLUMNS)
#define LED_PIN 13
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGBArray<NUM_LEDS> leds;
uint8_t max_bright = 10;
// definition of display colors
CRGB myRGBcolor_zyvx(255, 0, 0);
const uint8_t maxBitmap_zyvx[] = {
  B00000, B11011, B00000, B10001, B01110
};
CRGB myRGBcolor_x6aa(16, 255, 0);
const uint8_t maxBitmap_x6aa[] = {
  B00000, B11011, B00000, B10001, B01110
};

void plotMatrixChar(CRGB (*matrix)[5], CRGB myRGBcolor, int x, char character, int width, int height) {
  int y = 0;
  if (width > 0 && height > 0) {
    int charIndex = (int)character - 32;
    int xBitsToProcess = width;
    for (int i = 0; i < height; i++) {
      byte fontLine = FontData[charIndex][i];
      for (int bitCount = 0; bitCount < xBitsToProcess; bitCount++) {
        CRGB pixelColour = CRGB(0, 0, 0);
        if (fontLine & 0b10000000) {
          pixelColour = myRGBcolor;
        }
        fontLine = fontLine << 1;
        int xpos = x + bitCount;
        int ypos = y + i;
        if (xpos < 0 || xpos > 10 || ypos < 0 || ypos > 5)
          ;
        else {
          matrix[xpos][ypos] = pixelColour;
        }
      }
    }
  }
}

void ShowChar(char myChar, CRGB myRGBcolor) {
  CRGB matrixBackColor[10][5];
  int mapLED[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };
  plotMatrixChar(matrixBackColor, myRGBcolor, 0, myChar, 5, 5);
  for (int x = 0; x < NUM_COLUMNS; x++) {
    for (int y = 0; y < NUM_ROWS; y++) {
      int stripIdx = mapLED[y * NUM_COLUMNS + x];
      // Serial.println("index:" + String(stripIdx) + ", value: " + String(matrixBackColor[x][y]));
      leds[stripIdx] = matrixBackColor[x][y];
    }
  }
  FastLED.show();
  FastLED.delay(30);
}

void ShowString(String sMessage, CRGB myRGBcolor) {
  CRGB matrixBackColor[10][5];
  int mapLED[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };
  int messageLength = sMessage.length();
  Serial.println("string length:" + String(messageLength));
  for (int x = 0; x < messageLength; x++) {
    char myChar = sMessage[x];
    plotMatrixChar(matrixBackColor, myRGBcolor, 0, myChar, 5, 5);
    for (int sft = 0; sft <= 5; sft++) {
      for (int x = 0; x < NUM_COLUMNS; x++) {
        for (int y = 0; y < 5; y++) {
          int stripIdx = mapLED[y * 5 + x];
          if (x + sft < 5) {
            leds[stripIdx] = matrixBackColor[x + sft][y];
          } else {
            leds[stripIdx] = CRGB(0, 0, 0);
          }
        }
      }
      FastLED.show();
      FastLED.delay(100);
    }
  }
}

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
  i2cCustomPins.begin(I2C_SDA, I2C_SCL, 50000);

  Serial.println("construct MaqueenPlus object");
  mp = new DFRobot_MaqueenPlus(&i2cCustomPins, 0x10);

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

void setupLEDMatrix() {
  // initialize ESP32 board LED matrix
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(max_bright);
}

// void setupRFID() {
//   SPI.begin();
//   mfrc522.PCD_Init();
//   delay(4);                           // Optional delay. Some board do need more time after init to be ready
//   mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
//   Serial.println("RFID reader initialized. Scan an RFID tag...");
// }

// Function to move the robot car forward
// This function sets both motors to move forward (clockwise) at a given speed.
void moveForward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving forward at speed " + String(moveSpeed));
  ShowChar('F', myRGBcolor_zyvx);
  mp->setRGB(mp->eALL, mp->eRED);
  mp->motorControl(mp->eALL, mp->eCW, moveSpeed);
}

// Function to move the robot car backward
// This function sets both motors to move backward (counterclockwise) at a given speed.
void moveBackward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving backward at speed " + String(moveSpeed));
  ShowChar('B', myRGBcolor_zyvx);
  mp->setRGB(mp->eALL, mp->eRED);
  mp->motorControl(mp->eALL, mp->eCCW, moveSpeed);
}

// Function to turn the robot car to the left
// This function sets the left motor to move backward (counterclockwise)
// and the right motor to move forward (clockwise) to achieve a left turn.
void turnLeft() {
  int turnSpeed = constrain(motorSpeed, 0, 50);  // Limit speed to
  Serial.println("turning left at speed " + String(turnSpeed));
  ShowChar('L', myRGBcolor_zyvx);
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
  ShowChar('R', myRGBcolor_zyvx);
  mp->setRGB(mp->eRIGHT, mp->eRED);
  mp->setRGB(mp->eLEFT, mp->eNO);
  mp->motorControl(mp->eLEFT, mp->eCW, turnSpeed);
  mp->motorControl(mp->eRIGHT, mp->eCCW, turnSpeed);
}

// Function to stop both motors of the robot car
// This function stops all movement by setting both motors to 0 speed.
void stop() {
  //Serial.println("stopping");
  ShowChar('S', myRGBcolor_zyvx);
  mp->setRGB(mp->eALL, mp->eNO);
  mp->motorControl(mp->eALL, mp->eCW, 0);
  mp->motorControl(mp->eALL, mp->eCCW, 0);
}

// String readRFID() {
//   if (!mfrc522.PICC_IsNewCardPresent()) return "";  // Return empty string if no card is present
//   if (!mfrc522.PICC_ReadCardSerial()) return "";    // Return empty string if reading fails

//   // Convert UID to a string
//   String uidString = "";
//   for (byte i = 0; i < mfrc522.uid.size; i++) {
//     if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0";  // Add leading zero for single hex digit
//     uidString += String(mfrc522.uid.uidByte[i], HEX);     // Convert each byte to a hex string
//   }

//   uidString.toUpperCase();  // Convert to uppercase for consistency
//   mfrc522.PICC_HaltA();     // Halt PICC (Proximity Integrated Circuit Card)

//   return uidString;  // Return the UID as a string
// }

// void sendUIDToServer(String uid) {
//   // Construct the URL
//   String url = String(studentAppIP) + String(endpoint) + "?uid=" + uid;
//   Serial.println("Sending UID to server: " + url);

//   http.begin(url);                    // Specify the URL
//   int httpResponseCode = http.GET();  // Send the request

//   if (httpResponseCode > 0) {
//     Serial.println("HTTP Response code: " + String(httpResponseCode));
//     String payload = http.getString();
//     Serial.println("Response from server: " + payload);
//   } else {
//     Serial.println("Error on sending GET: " + String(httpResponseCode));
//   }
//   http.end();  // Free resources
// }

void setup() {
  // initialize serial print
  Serial.begin(115200);

  // Step 1: Set up WiFi connection
  setupWiFi();

  // Step 2: Initialize I2C communication
  setupI2C();

  // Step 3: Initialize RFID reader
 // setupRFID();

  // Step 4: Set up the web server
  setupWebServer();

  // Step 5: Initialize the LED matrix
  setupLEDMatrix();

  // Additional initialization for the MaqueenPlus robot car
  mp->setRGB(mp->eALL, mp->eNO);
  // enable MaqueenPlus PID operation control
  mp->PIDSwitch(mp->eON);
}

void loop() {

  // handle incoming requests
  server.handleClient();

  // String uuid = readRFID();
  // if (uuid != "") {  // Check if a valid UID is read
  //   Serial.println("Detected RFID Tag UID: " + uuid);
  //   sendUIDToServer(uuid);  // Send UID to the server
  // }

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
