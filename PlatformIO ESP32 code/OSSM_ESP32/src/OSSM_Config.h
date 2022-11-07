#ifndef OSSM_CONFIG_H
#define OSSM_CONFIG_H


#define DEBUG

#ifdef DEBUG
#define LogDebug(...) Serial.println(__VA_ARGS__)
#define LogDebugFormatted(...) Serial.printf(__VA_ARGS__)
#else
#define LogDebug(...) ((void)0)
#define LogDebugFormatted(...) ((void)0)
#endif

#define SW_VERSION "0.21"
#define HW_VERSION 21 //divide by 10 for real hw version
#define EEPROM_SIZE 200

//#define INITIAL_SETUP //should only be defined at initial burn to configure HW version

extern volatile int encoderButtonPresses;
extern volatile long lastEncoderButtonPressMillis;

/*
    User Config for OSSM - Reference board users should tweak this to match their personal build.
*/

/*
        Motion System Config
*/
// Top linear speed of the device.
const float hardcode_maxSpeedMmPerSecond = 900.0f;
// This should match the step/rev of your stepper or servo.
// N.b. the iHSV57 has a table on the side for setting the DIP switches to your preference.
const float hardcode_motorStepPerRevolution = 800.0f;
// Number of teeth the pulley that is attached to the servo/stepper shaft has.
const float hardcode_pulleyToothCount = 20.0f;
// Set to your belt pitch (Distance between two teeth on the belt) (E.g. GT2 belt has 2mm tooth pitch)
const float hardcode_beltPitchMm = 2.0f;
// This is in millimeters, and is what's used to define how much of
// your rail is usable.
// The absolute max your OSSM would have is the distance between the belt attachments subtract
// the linear block holder length (75mm on OSSM)
// Recommended to also subtract e.g. 20mm to keep the backstop well away from the device.
const float hardcode_maxStrokeLengthMm = 75.f;
/*
        Web Config
*/
// This should be unique to your device. You will use this on the
// web portal to interact with your OSSM.
// there is NO security other than knowing this name, make this unique to avoid
// collisions with other users
extern const char *ossmId;

/*
        Advanced Config
*/
// After homing this is the physical buffer distance from the effective zero to the home switch
// This is to stop the home switch being smacked constantly
const float hardcode_strokeZeroOffsetmm = 6.0f;
// The minimum value of the pot in percent
// prevents noisy pots registering commands when turned down to zero by user
const float commandDeadzonePercentage = 1.0f;
// affects acceleration in stepper trajectory (Aggressiveness of motion)
const float hardcode_accelerationScaling = 100.0f;

#endif
