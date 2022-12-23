#ifndef OSSM_CONFIG_H
#define OSSM_CONFIG_H

///////////////////////////////
///     DEBUG SETTINGS     ///
/////////////////////////////

// Global debug flag, unset to remove all debug outputs (not recommended)
#define DEBUG true

#if DEBUG
    #define LogDebug(...) Serial.println(__VA_ARGS__)
    #define LogDebugFormatted(...) Serial.printf(__VA_ARGS__)

    //// more granular debug settings ////
    // Set to get positional helpers (ie "looping" at the beginning of each loop() execution)
    #define DEBUG_CODE_POSITIONS false
    // Set to get periotic updates on the total number of strokes / power up time / distance fucked
    #define PRINT_STATS false
    // Set to spam pre-mode-selection encoder info. Useful if choosing "Simple Penetration" or "Stroke engine" is not
    // working
    #define PRINT_ENCODER_INFO_BEFORE_MODE false
    // Set to spam position, current info during homeing procedure
    #define PRINT_HOMING_INFO false

    // StrokeEngine specific debug messages
    #define DEBUG_STROKE_ENGINE true
#else
    #define LogDebug(...) ((void)0)
    #define LogDebugFormatted(...) ((void)0)
#endif

///////////////////////////////
///     FEATURE FLAGS      ///
/////////////////////////////

// Enables the torque setting (accessable by triple clicking)
#define SERVO_MOTOR_VERSION 6 // valid values = [5, 6]
#define SERVO_TORQUE_SETTING true

// Controls how internet connected the OSSM is
//   0 = no wifi at all
//   1 = local wifi only (not functionally implemented)
//   2 = internet connected / remote update only
//   3 = internet connected / remote update and control (default)
#define INTERNET_CONNECTION_MODE 3

// Don't mess with these unless you are who you know you are ;)
#define INTERNET_CONNECTION_TEST_SERVER false
#define OTA_TEST_FIRMWARE false

///////////////////////////////
///      VERSION INFO      ///
/////////////////////////////
#define SW_VERSION "0.23"
#define HW_VERSION 23 // divide by 10 for real hw version
#define EEPROM_SIZE 200
// #define INITIAL_SETUP //should only be defined at initial burn to configure HW version

///////////////////////////////
///      VOLATILE VARS     ///
/////////////////////////////
extern volatile int encoderButtonPresses;
extern volatile long lastEncoderButtonPressMillis;

///////////////////////////////
///       USER CONFIG      ///
/////////////////////////////
/*
    UI config
*/
// time window in miliseconds that clicks will register as double / triple clicks, rather than individual clicks.
//   raise if its hard to get into pattern / force mode
//   lower if you mean to cycle between stroke / depth / sensation, but instead you end up in pattern / force mode
const int MULTI_CLICK_SPEED = 250;
// time window that if two clicks happens within, only one counts. This it due to buttons being "noisy" when being
//   pressed. Without this a single click will register as multiple. If you have a motion disability, such as
//   parkensons, where you inadvertantly press things multiple times, you can raise this value to be higher to tune out
//   accedental inputs (you will need to change MULTI_CLICK_SPEED accordingly)
const int CLICK_DEBOUNCE_SPEED = 50;
/*
    Motion System Config
*/
// Min & Max torque settings. override this if you want a harder pounding. the iHSV57 defaults to 13
const int MIN_FORCE = 2;
const int MAX_FORCE = 11;
// Safety value for the setForce function. If a value is given higher than this, it will be ignored
const int MAX_SAFETY_FORCE = max(MAX_FORCE, 13);
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
