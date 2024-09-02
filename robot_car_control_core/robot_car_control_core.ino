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
int state = 1; // default: 1
bool busy = false; // default: false
// variables for robot cars' infrared sensors (1 means black, 0 means white)
int L1, L2, L3, R1, R2, R3;
// this flag is set to true in the first iteration of the loop after busy variable is set to true and set to false immediately after that
bool startFlag = false;

String taskId = ""; // default: 0

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

void ShowChar(char myChar, CRGB myRGBcolor) {
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

void ShowString(String sMessage, CRGB myRGBcolor) {
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

  // server.send(200, "text/plain", "{\"status\":\"accept\"}");
  if (busy) {
    JSONdocument["status"] = "reject, the car is busy";
    String message;
    serializeJson(JSONdocument, message);
    server.send(200, "text/plain", message);
  }
  else {

    // check if all parameters are present
    if (server.arg(0) == "") {
      JSONdocument["status"] = "reject, missing source location";
      String message;
      serializeJson(JSONdocument, message);
      server.send(200, "text/plain", message);
    }
    else if (server.arg(1) == "") {
      JSONdocument["status"] = "reject, missing target location";
      String message;
      serializeJson(JSONdocument, message);
      server.send(200, "text/plain", message);
    }
    else if (server.arg(2) == "") {
      JSONdocument["status"] = "reject, missing task id";
      String message;
      serializeJson(JSONdocument, message);
      server.send(200, "text/plain", message);
    }
    else {

      taskId = server.arg(0);
      busy = true;
      startFlag = true;

      // TODO: change the output of the prinltn function
      Serial.println("");

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
  mp->setRGB(mp->eALL, mp->eBLUE);
  // enable MaqueenPlus PID operation control
  mp->PIDSwitch(mp->eON);
}

void loop() {

  // Serial.println(mp->getVersion());
  //  checkAllSensors();

  // handle incoming requests
  server.handleClient();

  Serial.println("robot state: " + String(state));
  //   ShowChar function expects a character, while state is an int value
  //  --> adding 48 converts an integer value (0 to 9) to char (decimal values from 48 to 57)
  ShowChar(state + 48, myRGBcolor_zyvx);

  L1 = mp->getPatrol(mp->eL1);
  L2 = mp->getPatrol(mp->eL2);
  L3 = mp->getPatrol(mp->eL3);
  R1 = mp->getPatrol(mp->eR1);
  R2 = mp->getPatrol(mp->eR2);
  R3 = mp->getPatrol(mp->eR3);

  // car robot driving algorithm
  switch (state) {
    // default case, the car is free (busy = false) and waits for the next request to move
    case 1: {
        if (busy) {
          // if the car detects an object within n (=20?) cm, it stops and waits
          // uint8_t ultrasonic1 = mp->ultraSonic(mp->eP32, mp->eP25);
          // uint8_t ultrasonic2 = mp->ultraSonic(mp->eP32, mp->eP25);
          // uint8_t ultrasonic3 = mp->ultraSonic(mp->eP32, mp->eP25);
          // uint8_t ultrasonic_avg = (ultrasonic1 + ultrasonic2 + ultrasonic3) / 3;
          // TODO: TEST
          //if (ultrasonic_avg != 0 && ultrasonic_avg < 20) {
          //  stop();
          //  break;
          //}
          // start the move
          moveForward();

        }
        else {
          Serial.println("no request to move received");
        }
        break;
      }
    // case 2: move backward
    case 2: {
        moveBackward();
        break;
      }
    // case 3: turn the car left
    case 3: {
        turnLeft();
        break;
      }
    // case 4: turn the car right
    case 4: {
        turnRight();
        break;
      }
    // case 6: the car is at the end location --> send http message to Node.js control app and switch to state 1
    case 6: {
        busy = false;
        state = 1;

        // Call Node.js control app
        String httpURL = controlAppIp + String(controlAppPort) + "/report?taskId=" + String(taskId) + "&state=done";
        Serial.println("HTTP URL: " + httpURL);

        // Number of retry attempts for HTTP GET
        const int maxHttpRetries = 5;
        int httpRetryCount = 0;
        bool httpSuccess = false;

        while (httpRetryCount < maxHttpRetries) {
          http.begin(controlAppIp, controlAppPort, "/report?taskId=" + String(taskId) + "&state=done"); //HTTP
          int httpCode = http.GET();  // Start the connection and send HTTP header

          if (httpCode > 0) {  // HTTP response code > 0 means success
            Serial.println("HTTP response code: " + String(httpCode));
            Serial.println("HTTP response: " + http.getString());
            httpSuccess = true;  // Set success flag
            break;  // Exit retry loop on success
          } else {
            Serial.println("HTTP GET failed, response code: " + String(httpCode));
            Serial.println(String("HTTP GET error: ") + http.errorToString(httpCode).c_str());
            httpRetryCount++;
            delay(2000);  // Wait 2 seconds before retrying
          }
        }
        http.end();  // Close HTTP connection after each attempt

        // Check if HTTP request was successful
        if (!httpSuccess) {
          Serial.println("Failed to communicate with Node.js control app after retries.");
          // Handle the failure, possibly restart or change state
        } else {
          // Reset the busy state and prepare for the next request
          busy = false;
          state = 1;
        }
        break;
      }
  }
}

// Function to move the robot car forward
// This function sets both motors to move forward (clockwise) at a given speed.
void moveForward() {
  Serial.println("moving forward");
  mp->motorControl(mp->eALL, mp->eCW, 50);
}

// Function to move the robot car backward
// This function sets both motors to move backward (counterclockwise) at a given speed.
void moveBackward() {
  Serial.println("moving backward");
  mp->motorControl(mp->eALL, mp->eCCW, 50);
}

// Function to turn the robot car to the left
// This function sets the left motor to move backward (counterclockwise)
// and the right motor to move forward (clockwise) to achieve a left turn.
void turnLeft() {
  Serial.println("turning left");
  mp->motorControl(mp->eLEFT, mp->eCCW, 45);
  mp->motorControl(mp->eRIGHT, mp->eCW, 45);
}

// Function to turn the robot car to the right
// This function sets the right motor to move backward (counterclockwise)
// and the left motor to move forward (clockwise) to achieve a right turn.
void turnRight() {
  Serial.println("turning right");
  mp->motorControl(mp->eLEFT, mp->eCW, 45);
  mp->motorControl(mp->eRIGHT, mp->eCCW, 45);
}

// Function to stop both motors of the robot car
// This function stops all movement by setting both motors to 0 speed.
void stop() {
  // Serial.println("stopping");
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

  //  uint8_t distance = mp->ultraSonic(mp->eP32, mp->eP25);
  //  ShowString(String(distance), myRGBcolor_zyvx);
  //  Serial.println("ultrasonic distance: " + String(distance));
}
