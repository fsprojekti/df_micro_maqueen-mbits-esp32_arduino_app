#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DFRobot_MaqueenPlus.h>

// create the MaqueenPlus robot car object
DFRobot_MaqueenPlus  MaqueenPlus;

// set WiFi parameters
const char* ssid     = "yourssid";
const char* password = "yourpasswd";
WebServer server(80);

int state = 0;
bool busy = false;
// start and end location for the robot car moves
//    these two values are set when the request to move is received via WiFi
//    NOTE: the request to move is only processed after the previous request (move) is finished --> this is controlled by the outside app
long startLocation = 0;
long endLocation = 0;

int numOfRightTurnsToPass, numOfLeftTurnsToPass;
int numOfRightTurns = 0, numOfLeftTurns = 0;

// webserver handles
void handleRoot() {

  server.send(200, "text/plain", "hello from esp32!");
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

  server.on("/", handleRoot);

  server.on("/move", HTTP_GET, []() {

    if (busy) {
      server.send(200, "text/plain", "{"available":false}");
    }
    else {
      // first parameter in the GET message is the start location and the second parameter is the end location of the robot car move
      startLocation = server.pathArg(0).toInt();
      endLocation = server.pathArg(1).toInt();
      busy = true;
      server.send(200, "text/plain", "{"available":true}");
    }
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  server.begin();

}

void loop() {
  // handle incoming requests
  server.handleClient();

  // car robot driving algorithm
  switch (state) {
    // default case, the car is free (busy = false) and waits for the next request to move
    case 0:
      if (busy) {
        if (endLocation < startLocation)
          numOfRightTurnsToPass = 2 * (endLocation - 1);
        else
          numOfRightTurnsToPass = 2 * (endLocation - 5);

        numOfLeftTurns = 0;
        numOfRightTurns = 0;
        // start the move
        state = 1;
      }
      break;

    // case 1: the car moves forward following the line
    case 1:
      moveForward();

      //the car detects the location for the left turn at the grid outer borders --> the car must turn left
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 0) {
        stop();
        state = 2;
        break;
      }
      //the car detects the location for the left turn at the parking area --> the car turns left under two conditions:
      //  1. it is moving from a production area (1 to 4) to a parking area (5 to 9)
      //  2. it has passed the "numOfLeftTurnsToPass" turns
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 0) {
        if ((startLocation < endLocation) && (numOfLeftTurns >= numOfLeftTurnsToPass)) {
          stop();
          state = 3;
          break;
        }
        else {
          numOfLeftTurns++;
        }
      }
      //the car detects the location for the right turn (coming out of the parking or production area) --> the car must turn right
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        state = 4;
        break;
      }
      //the car detects the location for the right turn at the production area --> the car turns right under two conditions:
      //  1. it is moving from a parking area (5 to 9) to a production area (1 to 4)
      //  2. it has passed the "numOfRightLeftTurnsToPass" turns
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 0) {
        if ((startLocation > endLocation) && (numOfLeftTurns >= numOfLeftTurnsToPass)) {
          stop();
          state = 5;
          break;
        }
        else {
          numOfRightTurns++;
        }
      }
      //the car detects the location for the right turn going to the parking area --> the car must turn right
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        state = 6;
        break;
      }

      // TODO: DETECT THE END LOCATION FOR THE CAR MOVE
      // HOW? ALL PRODUCTION AREAS AND WAREHOUSE (SYSTEM APP) MUST BE EQUIPPED WITH SOME IDENTIFICATION
      // RFID??

      break;

    // case 2: turn the car left at the grid outer turns
    case 2:
      turnLeft();
      // the car turns until the sensors L1 and R1 detect the line and sensors L2 and R2 do not detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 0) {
        stop();
        state = 1;
        break;
      }
      break;

    // case 3: turn the car left going to the parking area
    case 3:
      turnLeft();
      // the car turns until the sensors L1, R1, L3 and R3 all detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        state = 1;
        break;
      }
      break;

    // case 4: turn the robot car right (coming out of the parking or production area)
    case 4:
      turnRight();
      // the car turns until the sensors L1 and R1 detect the line and sensors L2 and R2 do not detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 0) {
        stop();
        state = 1;
        break;
      }
      break;

    // case 5: turn the robot car right (going to the production area)
    case 5:
      turnRight();
      // the car turns until the sensors L1, R1, L3 and R3 all detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        state = 1;
        break;
      }
      break;
    // case 6: turn the robot car right (going to the parking area)
    case 6:
      turnRight();
      // the car turns until the sensors L1, R1, L3 and R3 all detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        state = 1;
        break;
      }
      break;
  }
}

