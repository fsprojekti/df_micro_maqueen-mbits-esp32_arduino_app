#include <DFRobot_MaqueenPlus.h>
#include <Adafruit_Microbit.h>
#include <Wire.h>
Adafruit_Microbit_Matrix microbit;

// create the MaqueenPlus robot car object
DFRobot_MaqueenPlus  MaqueenPlus;

int state = 666;
bool busy = false;
// start and end location for the robot car moves
//    these two values are set when the request to move is received via WiFi
//    NOTE: the request to move is only processed after the previous request (move) is finished --> this is controlled by the outside app
long startLocation = 1;
long endLocation = 6;

int numOfRightTurnsToPass, numOfLeftTurnsToPass;
int numOfRightTurns = 0, numOfLeftTurns = 0;

void setup() {
  // initialize serial print
  Serial.begin(9600);

  Wire.begin()
  microbit.begin();

  Serial.println("initial state:");
  Serial.println(state);
}

void loop() {

  Serial.println("stopping both motors clockwise");
//MaqueenPlus.motorControl(MaqueenPlus.eALL, MaqueenPlus.eCW, 0);
  
  Serial.println("stopping both motors counter-clockwise");
  //MaqueenPlus.motorControl(MaqueenPlus.eALL, MaqueenPlus.eCCW, 0);
  
  Serial.println("\ncar state: " + String(state));
  microbit.print(state);
  
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
      // if the car detects an object within 10 cm, it stops and waits
      if (MaqueenPlus.ultraSonic(MaqueenPlus.eP1, MaqueenPlus.eP2) < 10) {
        stop();
        break;
      }
      // there are no obstacles, move forward
      moveForward();

      //the car detects the location for the left turn at the grid outer borders --> the car must turn left
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 0) {
        stop();
        state = 2;
        break;
      }

      //the car detects the location for the left or right turn (coming out of the parking or production area)
      //  the car turns left if it is coming out of parking area or it turns right if it is coming out of production area
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        // car is coming out of parking area --> it turns left
        if (startLocation > endLocation) {
          state = 3;
        }
        // car is coming of production area --> it turns right
        else {
          state = 5;
        }
        break;
      }

      //the car detects the location for the left turn at the parking area --> the car turns left under two conditions:
      //  1. it is moving from a production area (1 to 4) to a parking area (5 to 9)
      //  2. it has passed the "numOfLeftTurnsToPass" turns
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 0) {
        if ((startLocation < endLocation) && (numOfLeftTurns >= numOfLeftTurnsToPass)) {
          stop();
          state = 4;
          break;
        }
        else {
          numOfLeftTurns++;
          break;
        }
      }

      //the car detects the location for the right turn at the production area --> the car turns right under two conditions:
      //  1. it is moving from a parking area (5 to 9) to a production area (1 to 4)
      //  2. it has passed the "numOfRightRightTurnsToPass" turns
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 0) {
        if ((startLocation > endLocation) && (numOfRightTurns >= numOfRightTurnsToPass)) {
          stop();
          state = 6;
          break;
        }
        else {
          numOfRightTurns++;
          break;
        }
      }
      //the car detects the location for the right turn going out of the parking area --> the car must turn right
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 0 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        state = 7;
        break;
      }

      //the car detects the end location for the car move (either the production area or the parking area)
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        break;
      }
      break;

    // case 2: turn the car left at the grid outer turns
    case 2:
      turnLeft();
      // the car turns until the sensors L1 and R1 detect the line and sensors L2 and R2 do not detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 0) {
        stop();
        state = 1;
      }
      break;

    // case 3: turn the car left coming out of the parking area
    case 3:
      turnLeft();
      // the car turns until the sensors L1 and R1 detect the line and sensors L2 and R2 do not detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 0) {
        stop();
        state = 1;
      }
      break;

    // case 4: turn the car left going to the parking area
    case 4:
      turnLeft();
      // the car turns until the sensors L1, R1, L3 and R3 all detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        state = 1;
      }
      break;

    // case 5: turn the car right (coming out of production area)
    case 5:
      turnRight();
      // the car turns until the sensors L1 and R1 detect the line and sensors L2 and R2 do not detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL2) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eR2) == 0) {
        stop();
        state = 1;
      }
      break;

    // case 6: turn the car right (going to the production area)
    case 6:
      turnRight();
      // the car turns until the sensors L1, R1, L3 and R3 all detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        state = 1;
      }
      break;

    // case 7: turn the car right (going out of the parking area)
    case 7:
      turnRight();
      // the car turns until the sensors L1, R1, L3 and R3 all detect the line
      if (MaqueenPlus.getPatrol(MaqueenPlus.eL1) == 1 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR1) == 1 && MaqueenPlus.getPatrol(MaqueenPlus.eL3) == 0 &&  MaqueenPlus.getPatrol(MaqueenPlus.eR3) == 1) {
        stop();
        state = 1;
      }
      break;

    // case 8: the car is at the end location --> return to state 1
    case 8:
      state = 1;
      break;
   case 666:
      Serial.print("sensor L1:");
      Serial.println(MaqueenPlus.getPatrol(MaqueenPlus.eL1));
      Serial.print("sensor L2:");
      Serial.println(MaqueenPlus.getPatrol(MaqueenPlus.eL2));
      Serial.print("sensor L3:");
      Serial.println(MaqueenPlus.getPatrol(MaqueenPlus.eL3));
      Serial.print("sensor R1:");
      Serial.println(MaqueenPlus.getPatrol(MaqueenPlus.eR1));
      Serial.print("sensor R2:");
      Serial.println(MaqueenPlus.getPatrol(MaqueenPlus.eR2));
      Serial.print("sensor R3:");
      Serial.println(MaqueenPlus.getPatrol(MaqueenPlus.eR3));
      break;
  }

  delay(10);
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
