#ifndef OSSM_PIN_H
#define OSSM_PIN_H
/*
    Pin Definitions - Drivers, Buttons and Remotes
    OSSM Reference board users are unlikely to need to modify this! See OSSM_Config.h
*/

/*
        Driver pins
*/
// Pin that pulses on servo/stepper steps - likely labelled PUL on drivers.
#define MOTOR_STEP_PIN 14
// Pin connected to driver/servo step direction - likely labelled DIR on drivers.
// N.b. to iHSV57 users - DIP switch #5 can be flipped to invert motor direction entirely
#define MOTOR_DIRECTION_PIN 27
// Pin for motor enable - likely labelled ENA on drivers.
#define MOTOR_ENABLE_PIN 26

/*
    Homing and safety pins
*/
// define the IO pin the emergency stop switch is connected to
#define STOP_PIN 19
// define the IO pin where the limit(homing) switch(es) are connected to (switches in
// series in normally open setup) Switches wired from IO pin to ground.
#define LIMIT_SWITCH_PIN 12

/*
        Wifi Control Pins
*/
// Pin for WiFi reset button (optional)
#define WIFI_RESET_PIN 23

//Pin for the toggle for wifi control (Can be left alone if no hardware toggle is required)
#define WIFI_CONTROL_TOGGLE_PIN 22

#define LOCAL_CONTROLLER INPUT_PULLDOWN
#define WIFI_CONTROLLER INPUT_PULLUP
//Choose whether the default control scheme is local (e.g. OSSM remote, potentiometers, etc.) or through Wi-Fi
//If both are desired, a hardware toggle will need to be installed and wired to WIFI_CONTROL_TOGGLE_PIN
#define WIFI_CONTROL_DEFAULT LOCAL_CONTROLLER

/*These are configured for the OSSM Remote - which has a screen, a potentiometer and an encoder which clicks*/
#define SPEED_POT_PIN 34
#define ENCODER_SWITCH 35
#define ENCODER_A 18
#define ENCODER_B 5
#define REMOTE_SDA 21
#define REMOTE_CLK 19
#define REMOTE_ADDRESS 0x3c


#endif