// TEST AND COMISSIONING OF MOVEMENT CODE

// This should really only be used for troubleshooting
// how to use:

//    1) remove belt & put mark on shaft corresponding to a mark on the ossm body
//    2) it will run 50 thrusts and stop for 5s for you to inspect
//    3) it should stop at the same position every time
//    4) there should be no wondering
//    5) stop the test, and fully assemble
//    6) have the linear rail mid stroke (so that the movement won't hit limits - this is un protected code) and so that the line on the motor shaft also lines up again
//    7) mark the linear rail against the body
//    8) check for movement
  
  
  
  // IO pin assignments
  const int MOTOR_STEP_PIN = 27;          // Default is 27 --- Set it to match your machine
  const int MOTOR_DIRECTION_PIN = 25;     // Default is 25 --- Set it to match your machine
  
  const int MS_DELAY = 20;                // The lower the delay, the faster the movement (stroke speed) MUST BE MORE THAN 3
  const int STEPS_FOR_STEPPING = 1000;    // The number of steps each time (stroke length)

void setup() {

  pinMode(MOTOR_STEP_PIN,OUTPUT);
  pinMode(MOTOR_DIRECTION_PIN,OUTPUT);



  // Get that serial running
  Serial.begin(115200);
  Serial.println("\n Starting");
  

}

void loop() {

  for (int i = 0; i <= 50; i++) {         // We be nestin'
      
        digitalWrite(MOTOR_DIRECTION_PIN, HIGH);
        Serial.println("\n One Way");
        for (int i = 0; i <= STEPS_FOR_STEPPING; i++) {         // One Way
          digitalWrite(MOTOR_STEP_PIN,HIGH);
          delayMicroseconds(MS_DELAY);
          digitalWrite(MOTOR_STEP_PIN,LOW);
          delayMicroseconds(MS_DELAY);
        }
      
        delayMicroseconds(100);
        Serial.println("\n Missy Elliot");
        digitalWrite(MOTOR_DIRECTION_PIN, LOW);
        delayMicroseconds(100);                    // This will hopefully help make sure the direction pin flips correctly
        
        Serial.println("\n The Other");
        for (int i = 0; i <= STEPS_FOR_STEPPING; i++) {       // Or Another
          digitalWrite(MOTOR_STEP_PIN,HIGH);
          delayMicroseconds(MS_DELAY);
          digitalWrite(MOTOR_STEP_PIN,LOW);
          delayMicroseconds(MS_DELAY);
        }

  }

  Serial.println("\n \n CHECK TO SEE IF THE MARK IS MOVING");
  delay(5000);
  

}                                         // I'm gonna step ya, step ya, step ya
