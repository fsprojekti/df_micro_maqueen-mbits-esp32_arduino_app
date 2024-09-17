#include <NeoPixelBus.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DFRobot_MaqueenPlus.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// Include the config file
#include "config.h"


// **************** global app variables *********************

String dir = "forward";                 // default direction
int motorSpeed = 50;                    // default speed
String taskId = "0";                    // default task Id
const int distanceThreshold = 20;       // distance threshold for detecting nearby objects with ultrasound sensors

// **************** WiFi parameters *********************
WebServer server(8000);

// create HTTP client
HTTPClient http;


// **************** Mbits I2C settings *********************
DFRobot_MaqueenPlus mp;

// **************** LED matrix settings *********************
// Define the number of LEDs in the 5x5 matrix
const uint16_t PixelCount = 25;  // 5x5 LED matrix has 25 LEDs
const uint8_t PixelPin = 4;      // GPIO 4 for LED matrix data line

// Use the RMT method for controlling WS2812 LEDs on ESP32
NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> strip(PixelCount, PixelPin);

// Define colors
const RgbColor black(0, 0, 0);
const RgbColor white(55, 55, 55);

// Define a simple 5x5 pixel font for uppercase letters (A-Z)
const uint8_t font[26][5] = {
  { 0b01110, 0b10001, 0b10001, 0b11111, 0b10001 },  // A
  { 0b11110, 0b10001, 0b11110, 0b10001, 0b11110 },  // B
  { 0b01110, 0b10001, 0b10000, 0b10001, 0b01110 },  // C
  { 0b11110, 0b10001, 0b10001, 0b10001, 0b11110 },  // D
  { 0b11111, 0b10000, 0b11110, 0b10000, 0b11111 },  // E
  { 0b11111, 0b10000, 0b11110, 0b10000, 0b10000 },  // F
  { 0b01110, 0b10000, 0b10111, 0b10001, 0b01110 },  // G
  { 0b10001, 0b10001, 0b11111, 0b10001, 0b10001 },  // H
  { 0b01110, 0b00100, 0b00100, 0b00100, 0b01110 },  // I
  { 0b00001, 0b00001, 0b00001, 0b10001, 0b01110 },  // J
  { 0b10001, 0b10010, 0b11100, 0b10010, 0b10001 },  // K
  { 0b10000, 0b10000, 0b10000, 0b10000, 0b11111 },  // L
  { 0b10001, 0b11011, 0b10101, 0b10001, 0b10001 },  // M
  { 0b10001, 0b11001, 0b10101, 0b10011, 0b10001 },  // N
  { 0b01110, 0b10001, 0b10001, 0b10001, 0b01110 },  // O
  { 0b11110, 0b10001, 0b11110, 0b10000, 0b10000 },  // P
  { 0b01110, 0b10001, 0b10101, 0b10011, 0b01111 },  // Q
  { 0b11110, 0b10001, 0b11110, 0b10010, 0b10001 },  // R
  { 0b01111, 0b10000, 0b01110, 0b00001, 0b11110 },  // S
  { 0b11111, 0b00100, 0b00100, 0b00100, 0b00100 },  // T
  { 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 },  // U
  { 0b10001, 0b10001, 0b01010, 0b01010, 0b00100 },  // V
  { 0b10001, 0b10001, 0b10101, 0b11011, 0b10001 },  // W
  { 0b10001, 0b01010, 0b00100, 0b01010, 0b10001 },  // X
  { 0b10001, 0b01010, 0b00100, 0b00100, 0b00100 },  // Y
  { 0b11111, 0b00010, 0b00100, 0b01000, 0b11111 },  // Z
};

void drawChar(char c, int offsetX) {
  if (c < 'A' || c > 'Z') return;  // Only draw uppercase letters

  int index = c - 'A';  // Get the index in the font array

  // Iterate through the 5 rows of the character
  for (int y = 0; y < 5; y++) {
    uint8_t rowData = font[index][y];  // Get the row data for the character

    // Reverse the bits in the row to mirror the character horizontally
    uint8_t reversedRow = 0;
    for (int bit = 0; bit < 5; bit++) {
      if (rowData & (1 << bit)) {
        reversedRow |= (1 << (4 - bit));
      }
    }

    // Iterate through the 5 columns of the row
    for (int x = 0; x < 5; x++) {
      int posX = x + offsetX;  // Calculate the horizontal position with offset

      if (posX < 0 || posX >= 5) continue;  // Skip drawing if out of visible range

      // Check if the bit is set (pixel is on)
      if (reversedRow & (1 << x)) {
        int ledIndex = ((4 - posX) * 5) + y;  // Flip vertically to calculate LED index on the matrix

        // Draw the pixel if it is within the matrix boundaries
        if (ledIndex >= 0 && ledIndex < PixelCount) {
          strip.SetPixelColor(ledIndex, white);  // Set the pixel to white
        }
      }
    }
  }
}

