#include <Arduino.h>
#include <Servo.h>
#include <math.h>

#include "vectors.h"
#include "RC.h"
#include "Initializations.h"
#include "Helpers.h"

enum State {
  Initialize,
  Standing,
  Car,
  Crab,
  Calibrate
};

int phase = 0;
float points = 800;
int phaseProgress = 500;
int standProgress = 0;
State currentState = Initialize;

float standingDistanceAdjustment = 0;

float distanceFromGroundBase = -60;
float distanceFromGround = 0; 
float previousDistanceFromGround = 0;

float liftHeight = 130;
float landHeight = 70;
float landHeightOffsetMultiplier = 0.5;      
float strideLength = 240;
float strideOvershoot = 10;
float distanceFromCenter = 220;

float crabTargetForwardAmount = 0;
float crabForwardAmount = 0;

Vector2 joy1TargetVector = Vector2(0,0);
float joy1TargetMagnitude = 0;

Vector2 joy1CurrentVector = Vector2(0,0);
float joy1CurrentMagnitude = 0;

Vector2 joy2TargetVector = Vector2(0,0);
float joy2TargetMagnitude = 0;

Vector2 joy2CurrentVector = Vector2(0,0);
float joy2CurrentMagnitude = 0;

float timeSinceLastInput = 0;

float landingBuffer = 15;



void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  attachServos(); 
  RC_Setup(); 
  stateInitialize();
}

void loop() {

  bool connected = RC_GetDataPackage();
  //RC_DisplayData();

  if(connected){
    double joy1x = map(rc_data.joy1_X,0,254,-100,100);
    double joy1y = map(rc_data.joy1_Y,0,254,-100,100);

    double joy2x = map(rc_data.joy2_X,0,254,100,-100);
    double joy2y = map(rc_data.joy2_Y,0,254,-100,100);
    
    joy1TargetVector = Vector2(joy1x,joy1y);
    joy1TargetMagnitude = constrain(calculateHypotenuse(abs(joy1x),abs(joy1y)),0,100);   

    joy2TargetVector = Vector2(joy2x,joy2y);
    joy2TargetMagnitude = constrain(calculateHypotenuse(abs(joy2x),abs(joy2y)),0,100);  

    previousDistanceFromGround = distanceFromGround;
    distanceFromGround = distanceFromGroundBase + rc_data.slider1 * -1.7;
    distanceFromCenter = map(rc_data.slider2,0,100,120,280);
  }
  else{
    calibrationState();
    //Serial.println("State: Disconnected");
    return;
  }

  joy1CurrentVector = lerp(joy1CurrentVector, joy1TargetVector, 0.08);
  joy1CurrentMagnitude = lerp(joy1CurrentMagnitude, joy1TargetMagnitude, 0.08);

  joy2CurrentVector = lerp(joy2CurrentVector, joy2TargetVector, 0.12);
  joy2CurrentMagnitude = lerp(joy2CurrentMagnitude, joy2TargetMagnitude, 0.12);  


  if(rc_data.pushButton2){
    crabState();
  }

  else if(abs(joy1CurrentMagnitude) >= 5 || abs(joy2CurrentMagnitude) >= 5){
    carState();
    timeSinceLastInput = millis();
  }

  if(abs(timeSinceLastInput - millis()) > 5) {
    standingState();
    return;
  }  

  if(phaseProgress >= points){
    setPhaseStartPoints(); 
    //delay(30);
    phaseProgress = 0; 

    phase++;
    if(phase > 1) phase = 0;
  }
}



void setPhaseStartPoints(){
  for(int i = 0; i < 6; i++){
      phaseStartPoints[i] = currentPoints[i];
    }
}

int angleToMicroseconds(double angle, int servoID) {
  double val = 500.0 + (((2500.0 - 500.0) / 180.0) * angle);
  val = map(val, 500, 2500, calibrations[servoID].y, calibrations[servoID].x);
  return (int)val;
}

void moveToPos(int leg, Vector3 pos){
  currentPoints[leg] = pos;
  
  float dis = Vector3(0,0,0).distanceTo(pos);
  if(dis > legLength){
    print_value("Point impossible to reach", pos);
    print_value("Distance",dis);
    return;
  }

  float x = pos.x;
  float y = pos.y;
  float z = pos.z;

  float o1 = offsets[leg].x;
  float o2 = offsets[leg].y;
  float o3 = offsets[leg].z;

  float theta1 = atan2(y,x) * (180 / PI) + o1; // base angle
  float l = sqrt(x*x + y*y); // x and y extension 
  float l1 = l - a1;
  float h = sqrt(l1*l1 + z*z);

  float phi1 = acos(constrain((pow(h,2) + pow(a2,2) - pow(a3,2)) / (2*h*a2),-1,1));
  float phi2 = atan2(z, l1);
  float theta2 = (phi1 + phi2) * 180 / PI + o2;
  float phi3 = acos(constrain((pow(a2,2) + pow(a3,2) - pow(h,2)) / (2*a2*a3),-1,1));
  float theta3 = 180 - (phi3 * 180 / PI) + o3;

  targetRot = Vector3(theta1,theta2,theta3);
  
  int coxaMicroseconds = angleToMicroseconds(targetRot.x, leg + 2);
  int femurMicroseconds = angleToMicroseconds(targetRot.y, leg + 1);
  int tibiaMicroseconds = angleToMicroseconds(targetRot.z, leg);

  switch(leg){
    case 0:
      coxa1.writeMicroseconds(coxaMicroseconds);
      femur1.writeMicroseconds(femurMicroseconds);
      tibia1.writeMicroseconds(tibiaMicroseconds);
      break;

    case 1:
      coxa2.writeMicroseconds(coxaMicroseconds);
      femur2.writeMicroseconds(femurMicroseconds);
      tibia2.writeMicroseconds(tibiaMicroseconds);
      break;

    case 2:
      coxa3.writeMicroseconds(coxaMicroseconds);
      femur3.writeMicroseconds(femurMicroseconds);
      tibia3.writeMicroseconds(tibiaMicroseconds);
      break;

    case 3:
      coxa4.writeMicroseconds(coxaMicroseconds);
      femur4.writeMicroseconds(femurMicroseconds);
      tibia4.writeMicroseconds(tibiaMicroseconds);
      break;

    case 4:
      coxa5.writeMicroseconds(coxaMicroseconds);
      femur5.writeMicroseconds(femurMicroseconds);
      tibia5.writeMicroseconds(tibiaMicroseconds);
      break;

    case 5:
      coxa6.writeMicroseconds(coxaMicroseconds);
      femur6.writeMicroseconds(femurMicroseconds);
      tibia6.writeMicroseconds(tibiaMicroseconds);
      break;

    default:
      break;
  }
  return; 
}






