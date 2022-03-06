/*
    User Config for OSSM - Reference board users should tweak this to match their personal build.
*/

/*
        Motion System Config
*/
//Top linear speed of the device.
const float maxSpeedMmPerSecond = 900.0f;
//This should match the step/rev of your stepper or servo.
//N.b. the iHSV57 has a table on the side for setting the DIP switches to your preference.
const float motorStepPerRevolution = 800.0f;
//Number of teeth the pulley that is attached to the servo/stepper shaft has.
const float pulleyToothCount = 20.0f;
// Set to your belt pitch (Distance between two teeth on the belt) (E.g. GT2 belt has 2mm tooth pitch)
const float beltPitchMm = 2.0f;
// This is in millimeters, and is what's used to define how much of
// your rail is usable.
// The absolute max your OSSM would have is the distance between the belt attachments subtract
// the linear block holder length (75mm on OSSM)
// Recommended to also subtract e.g. 20mm to keep the backstop well away from the device.
const float maxStrokeLengthMm = 75.f;
/*
        Web Config
*/
// This should be unique to your device. You will use this on the
// web portal to interact with your OSSM.
// there is NO security other than knowing this name, make this unique to avoid
// collisions with other users
const char *ossmId = "OSSM1";

/*
        Xtoys Config

*/
const float xtoySpeedScaling  = 1000.0f;          // Scaling how fast Xtoy can travel it is limted by maxSpeedMmPerSecond at top.
const float xtoyAccelartion   = 40000.0f;         // Hard Coded Acceleration.
const float xtoyDeaccelartion =  80000.0f;        // Hard Coded Decceleration.

/*
        Advanced Config
*/
// After homing this is the physical buffer distance from the effective zero to the home switch
// This is to stop the home switch being smacked constantly
const float strokeZeroOffsetmm = 10.0f;
// The minimum value of the pot in percent
// prevents noisy pots registering commands when turned down to zero by user
const float commandDeadzonePercentage = 1.0f;
// affects acceleration in stepper trajectory (Aggressiveness of motion)
const float accelerationScaling = 100.0f;