// Function to clear the matrix
void clearMatrix() {
  for (int i = 0; i < PixelCount; i++) {
    strip.SetPixelColor(i, black);  // Set all pixels to black (off)
  }
  strip.Show();  // Update the display
}

void scrollText(const char* text, int delayMs) {
  int textLength = strlen(text);    // Get the length of the text
  int totalWidth = textLength * 6;  // Each character is 5 pixels wide + 1 pixel space

  // Scroll from right to left
  for (int offset = 5; offset > -totalWidth; offset--) {  // Start from right and move left
    clearMatrix();                                        // Clear the matrix
    for (int i = 0; i < textLength; i++) {
      drawChar(text[i], offset + (i * 6));  // Draw each character with spacing
    }
    strip.Show();  // Update the display
    delay(delayMs);
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

  Serial.println("initialize MaqueenPlus I2C communication");
  mp.begin();  // Initialize I2C communication with the MaqueenPlus
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
  Serial.println("Initializing LED matrix...");
  strip.Begin();
  strip.Show();  // Ensure all LEDs are off initially
  Serial.println("LED matrix initialized.");
}

// Function to check ultrasonic sensors for nearby objects and stop the robot if an object is detected
void checkUltrasoundSensors() {
  // Read distance from the ultrasonic sensor
  uint8_t distance = mp.ultraSonic(mp.eP13, mp.eP14);
  Serial.println("Ultrasound sensors detected distance :" + String(distance));

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
  mp.setRGB(mp.eALL, mp.eNO);
  // enable MaqueenPlus PID operation control
  mp.PIDSwitch(mp.eON);
}

void loop() {

  // handle incoming requests
  server.handleClient();

  if (dir == "forward") {
    //stop();
    moveForward();
  } else if (dir == "backward") {
    //stop();
    moveBackward();
  } else if (dir == "left") {
    //stop();
    turnLeft();
  } else if (dir == "right") {
    //stop();
    turnRight();
  } else if (dir == "stop") {
    stop();
  }
}

// Function to move the robot car forward
// This function sets both motors to move forward (clockwise) at a given speed.
void moveForward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving forward at speed " + String(moveSpeed));
  scrollText("FORWARD", 150);
  mp.setRGB(mp.eALL, mp.eRED);
  mp.motorControl(mp.eALL, mp.eCW, moveSpeed);
}

// Function to move the robot car backward
// This function sets both motors to move backward (counterclockwise) at a given speed.
void moveBackward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving backward at speed " + String(moveSpeed));
  scrollText("BACK", 150);
  mp.setRGB(mp.eALL, mp.eRED);
  mp.motorControl(mp.eALL, mp.eCCW, moveSpeed);
}

// Function to turn the robot car to the left
// This function sets the left motor to move backward (counterclockwise)
// and the right motor to move forward (clockwise) to achieve a left turn.
void turnLeft() {
  int turnSpeed = constrain(motorSpeed, 0, 50);  // Limit speed to 50
  scrollText("LEFT", 150);
  mp.setRGB(mp.eLEFT, mp.eRED);
  mp.setRGB(mp.eRIGHT, mp.eNO);
  mp.motorControl(mp.eLEFT, mp.eCCW, turnSpeed);
  mp.motorControl(mp.eRIGHT, mp.eCW, turnSpeed);
}

// Function to turn the robot car to the right
// This function sets the right motor to move backward (counterclockwise)
// and the left motor to move forward (clockwise) to achieve a right turn.
void turnRight() {
  int turnSpeed = constrain(motorSpeed, 0, 50);  // Limit speed to 50
  Serial.println("turning right at speed " + String(turnSpeed));
  scrollText("RIGHT", 150);
  mp.setRGB(mp.eRIGHT, mp.eRED);
  mp.setRGB(mp.eLEFT, mp.eNO);
  mp.motorControl(mp.eLEFT, mp.eCW, turnSpeed);
  mp.motorControl(mp.eRIGHT, mp.eCCW, turnSpeed);
}

// Function to stop both motors of the robot car
// This function stops all movement by setting both motors to 0 speed.
void stop() {
  // Serial.println("stopping");
  scrollText("STOP", 150);
  mp.setRGB(mp.eALL, mp.eNO);
  mp.motorControl(mp.eALL, mp.eCW, 0);
  mp.motorControl(mp.eALL, mp.eCCW, 0);
}
