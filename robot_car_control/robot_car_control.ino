#include <FastLED.h>
#include "Dots5x5font.h"
#include <WiFi.h>;
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DFRobot_MaqueenPlus.h>
#include <HTTPClient.h>

// **************** global app variables *********************
int state = 0; // default: 0
bool busy = true; // default: false
int sensorGrayscaleSplittingValue = 150;
// variables for robot cars' infrared sensors (1 means black, 0 means white)
int L1, L2, L3, R1, R2, R3;

// start and end location for the robot car moves
//    these two values are set when the request to move is received via WiFi
//    NOTE: the request to move is only processed after the previous request (move) is finished --> this is controlled by the outside app
long sourceLocation = 5; // default: 0
long targetLocation = 1; // default: 0
long taskId = 1; // default: 0

// production areas locations: 1, 2, 3, 4
int firstProductionAreaLocation = 1;
int lastProductionAreaLocation = 4;
int mainRoboticArmLocation = 5;
// parking areas locations: 6, 7
int firstParkingAreaLocation = 6;
int lastParkingAreaLocation = 7;

int numOfRightTurnsToPass, numOfLeftTurnsToPass;
int numOfRightTurns = 0, numOfLeftTurns = 0;

// **************** WiFi parameters *********************
const char* ssid     = "DESKTOP-4NT77NL-4917";
const char* password = "TPlaptopHOTSPOT";
WebServer server(80);

// **************** Mbits I2C settings *********************
// define I2C data and clock pins used on Mbits ESP32 board
#define I2C_SDA 22
#define I2C_SCL 21

// create a pointer to DFRobot_MaqueenPlus object
// --> this is done as we need to manually set I2C pins and create the DFRobot_MaqueenPlus object inside the setup() function
// --> DFRobot_MaqueenPlus object is accessed via a pointer throughout the program
DFRobot_MaqueenPlus* mp;
// TwoWire object is needed for manual I2C pins definition
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
        if (xpos < 0 || xpos > 10 || ypos < 0 || ypos > 5);
        else {
          matrix[xpos][ypos] = pixelColour;
        }
      }
    }
  }
}

void ShowChar(char myChar, CRGB myRGBcolor) {
  CRGB matrixBackColor[10][5];
  int mapLED[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
  plotMatrixChar(matrixBackColor, myRGBcolor, 0 , myChar, 5, 5);
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
  int mapLED[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
  int messageLength = sMessage.length();
  // Serial.println("string length:" + String(messageLength));
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
      if (sft < sensorGrayscaleSplittingValue) {
        FastLED.delay(200);
      } else {
        FastLED.delay(30);
      }
    }
  }
}

// **************** web server handles *********************
void handleRoot() {

  Serial.println("received a request to /");
  server.send(200, "text/plain", "Hello from DF micro:Maqueen Plus!");
}