// basic line following
void moveForward() {
  // if sensors L1 and R1 detect the line, move straight forward
  if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 0) {
    MaqueenPlus.motorControl(MaqueenPlus.eALL, MaqueenPlus.eCW, 50);
  }
  else {
    // if L1 and L2 do not detect the line, but R1 and R2 do, turn left
    if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 1) {
      MaqueenPlus.motorControl(MaqueenPlus.eLEFT, MaqueenPlus.eCW, 160);
      MaqueenPlus.motorControl(MaqueenPlus.eRIGHT, MaqueenPlus.eCW, 50);
    }
    // if only R2 detects the line, turn left a bit faster
    else if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 1) {
      MaqueenPlus.motorControl(MaqueenPlus.eLEFT, MaqueenPlus.eCW, 200);
      MaqueenPlus.motorControl(MaqueenPlus.eRIGHT, MaqueenPlus.eCW, 50);
    }
    // if R1 and R2 do not detect the line, but L1 and L2 do, turn right
    if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 0) {
      MaqueenPlus.motorControl(MaqueenPlus.eLEFT, MaqueenPlus.eCW, 50);
      MaqueenPlus.motorControl(MaqueenPlus.eRIGHT, MaqueenPlus.eCW, 160);
    }
    // if only L2 detects the line, turn right a bit faster
    else if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 0) {
      MaqueenPlus.motorControl(MaqueenPlus.eLEFT, MaqueenPlus.eCW, 50);
      MaqueenPlus.motorControl(MaqueenPlus.eRIGHT, MaqueenPlus.eCW, 200);
    }
  }
}

// turn left: move the right motor backwards (counter clockwise) and the left motor forward (clockwise)
void turnLeft() {
  MaqueenPlus.motorControl(MaqueenPlus.eLEFT, MaqueenPlus.eCCW, 50);
  MaqueenPlus.motorControl(MaqueenPlus.eRIGHT, MaqueenPlus.eCW, 50);


}

// turn right: move the left motor backwards (counter clockwise) and the right motor forward (clockwise)
void turnRight() {
  MaqueenPlus.motorControl(MaqueenPlus.eLEFT, MaqueenPlus.eCW, 50);
  MaqueenPlus.motorControl(MaqueenPlus.eRIGHT, MaqueenPlus.eCCW, 50);
}

// stop: stop both motors
void stop() {
  MaqueenPlus.motorControl(MaqueenPlus.eALL, MaqueenPlus.eCW, 0);
  MaqueenPlus.motorControl(MaqueenPlus.eALL, MaqueenPlus.eCCW, 0);
}

// listen for incoming messages and process it based on the type of request
// TODO: DEFINE THE REQUESTS (API-S)
//void processWifiRequest() {

//  WiFiClient client = server.available();   // listen for incoming clients
//
//  if (client) {                             // if you get a client,
//    Serial.println("New Client.");           // print a message out the serial port
//    String currentLine = "";                // make a String to hold incoming data from the client
//    while (client.connected()) {            // loop while the client's connected
//      if (client.available()) {             // if there's bytes to read from the client,
//        char c = client.read();             // read a byte, then
//        Serial.write(c);                    // print it out the serial monitor
//        if (c == '\n') {                    // if the byte is a newline character
//
//          // if the current line is blank, you got two newline characters in a row.
//          // that's the end of the client HTTP request, so send a response:
//          if (currentLine.length() == 0) {
//            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
//            // and a content-type so the client knows what's coming, then a blank line:
//            client.println("HTTP/1.1 200 OK");
//            client.println("Content-type:text/html");
//            client.println();
//
//            // the content of the HTTP response follows the header:
//            client.print("Click <a href=\"/H\">here</a> to turn the LED on pin 5 on.<br>");
//            client.print("Click <a href=\"/L\">here</a> to turn the LED on pin 5 off.<br>");
//
//            // The HTTP response ends with another blank line:
//            client.println();
//            // break out of the while loop:
//            break;
//          } else {    // if you got a newline, then clear currentLine:
//            currentLine = "";
//          }
//        } else if (c != '\r') {  // if you got anything else but a carriage return character,
//          currentLine += c;      // add it to the end of the currentLine
//        }
//
//        // Check which GET request was sent
//        // TODO!!!
//        if (currentLine.endsWith("GET /H")) {
//          // GET /H turns the LED on
//          busy = true;
//          // set start and end location
//        }
//        if (currentLine.endsWith("GET /L")) {
//          digitalWrite(5, LOW);                // GET /L turns the LED off
//          busy = true;
//          // set start and end location
//        }
//      }
//    }
//    // close the connection:
//    client.stop();
//    Serial.println("Client Disconnected.");
//  }
//}
