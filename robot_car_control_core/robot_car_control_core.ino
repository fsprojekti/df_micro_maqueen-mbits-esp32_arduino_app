#include <NeoPixelBus.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DFRobot_MaqueenPlus.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// Include the config file
#include "config.h"

#include <SPI.h>
#include <MFRC522.h>

// **************** global app variables *********************
String dir = "forward";  // default direction
int motorSpeed = 50;     // default speed
String taskId = "0";     // default task Id

// **************** WiFi / Web parameters *********************
WebServer server(8000);
HTTPClient http; // create HTTP client

// **************** RFID parameters *********************
// RFID pin definitions for BPI:bit (ESP32-based)
#define RST_PIN 4  // Configurable, connected to RFID module RST pin
#define SS_PIN 5   // Configurable, connected to RFID module SDA pin

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

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

// Function to draw a character on the matrix with a horizontal flip
void drawChar(char c) {
  Serial.println("drawing character: " + String(c));  // Correct string concatenation

  clearMatrix();  // Clear the matrix before drawing a new character

  if (c < 'A' || c > 'Z') return;  // Only draw uppercase letters

  int index = c - 'A';  // Get the index in the font array

  // Iterate through the 5 rows of the character
  for (int y = 0; y < 5; y++) {
    uint8_t rowData = font[index][y];  // Get the row data for the character

    // Reverse the bits in the row to mirror the character horizontally
    uint8_t reversedRow = 0;
    for (int bit = 0; bit < 5; bit++) {
      if (rowData & (1 << bit)) {
        reversedRow |= (1 << (4 - bit));  // Flip the bits within the row
      }
    }

    // Iterate through the 5 columns of the reversed row
    for (int x = 0; x < 5; x++) {
      // Check if the bit is set (pixel is on)
      if (reversedRow & (1 << (4 - x))) {  // Check each bit from left to right after reversal

        // Calculate LED index according to your matrix layout
        int ledIndex = (x * 5) + y;  // Corrected index calculation for the matrix layout

        // Draw the pixel if it is within the matrix boundaries
        if (ledIndex >= 0 && ledIndex < PixelCount) {
          strip.SetPixelColor(ledIndex, white);  // Set the pixel to white
        }
      }
    }
  }
  strip.Show();  // Show the updated display
}


// Function to clear the matrix
void clearMatrix() {
  for (int i = 0; i < PixelCount; i++) {
    strip.SetPixelColor(i, black);  // Set all pixels to black (off)
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

void setupRFID() {
    SPI.begin();
    mfrc522.PCD_Init();
    Serial.println("RFID reader initialized. Scan an RFID tag...");
}

// Function to move the robot car forward
// This function sets both motors to move forward (clockwise) at a given speed.
void moveForward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving forward at speed " + String(moveSpeed));
  drawChar('F');
  mp.setRGB(mp.eALL, mp.eRED);
  mp.motorControl(mp.eALL, mp.eCW, moveSpeed);
}

// Function to move the robot car backward
// This function sets both motors to move backward (counterclockwise) at a given speed.
void moveBackward() {
  int moveSpeed = constrain(motorSpeed, 0, 100);  // Limit speed to 100
  Serial.println("moving backward at speed " + String(moveSpeed));
  drawChar('B');
  mp.setRGB(mp.eALL, mp.eRED);
  mp.motorControl(mp.eALL, mp.eCCW, moveSpeed);
}

// Function to turn the robot car to the left
// This function sets the left motor to move backward (counterclockwise)
// and the right motor to move forward (clockwise) to achieve a left turn.
void turnLeft() {
  int turnSpeed = constrain(motorSpeed, 0, 50);  // Limit speed to
  Serial.println("turning left at speed " + String(turnSpeed));
  drawChar('L');
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
  drawChar('R');
  mp.setRGB(mp.eRIGHT, mp.eRED);
  mp.setRGB(mp.eLEFT, mp.eNO);
  mp.motorControl(mp.eLEFT, mp.eCW, turnSpeed);
  mp.motorControl(mp.eRIGHT, mp.eCCW, turnSpeed);
}

// Function to stop both motors of the robot car
// This function stops all movement by setting both motors to 0 speed.
void stop() {
  Serial.println("stopping");
  drawChar('S');
  mp.setRGB(mp.eALL, mp.eNO);
  mp.motorControl(mp.eALL, mp.eCW, 0);
  mp.motorControl(mp.eALL, mp.eCCW, 0);
}

String readRFID() {
    if (!mfrc522.PICC_IsNewCardPresent()) return "";  // Return empty string if no card is present
    if (!mfrc522.PICC_ReadCardSerial()) return "";    // Return empty string if reading fails

    // Convert UID to a string
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) uidString += "0";  // Add leading zero for single hex digit
        uidString += String(mfrc522.uid.uidByte[i], HEX);     // Convert each byte to a hex string
    }

    uidString.toUpperCase();  // Convert to uppercase for consistency
    mfrc522.PICC_HaltA();     // Halt PICC (Proximity Integrated Circuit Card)

    return uidString;  // Return the UID as a string
}

void sendUIDToServer(String uid) {
    // Construct the URL
    String url = String(serverIP) + String(endpoint) + "?uid=" + uid;
    Serial.println("Sending UID to server: " + url);

    http.begin(url);  // Specify the URL
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

  // Step 1: Set up WiFi connection
  setupWiFi();

  // Step 2: Initialize I2C communication
  setupI2C();

  // Step 3: Initialize RFID reader
  setupRFID();

  // Step 4: Set up the web server
  setupWebServer();

  // Step 5: Initialize the LED matrix
  setupLEDMatrix();

  // Additional initialization for the MaqueenPlus robot car
  mp.setRGB(mp.eALL, mp.eNO);
  // enable MaqueenPlus PID operation control
  mp.PIDSwitch(mp.eON);
}

void loop() {

  // handle incoming requests
  server.handleClient();

  String uuid = readRFID();
  if (rfidUID != "") {  // Check if a valid UID is read
        Serial.println("Detected RFID Tag UID: " + rfidUID);
        sendUIDToServer(rfidUID);  // Send UID to the server
    }


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