void handleMove() {

  Serial.println("received an HTTP request to /move");

  // server.send(200, "text/plain", "{status:accept}");
  if (busy) {
    server.send(200, "text/plain", "{status:reject}");
  }
  else {
    // check if all parameters are present
    if (server.arg("source") == "")
      server.send(200, "text/plain", "{status:reject, missing source location}");
    else if (server.arg("target") == "")
      server.send(200, "text/plain", "{status:reject, missing target location}");
    else if (server.arg("taskId") == "")
      server.send(200, "text/plain", "{status:reject, missing task id}");
    else {
      sourceLocation = server.arg("source").toInt();
      targetLocation = server.arg("target").toInt();
      taskId = server.arg("taskId").toInt();
      // busy = true;
      Serial.println("source: " + String(sourceLocation) + ", target: " + String(targetLocation) + ", taskId: " + String(taskId));
      server.send(200, "text/plain", "{status:accept}");
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

void setup() {
  // initialize serial print
  Serial.begin(9600);

  // connect to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  Serial.println("");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // start the local server (for incoming requests to the robot car)
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  Serial.println("define I2C pins");
  i2cCustomPins.begin(I2C_SDA, I2C_SCL, 100000);

  Serial.println("construct MaqueenPlus object");
  mp = new DFRobot_MaqueenPlus(&i2cCustomPins, 0x10);

  Serial.println("initialize MaqueenPlus I2C communication");
  mp->begin();

  // web server API endpoints
  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.onNotFound(handleNotFound);

  // start web server
  server.begin();
  Serial.println("HTTP server started");

  // initialize ESP32 board LED matrix
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(max_bright);

  // set MaqueenPlus front lights color
  mp->setRGB(mp->eALL, mp->eBLUE);
  // enable MaqueenPlus PID operation control
  mp->PIDSwitch(mp->eON);
}

void loop() {

  // Serial.println(mp->getVersion());
  // delay(100);
  // stop();
  //  checkAllSensors();

  //  Serial.println("left motor speed:" + String(mp->getSpeed(mp->eLEFT)));
  //  Serial.println("right motor speed:" + String(mp->getSpeed(mp->eRIGHT)));

  // handle incoming requests
  server.handleClient();

  Serial.println("robot state: " + String(state));
  // ShowChar function expects a character, while state is an int value
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
    case 0:
      if (busy) {
        // start the move

        //the car detects the location for the left or right turn (coming out of the parking or production area)
        // the car turns left if it is coming out of parking area or it turns right if it is coming out of production area
        if (L1 && R1 && !L3 && !R3) {
          //    stop();
          // car is coming out of parking area --> it turns left
          if (sourceLocation > targetLocation) {
            state = 2;
          }
          // car is coming of production area --> it turns right
          else {
            state = 4;
          }
          break;
        }
        // the car detects the end location for the car move (either the production area or the parking area)
        else if (!L3 && !R3) {
          stop();
          state = 8;
          break;
        }
        // the car detects the location for the left turn at the grid outer borders -- > the car must turn left
        else if (!L3) {
          stop();
          state = 2;
          break;
        }
        //the car detects the location for the right turn at the grid outer borders --> the car must turn right
        else if (!R3) {
          stop();
          state = 4;
          break;
        }
        //the car detects the location for the left turn at the parking area --> the car turns left under two conditions:
        // 1. it is moving from a production area (1 to 4) to a parking area (5 to 9)
        // 2. it has passed the "numOfLeftTurnsToPass" turns
        else if ((!L1 || !R1) && !L3 && R3) {
          if ((sourceLocation < targetLocation) && (numOfLeftTurns >= numOfLeftTurnsToPass)) {
            stop();
            state = 3;
            break;
          }
          else {
            numOfLeftTurns++;
            break;
          }
        }
        //the car detects the location for the right turn at the production area --> the car turns right under two conditions:
        // 1. it is moving from a parking area (5 to 9) to a production area (1 to 4)
        // 2. it has passed the "numOfRightRightTurnsToPass" turns
        else if ((!L1 || !R1) && L3 && !R3) {
          if ((sourceLocation > targetLocation) && (numOfRightTurns >= numOfRightTurnsToPass)) {
            stop();
            state = 5;
            break;
          }
          else {
            numOfRightTurns++;
            break;
          }
        }
        else {
          // move forward
          state = 0;
        }
        break;
      }

      break;

    case 555:
      // for testing purposes, the car moves straight forward without line following
      //   stop();
      moveForwardFix();
      break;

    // case 1: the car moves forward following the line
    case 1:
      moveForward();

    // case 2: turn the car left (at the grid outer turns or coming out of the parking area)
    case 2:
      turnLeft();
      // the car turns until the sensors L1 and R1 detect the line
      if (!L1 && !R1) {
        stop();
        state = 0;
      }
      break;

    // case 3: turn the car left (going to the parking area)
    case 3:
      turnLeft();
      // the car turns until the sensors L3 and R3 and either L1 or R1 detect the line
      if ((!L1 || !R1) && !L3 && !R3) {
        stop();
        state = 0;
      }
      break;

    // case 4: turn the car right (at the grid outer turns coming out of production area)
    case 4:
      turnRight();
      // the car turns until the sensors L1 and R1 detect the line
      if (!L1 && !R1) {
        stop();
        state = 0;
      }
      break;

    // case 5: turn the car right (going to the production area)
    case 5:
      turnRight();
      // the car turns until the sensors L3 and R3 and either L1 or R1 detect the line
      if ((!L1 || !R1) && !L3 && !R3) {
        stop();
        state = 0;
      }
      break;

    // case 6: the car is at the end location --> send http message to Node.js control app and switch to state 0
    case 6:
      state = 0;
      break;

      //      // call Node.js control app
      //      // create HTTP client
      //      HTTPClient http;
      //      // set server host and path
      //      http.begin("193.2.80.85/report?taskId" + String(taskId) + "&state=done"); //HTTP
      //      // start the connection and send HTTP header
      //      int httpCode = http.GET();
      //
      //      // httpCode will be negative on error
      //      if (httpCode > 0) {
      //        // HTTP header has been send and Server response header has been handled
      //        Serial.println("HTTP response code: " + httpCode);
      //        Serial.println("HTTP response: " + http.getString());
      //
      //        // set status (busy) to false and return to the default case to wait for a new request
      //        busy = false;
      //        state = 0;
      //      }
      //      // error sending HTTP request to the Node.js control app, retry
      //      else {
      //        Serial.println("HTTP response code: " + httpCode);
      //        Serial.println("HTTP GET failed, error:");
      //        Serial.println(http.errorToString(httpCode).c_str());
      //      }
  }
}

void moveForwardFix() {
  mp->motorControl(mp->eALL, mp->eCW, 200);
}

// basic line following
void moveForward() {
  // Serial.println("moving forward");
  // 1001
  if (L2 && !L1 && !R1 && R2) {
    mp->motorControl(mp->eALL, mp->eCW, 50);
    //    ShowChar('A', myRGBcolor_zyvx);
  }
  // 1101
  else if (L2 && L1 && !R1 && R2) {
    mp->motorControl(mp->eLEFT, mp->eCW, 50);
    mp->motorControl(mp->eRIGHT, mp->eCW, 37);
  }
  // 1100
  else if (L2 && L1 && !R1 && !R2) {
    mp->motorControl(mp->eLEFT, mp->eCW, 50);
    mp->motorControl(mp->eRIGHT, mp->eCW, 0);
  }
  // 1110
  else if (L2 && L1 && R1 && !R2) {
    mp->motorControl(mp->eLEFT, mp->eCW, 50);
    mp->motorControl(mp->eRIGHT, mp->eCCW, 20);
  }
  // 1011
  else if (L2 && !L1 && R1 && R2) {
    mp->motorControl(mp->eLEFT, mp->eCW, 37);
    mp->motorControl(mp->eRIGHT, mp->eCW, 50);
  }
  // 0011
  else if (!L2 && !L1 && R1 && R2) {
    mp->motorControl(mp->eLEFT, mp->eCW, 0);
    mp->motorControl(mp->eRIGHT, mp->eCW, 50);
  }
  // 0111
  else if (!L2 && L1 && R1 && R2) {
    mp->motorControl(mp->eLEFT, mp->eCCW, 20);
    mp->motorControl(mp->eRIGHT, mp->eCW, 50);
  }
  // 1111
  else if (L2 && L1 && R1 && R2) {
    mp->motorControl(mp->eALL, mp->eCW, 50);
  }
}

// turn left: move the right motor backwards (counter clockwise) and the left motor forward (clockwise)
void turnLeft() {
  // Serial.println("turning left");
  mp->motorControl(mp->eLEFT, mp->eCCW, 37);
  mp->motorControl(mp->eRIGHT, mp->eCW, 37);
}

// turn right: move the left motor backwards (counter clockwise) and the right motor forward (clockwise)
void turnRight() {
  // Serial.println("turning right");
  mp->motorControl(mp->eLEFT, mp->eCW, 37);
  mp->motorControl(mp->eRIGHT, mp->eCCW, 37);
}

// stop: stop both motors
void stop() {
  // Serial.println("stopping");
  mp->motorControl(mp->eALL, mp->eCW, 0);
  mp->motorControl(mp->eALL, mp->eCCW, 0);
}

// check current values of all relevant sensors
void checkAllSensors() {

//  unsigned long time1 = micros();
//  Serial.println("L1: " + String(mp->getPatrol(mp->eL1)));
//  unsigned long time2 = micros();
//  Serial.println("L2: " + String(mp->getPatrol(mp->eL2)));
//  unsigned long time3 = micros();
//  Serial.println("L3: " + String(mp->getPatrol(mp->eL3)));
//  unsigned long time4 = micros();
//  Serial.println("R1: " + String(mp->getPatrol(mp->eR1)));
//  unsigned long time5 = micros();
//  Serial.println("R2: " + String(mp->getPatrol(mp->eR2)));
//  unsigned long time6 = micros();
//  Serial.println("R3: " + String(mp->getPatrol(mp->eR3)));
//  unsigned long time7 = micros();

  // double duration1 = (double)(time2 - time1);
  // double duration2 = (double)(time3 - time2);
  // double duration3 = (double)(time4 - time3);
  // double duration4 = (double)(time5 - time4);
  // double duration5 = (double)(time6 - time5);
  // double duration6 = (double)(time7 - time6);

  // Serial.println("read time for L1: " + String(duration1));
  // Serial.println("read time for L2: " + String(duration2));
  // Serial.println("read time for L3: " + String(duration3));
  // Serial.println("read time for L4: " + String(duration4));
  // Serial.println("read time for L5: " + String(duration5));
  // Serial.println("read time for L6: " + String(duration6));
  // // uint8_t distance = mp->ultraSonic(mp->eP12, mp->eP13);
  // ShowString(String(distance), myRGBcolor_zyvx);
  // Serial.println("ultrasonic distance: " + String(distance));
}
