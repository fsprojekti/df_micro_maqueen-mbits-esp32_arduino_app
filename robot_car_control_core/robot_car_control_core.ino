#include <FastLED.h>
#include "Dots5x5font.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DFRobot_MaqueenPlus.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// Include the config file
#include "config.h"


// **************** global app variables *********************
// variables for robot cars' infrared sensors (1 means black, 0 means white)
int L1, L2, L3, R1, R2, R3;

String dir = "forward"; // default direction
int motorSpeed = 50; // default speed
String taskId = "0"; // default task Id
const int distanceThreshold = 20; // distance threshold for detecting nearby objects with ultrasound sensors
CRGB whiteColor = CRGB(255, 255, 255); // for showing the text on the board matrix

// **************** WiFi parameters *********************
WebServer server(8000);

// create HTTP client
HTTPClient http;

// student laptop IP and port
String controlAppIp = "192.168.137.1";
int controlAppPort = 3000;

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
        if (xpos < 0 || xpos > 2 * NUM_COLUMNS || ypos < 0 || ypos > NUM_ROWS) continue;
        else {
          matrix[xpos][ypos] = pixelColour;
        }
      }
    }
  }
}

void showChar(char myChar, CRGB myRGBcolor) {
  CRGB matrixBackColor[2 * NUM_COLUMNS][NUM_ROWS];
  int mapLED[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
  plotMatrixChar(matrixBackColor, myRGBcolor, 0 , myChar, NUM_COLUMNS, NUM_ROWS);
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

void showString(String sMessage, CRGB myRGBcolor) {
  CRGB matrixBackColor[2 * NUM_COLUMNS][NUM_ROWS];
  int mapLED[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
  int messageLength = sMessage.length();
  Serial.println("string length:" + String(messageLength));
  for (int x = 0; x < messageLength; x++) {
    char myChar = sMessage[x];
    plotMatrixChar(matrixBackColor, myRGBcolor, 0, myChar, NUM_COLUMNS, NUM_ROWS);
    for (int sft = 0; sft <= NUM_COLUMNS; sft++) {
      for (int x = 0; x < NUM_COLUMNS; x++) {
        for (int y = 0; y < NUM_ROWS; y++) {
          int stripIdx = mapLED[y * NUM_COLUMNS  + x];
          if (x + sft < 2 * NUM_COLUMNS) {
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

  StaticJsonDocument<100> JSONdocument;

  // check if all parameters are present
  if (server.arg(0) == "") {
    JSONdocument["status"] = "rejected, missing direction";
    String message;
    serializeJson(JSONdocument, message);
    server.send(200, "text/plain", message);
  }
  else if (server.arg(1) == "") {
    JSONdocument["status"] = "rejected, missing speed";
    String message;
    serializeJson(JSONdocument, message);
    server.send(200, "text/plain", message);
  }
  else if (server.arg(2) == "") {
    JSONdocument["status"] = "rejected, missing task id";
    String message;
    serializeJson(JSONdocument, message);
    server.send(200, "text/plain", message);
  }

  else {

    dir = server.arg(0);
    motorSpeed = server.arg(1).toInt();
    taskId = server.arg(1);

    Serial.println("received a task to move, direction: " + dir + ", motor speed: " + String(motorSpeed) + ", task id: " + String(taskId));

    if (dir != "forward" && dir != "backward" && dir != "left" && dir != "right" && dir != "stop") {
      JSONdocument["status"] = "reject, invalid direction";
      String message;
      serializeJson(JSONdocument, message);
      server.send(200, "text/plain", message);
    }
    else {
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
      return;  // Exit the function
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
  i2cCustomPins.begin(I2C_SDA, I2C_SCL, 100000);  // Initialize I2C with custom pins

  Serial.println("construct MaqueenPlus object");
  mp = new DFRobot_MaqueenPlus(&i2cCustomPins, 0x10);  // Create MaqueenPlus object with I2C address

  Serial.println("initialize MaqueenPlus I2C communication");
  mp->begin();  // Initialize I2C communication with the MaqueenPlus
}

void setupWebServer() {
  // Define web server API endpoints
  server.on("/", handleRoot);  // Route for root
  server.on("/move", handleMove);  // Route for move command
  server.onNotFound(handleNotFound);  // Route for 404 errors

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
}

void setupLEDMatrix() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);  // Add LED strip
  FastLED.setBrightness(max_bright);  // Set maximum brightness
}

// Function to check ultrasonic sensors for nearby objects and stop the robot if an object is detected
void checkUltrasoundSensors() {
  // Read distance from the ultrasonic sensor
  uint8_t distance = mp->ultraSonic(mp->eP13, mp->eP14);

  // If an object is detected within the threshold distance, stop the robot
  if (distance > 0 && distance < distanceThreshold) {  // 0 means no object detected, so we check for > 0
    Serial.println("Object detected within " + String(distanceThreshold) + " cm. Stopping the robot.");
    stop();  // Call the stop function to stop the robot
  }
}

void setup() {
  // initialize serial print
  Serial.begin(115200);

  // Step 1: Set up WiFi connection
  setupWiFi();

  // Step 2: Initialize I2C communication
  setupI2C();

  // Step 3: Set up the web server
  setupWebServer();

  // Step 4: Initialize the LED matrix
  setupLEDMatrix();

  // Additional initialization for the MaqueenPlus robot car
  mp->setRGB(mp->eALL, mp->eYELLOW);
  // enable MaqueenPlus PID operation control
  mp->PIDSwitch(mp->eON);
}

void loop() {

  // Serial.println(mp->getVersion());
  //  checkAllSensors();

  // Check ultrasonic sensors for nearby objects and stop if detected
  checkUltrasoundSensors();

  // handle incoming requests
  server.handleClient();

  // read values of infrared sensors
  L1 = mp->getPatrol(mp->eL1);
  L2 = mp->getPatrol(mp->eL2);
  L3 = mp->getPatrol(mp->eL3);
  R1 = mp->getPatrol(mp->eR1);
  R2 = mp->getPatrol(mp->eR2);
  R3 = mp->getPatrol(mp->eR3);

  if (dir == "forward") {
    stop();
    moveForward();
  }
  else if (dir == "backward") {
    stop();
    moveBackward();
  }
  else if (dir == "left") {
    stop();
    turnLeft();
  }
  else if (dir == "right") {
    stop();
    turnRight();
  }
  else if (dir == "stop") {
    stop();
  }
}

// Function to move the robot car forward
// This function sets both motors to move forward (clockwise) at a given speed.
void moveForward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving forward at speed " + String(moveSpeed));
  showString("forward, " + String(moveSpeed), whiteColor);
  mp->setRGB(mp->eALL, mp->eGREEN);
  mp->motorControl(mp->eALL, mp->eCW, moveSpeed);
}

// Function to move the robot car backward
// This function sets both motors to move backward (counterclockwise) at a given speed.
void moveBackward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving backward at speed " + String(moveSpeed));
  showString("back, " + String(moveSpeed), whiteColor);
  mp->setRGB(mp->eALL, mp->eBLUE);
  mp->motorControl(mp->eALL, mp->eCCW, moveSpeed);
}

// Function to turn the robot car to the left
// This function sets the left motor to move backward (counterclockwise)
// and the right motor to move forward (clockwise) to achieve a left turn.
void turnLeft() {
  int turnSpeed = constrain(motorSpeed, 0, 50);  // Limit speed to 50
  Serial.println("turning left at speed " + String(turnSpeed));
  showString("left, " + String(turnSpeed), whiteColor);
  mp->setRGB(mp->eALL, mp->eCYAN);
  mp->motorControl(mp->eLEFT, mp->eCCW, turnSpeed);
  mp->motorControl(mp->eRIGHT, mp->eCW, turnSpeed);
}

// Function to turn the robot car to the right
// This function sets the right motor to move backward (counterclockwise)
// and the left motor to move forward (clockwise) to achieve a right turn.
void turnRight() {
  int turnSpeed = constrain(motorSpeed, 0, 50);  // Limit speed to 50
  Serial.println("turning right at speed " + String(turnSpeed));
  showString("right, " + String(turnSpeed), whiteColor);
  mp->setRGB(mp->eALL, mp->eCYAN);
  mp->motorControl(mp->eLEFT, mp->eCW, turnSpeed);
  mp->motorControl(mp->eRIGHT, mp->eCCW, turnSpeed);
}

// Function to stop both motors of the robot car
// This function stops all movement by setting both motors to 0 speed.
void stop() {
  // Serial.println("stopping");
  showString("stop", whiteColor);
  mp->setRGB(mp->eALL, mp->eRED);
  mp->motorControl(mp->eALL, mp->eCW, 0);
  mp->motorControl(mp->eALL, mp->eCCW, 0);
}

// check current values of all relevant sensors
void checkAllSensors() {

  //  unsigned long time1 = micros();
  Serial.println("L1: " + String(mp->getPatrol(mp->eL1)));
  //  unsigned long time2 = micros();
  Serial.println("L2: " + String(mp->getPatrol(mp->eL2)));
  //  unsigned long time3 = micros();
  Serial.println("L3: " + String(mp->getPatrol(mp->eL3)));
  //  unsigned long time4 = micros();
  Serial.println("R1: " + String(mp->getPatrol(mp->eR1)));
  //  unsigned long time5 = micros();
  Serial.println("R2: " + String(mp->getPatrol(mp->eR2)));
  //  unsigned long time6 = micros();
  Serial.println("R3: " + String(mp->getPatrol(mp->eR3)));
  //  unsigned long time7 = micros();

  //  uint8_t distance = mp->ultraSonic(mp->eP13, mp->eP12);
  //  showString(String(distance), myRGBcolor_zyvx);
  //  Serial.println("ultrasonic distance: " + String(distance));
}
