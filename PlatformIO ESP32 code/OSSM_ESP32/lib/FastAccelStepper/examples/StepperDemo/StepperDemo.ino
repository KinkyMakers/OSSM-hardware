#include "AVRStepperPins.h"
#include "FastAccelStepper.h"
#include "test_seq.h"

#ifdef SIM_TEST_INPUT
#include <avr/sleep.h>
#endif
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_task_wdt.h>
#endif

#define VERSION "post-4f2e1e4"

struct stepper_config_s {
  uint8_t step;
  uint8_t enable_low_active;
  uint8_t enable_high_active;
  uint8_t direction;
  uint16_t dir_change_delay;
  bool direction_high_count_up;
  bool auto_enable;
  uint32_t on_delay_us;
  uint16_t off_delay_ms;
};

#if defined(ARDUINO_ARCH_AVR)
#if defined(__AVR_ATmega328P__)
// Example hardware configuration for Arduino Nano
// Please adapt to your configuration
const uint8_t led_pin = 13;  // turn off with PIN_UNDEFINED
const struct stepper_config_s stepper_config[MAX_STEPPER] = {
    {
      // stepper 1 shall be connected to OC1A
      step : stepPinStepper1A,
      enable_low_active : 6,
      enable_high_active : PIN_UNDEFINED,
      direction : 5,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 500000,
      off_delay_ms : 5000
    },
    {
      // stepper 2 shall be connected to OC1B
      step : stepPinStepper1B,
      enable_low_active : 8,
      enable_high_active : PIN_UNDEFINED,
      direction : 7,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    }};
#elif defined(__AVR_ATmega2560__)
// Example hardware configuration for Arduino ATmega2560
// Please adapt to your configuration
const uint8_t led_pin = PIN_UNDEFINED;  // turn off with PIN_UNDEFINED
const struct stepper_config_s stepper_config[MAX_STEPPER] = {
    {
      step : stepPinStepperA,
      enable_low_active : 19,
      enable_high_active : PIN_UNDEFINED,
      direction : 21,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 500000,
      off_delay_ms : 5000
    },
    {
      step : stepPinStepperB,
      enable_low_active : 18,
      enable_high_active : PIN_UNDEFINED,
      direction : 20,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      // stepper 3 shall be connected to OC4C
      step : stepPinStepperC,
      enable_low_active : 43,
      enable_high_active : PIN_UNDEFINED,
      direction : 42,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    }};
#elif defined(__AVR_ATmega32U4__)
// Example hardware configuration for Arduino ATmega32u4
// Please adapt to your configuration
const uint8_t led_pin = PIN_UNDEFINED;  // turn off with PIN_UNDEFINED
const struct stepper_config_s stepper_config[MAX_STEPPER] = {
    {
      step : stepPinStepperA,
      enable_low_active : 16,
      enable_high_active : PIN_UNDEFINED,
      direction : 26,  // PB4
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 500000,
      off_delay_ms : 5000
    },
    {
      step : stepPinStepperB,
      enable_low_active : 15,
      enable_high_active : PIN_UNDEFINED,
      direction : 14,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    }};
#endif
#elif defined(ARDUINO_ARCH_ESP32)
// Example hardware configuration for esp32 board.
// Please adapt to your configuration
const uint8_t led_pin = PIN_UNDEFINED;
const struct stepper_config_s stepper_config[MAX_STEPPER] = {
    // clang-format off
    // Test-HW
    // Position 01 linked to atmega nano
    // 2: Enable Left Pin 13 GPIO13   , DIR Right Pin 7 GPIO18,    Step Right Pin 13 GPIO15
    // 3: Enable Left Pin 12 GPIO12   , DIR Right Pin 6 GPIO19,    Step Right Pin 12 GPIO2  blue LED
    // 4: Enable Left Pin 11 GPIO14   , DIR Right Pin 5 GPIO21,    Step Right Pin 11 GPIO4
    // 5: Enable Left Pin 10 GPIO27   , DIR Right Pin 4 GPIO3 RX0, Step Right Pin 10 GPIO16 RX2
    // 6: Enable Left Pin 9  GPIO26 A9, DIR Right Pin 3 GPIO1 TX0, Step Right Pin 9  GPIO17 TX2
    // 7: Enable Left Pin 8  GPIO25 A8, DIR Right Pin 2 GPIO22,    Step Right Pin 8  GPIO5
    //                          ALL Enable: Right Pin 1 GPIO23
    // Left Pin 15: +5V
    // clang-format on
    {
      // position 01.234567 => 2
      step : 17,
      enable_low_active : 26,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,  // was GPIO 1 in conflict with TXD, via wire to Dir of
                       // next stepper
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 50,
      off_delay_ms : 1000
    },
    {
      // position 01.234567 => 3
      step : 15,
      enable_low_active : 13,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 500,
      off_delay_ms : 1000
    }
#if MAX_STEPPER > 2
    ,
    {
      // position 01.234567 => 4, step is linked to blue LED
      step : 2,
      enable_low_active : 12,
      enable_high_active : PIN_UNDEFINED,
      direction : 19,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 500,
      off_delay_ms : 1000
    },
    {
      // position 01.234567 => 5
      step : 5,
      enable_low_active : 25,
      enable_high_active : PIN_UNDEFINED,
      direction : 22,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    }
#endif
#if MAX_STEPPER > 4
    ,
    {
      // position 01.234567 => 6
      step : 16,
      enable_low_active : 27,
      enable_high_active : PIN_UNDEFINED,
      direction : 21,  // was GPIO 3 in conflict with RXD, via wire to GPIO21
                       // (Dir next stepper)
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      // position 01.234567 => 7
      step : 4,
      enable_low_active : 14,
      enable_high_active : PIN_UNDEFINED,
      direction : 21,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    }
#endif
#if MAX_STEPPER > 6
    ,
    {
      step : 14,  // direction pin of M3
      enable_low_active : 26,
      enable_high_active : PIN_UNDEFINED,
      direction : 19,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      step : 23,  // ALL ENABLE PIN !!!!
      enable_low_active : PIN_UNDEFINED,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    }
#endif
#if MAX_STEPPER == 14
    ,
    {
      step : 32,
      enable_low_active : PIN_UNDEFINED,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      step : 33,
      enable_low_active : PIN_UNDEFINED,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      step : 25,  // enable pin of M6
      enable_low_active : PIN_UNDEFINED,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      step : 26,  // enable pin of M5
      enable_low_active : PIN_UNDEFINED,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      step : 22,  // direction pin of M6
      enable_low_active : 26,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      step : 21,  // direction pin of M3
      enable_low_active : PIN_UNDEFINED,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    }
#endif
};
#elif defined(ARDUINO_ARCH_SAM)
// Hardware configuration copied from esp32 board. Not used on due board
// Please adapt to your configuration
const uint8_t led_pin = PIN_UNDEFINED;
const struct stepper_config_s stepper_config[MAX_STEPPER] = {
    {
      step : 17,
      enable_low_active : 26,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,  // was GPIO 1 in conflict with TXD, via wire to Dir of
                       // next stepper
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 50,
      off_delay_ms : 1000
    },
    {
      step : 15,
      enable_low_active : 13,
      enable_high_active : PIN_UNDEFINED,
      direction : 18,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 500,
      off_delay_ms : 1000
    },
    {
      step : 2,
      enable_low_active : 12,
      enable_high_active : PIN_UNDEFINED,
      direction : 19,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 500,
      off_delay_ms : 1000
    },
    {
      step : 5,
      enable_low_active : 25,
      enable_high_active : PIN_UNDEFINED,
      direction : 22,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      step : 16,
      enable_low_active : 27,
      enable_high_active : PIN_UNDEFINED,
      direction : 21,  // was GPIO 3 in conflict with RXD, via wire to GPIO21
                       // (Dir next stepper)
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    },
    {
      step : 4,
      enable_low_active : 14,
      enable_high_active : PIN_UNDEFINED,
      direction : 21,
      dir_change_delay : 0,
      direction_high_count_up : true,
      auto_enable : true,
      on_delay_us : 5000,
      off_delay_ms : 10
    }};
#endif

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper[MAX_STEPPER];

enum { normal, test, config } mode = normal;
bool test_ongoing = false;
struct test_seq_s test_seq[MAX_STEPPER] = {
#if MAX_STEPPER == 8
    {.test = NULL}, {.test = NULL},
#endif
#if MAX_STEPPER >= 6
    {.test = NULL}, {.test = NULL}, {.test = NULL},
#endif
#if MAX_STEPPER >= 3
    {.test = NULL},
#endif
    {.test = NULL}, {.test = NULL}};

#define _NL_ "\n"
#define _SEP_ "|"
#define _SEP_CHAR_ '|'

// clang-format off
const static char messages[] PROGMEM =
#define _Move_ "\200"
    "Move " _SEP_
#define _Output_driver_ "\201"
    "Output driver " _SEP_
#define _Cannot_set_ "\202"
    "Cannot set " _SEP_
#define _pin_ "\203"
    "pin " _SEP_
#define _step "\204"
    "step" _SEP_
#define _to_ "\205"
    "to " _SEP_
#define _LOW_ "\206"
    "LOW " _SEP_
#define _HIGH_ "\207"
    "HIGH " _SEP_
#define _to_LOW_nl "\210"
     _to_  _LOW_ _NL_ _SEP_
#define _to_HIGH_nl "\211"
     _to_  _HIGH_ _NL_ _SEP_
#define _Toggle_ "\212"
    "Toggle " _SEP_
#define _ERROR_ "\213"
    "ERROR " _SEP_
#define _high_counts_ "\214"
    "high counts " _SEP_
#define _enable_ "\215"
    "enable " _SEP_
#define _disable "\216"
    "disable" _SEP_
#define _direction_ "\217"
    "direction " _SEP_
#define _pulse_counter_ "\220"
    "pulse counter " _SEP_
#define _attach "\221"
    "attach" _SEP_
#define _set_ "\222"
    "set " _SEP_
#define _time_to_ "\223"
    "time" _to_ _SEP_
#define _is_not_defined_nl "\224"
    "is not defined" _NL_ _SEP_
#define _run_ "\225"
    "run " _SEP_
#define _forward_ "\226"
    "forward " _SEP_
#define _backward_ "\227"
    "backward " _SEP_
#define _Stepped_ "\230"
    "Stepped " _SEP_
#define _acceleration_ "\231"
    "acceleration " _SEP_
#define _speed_ "\232"
    "speed " _SEP_
#define _test_ "\233"
    "test " _SEP_
#define _erroneous_ "\234"
    "erroneous " _SEP_
#define _digitalRead_ "\235"
    "digitalRead() " _SEP_
#define ____ "\236"
    "    " _SEP_
#define ________ "\237"
	____ ____  _SEP_
#define _ooo_ "\240"
    " ... " _SEP_
#define _Enter_ "\241"
    "Enter " _SEP_
#define _mode_ "\242"
    "mode " _SEP_
#define _clear_ "\243"
    "clear " _SEP_
#define _stepper "\244"
    _step "per" _SEP_
#define _select "\245"
    "select" _SEP_
#define _selected_stepper "\246"
    _select "ed " _stepper _SEP_
#define _usage_ "\247"
    "usage " _SEP_
#define _steps_ "\250"
    _step "s " _SEP_
#define _output_ "\251"
    "output " _SEP_
#define _test_sequence_ "\252"
    _test_ "sequence " _SEP_
#define _Perform_ "\253"
    "Perform " _SEP_
#define _configuration_ "\254"
    "configuration " _SEP_
#define _delay_ "\255"
    "delay " _SEP_
#define _step_pin_ "\256"
    _step " " _pin_ _SEP_
#define _one_step_ "\257"
    "one " _step " " _SEP_
#define _position_ "\260"
    "position " _SEP_
#define _of_the_ "\261"
    "of the " _SEP_
#define _Turn_ "\262"
    "Turn " _SEP_
#define _stop "\263"
    "stop"  _SEP_
#define _from_ "\264"
    "from "  _SEP_
#define _comma__counting_ "\265"
    ", counting "  _SEP_
#define __disable_auto_enable_nl "\266"
    " " _disable " auto " _enable_ _NL_ _SEP_
#define _ooo_set_selected_stepper_s_ "\267"
   _ooo_ _set_ _selected_stepper "'s " _SEP_
#define _m1_m2_to_select_stepper_ "\270"
    ____ "M1/M2/.. " _ooo_ _to_ _select " " _stepper _NL_ _SEP_
#define _print_this_usage_ "\271"
    ____ "?" ________ _ooo_ "Print this usage" _NL_ _NL_ _SEP_
#define _toggle_print_usage_after_stepper_stop "\272"
    ____ "Q" ________ _ooo_ _Toggle_ "print " _usage_ "on " _stepper " " _stop _NL_ _SEP_
#define _I_speed_I_ "\273"
    "<speed> "  _SEP_
#define _I_accel_I_ "\274"
    "<accel> "  _SEP_
#define _Enter_command_seperated_by_space_carriage_return_or_newline_NL "\275"
    _Enter_ "commands separated by space, carriage return or newline:" _NL_ _SEP_
#define MSG_OFFSET 62
#define MSG_SELECT_STEPPER 0+MSG_OFFSET
    "Select " _stepper " " _SEP_
#define MSG_TOGGLE_MOTOR_INFO 1+MSG_OFFSET
    _Toggle_ _stepper " info" _NL_ _SEP_
#define MSG_TOGGLE_USAGE_INFO 2+MSG_OFFSET
    _Toggle_  _usage_ "info" _NL_ _SEP_
#define MSG_ENTER_TEST_MODE 3+MSG_OFFSET
    _Enter_ _test_ _mode_ _NL_ _SEP_
#define MSG_SET_ACCELERATION_TO 4+MSG_OFFSET
    _set_ _acceleration_ _to_ _SEP_
#define MSG_SET_SPEED_TO_US 5+MSG_OFFSET
    _set_ _speed_ "(us/" _step ") " _to_ _SEP_
#define MSG_MOVE_STEPS 6+MSG_OFFSET
    _Move_ _steps_ _SEP_
#define MSG_MOVE_TO_POSITION 7+MSG_OFFSET
    _Move_ _to_ _position_ _SEP_
#define MSG_RETURN_CODE 8+MSG_OFFSET
    "returncode = " _SEP_
#define MSG_SET_POSITION 9+MSG_OFFSET
    _set_ _position_ _SEP_
#define MSG_SET_ENABLE_TIME 10+MSG_OFFSET
    _set_ _enable_ _time_to_ _SEP_
#define MSG_SET_DISABLE_TIME 11+MSG_OFFSET
    _set_ _disable " " _time_to_ _SEP_
#define MSG_OUTPUT_DRIVER_ON 12+MSG_OFFSET
    _Output_driver_ "on" _NL_ _SEP_
#define MSG_OUTPUT_DRIVER_OFF 13+MSG_OFFSET
    _Output_driver_ "off" _NL_ _SEP_
#define MSG_OUTPUT_DRIVER_AUTO 14+MSG_OFFSET
    _Output_driver_ "automatic " _mode_ _NL_ _SEP_
#define MSG_STOP 15+MSG_OFFSET
    _stop _NL_ _SEP_
#define MSG_KEEP_RUNNING 16+MSG_OFFSET
    "Keep running" _NL_ _SEP_
#define MSG_RUN_FORWARD 17+MSG_OFFSET
    _run_ _forward_ _NL_ _SEP_
#define MSG_RUN_BACKWARD 18+MSG_OFFSET
    _run_ _backward_ _NL_ _SEP_
#define MSG_IMMEDIATE_STOP 19+MSG_OFFSET
    "immediate " _stop _NL_ _SEP_
#define MSG_UPDATE_SPEED_ACCELERATION 20+MSG_OFFSET
    "Update " _speed_ " / " _acceleration_ _NL_ _SEP_
#define MSG_BLOCKING_WAIT 21+MSG_OFFSET
    "Blocking wait for running " _stepper " " _to_ _stop _NL_ _SEP_
#define MSG_TEST_DIRECT_DRIVE 22+MSG_OFFSET
    _test_ "direct drive" _NL_ _SEP_
#define MSG_STEPPED_FORWARD 23+MSG_OFFSET
    _Stepped_ _forward_ _NL_ _SEP_
#define MSG_STEPPED_BACKWARD 24+MSG_OFFSET
    _Stepped_ _backward_ _NL_ _SEP_
#define MSG_WAIT_MS 25+MSG_OFFSET
    " ms wait" _NL_ _SEP_
#define MSG_SELECT_TEST_SEQUENCE 26+MSG_OFFSET
    "Select " _test_ "sequence: " _SEP_
#define MSG_EXIT_TO_MAIN_MENU 27+MSG_OFFSET
    "Exit " _to_ "main menu" _NL_ _SEP_
#define MSG_RUN_TESTS 28+MSG_OFFSET
    _run_ _test_ _NL_ _SEP_
#define MSG_WAIT_COMPLETE 29+MSG_OFFSET
    "Wait complete" _NL_ _SEP_
#define MSG_FAILED_STATUS 30+MSG_OFFSET
    "Failed status " _from_ _test_ _NL_ _SEP_
#define MSG_ENABLE_LOW_PIN_IS_NOT_LOW 31+MSG_OFFSET
    _Cannot_set_ _enable_  _LOW_  _pin_ _to_LOW_nl _SEP_
#define MSG_ENABLE_LOW_PIN_IS_NOT_HIGH 32+MSG_OFFSET
    _Cannot_set_ _enable_  _LOW_  _pin_ _to_HIGH_nl _SEP_
#define MSG_ENABLE_HIGH_PIN_IS_NOT_LOW 33+MSG_OFFSET
    _Cannot_set_ _enable_  _HIGH_  _pin_ _to_LOW_nl _SEP_
#define MSG_ENABLE_HIGH_PIN_IS_NOT_HIGH 34+MSG_OFFSET
    _Cannot_set_ _enable_  _HIGH_  _pin_ _to_HIGH_nl _SEP_
#define MSG_STEP_PIN_IS_NOT_LOW 35+MSG_OFFSET
    _Cannot_set_ _step_pin_ _to_LOW_nl _SEP_
#define MSG_STEP_PIN_IS_NOT_HIGH 36+MSG_OFFSET
    _Cannot_set_ _step_pin_ _to_HIGH_nl _SEP_
#define MSG_CANNOT_SET_DIRECTION_PIN 37+MSG_OFFSET
    _Cannot_set_ _direction_ _pin_ _to_ _SEP_
#define MSG_STEPPER_VERSION 38+MSG_OFFSET
    "StepperDemo Version " VERSION
    _NL_ _SEP_
#define MSG_ATTACH_PULSE_COUNTER 39+MSG_OFFSET
    _attach " " _pulse_counter_ _SEP_
#define MSG_ERROR_ATTACH_PULSE_COUNTER 40+MSG_OFFSET
    _ERROR_ _attach "ing "  _pulse_counter_ _NL_ _SEP_
#define MSG_ERROR_INVALID_VALUE 41+MSG_OFFSET
    _ERROR_ "invalid value" _NL_ _SEP_
#define MSG_ERROR_MOVE_ERR_ACCELERATION_IS_UNDEFINED__MINUS_3 42+MSG_OFFSET
    _ERROR_ _acceleration_ _is_not_defined_nl _SEP_
#define MSG_ERROR_MOVE_ERR_SPEED_IS_UNDEFINED__MINUS_2 43+MSG_OFFSET
    _ERROR_ _speed_ _is_not_defined_nl _SEP_
#define MSG_ERROR_MOVE_ERR_NO_DIRECTION_PIN__MINUS_1 44+MSG_OFFSET
    _ERROR_ "no " _direction_  _pin_ "=> impossible move" _NL_ _SEP_
#define MSG_MOVE_OK 45+MSG_OFFSET
    "OK" _NL_ _SEP_
#define MSG_STRAY_DIGITAL_READ_TOGGLE 46+MSG_OFFSET
    _Toggle_ _erroneous_ _digitalRead_ _to_ _step_pin_ _NL_ _SEP_
#define MSG_STRAY_DIGITAL_READ_ENABLED 47+MSG_OFFSET
    _erroneous_ _digitalRead_ _to_ _step_pin_ "IS ON !!!" _NL_ _SEP_
#define MSG_LONG_INTERRUPT_BLOCK_TOGGLE 48+MSG_OFFSET
    _Toggle_ _erroneous_ "100 µs ISR block" _NL_ _SEP_
#define MSG_LONG_INTERRUPT_BLOCK_ENABLED 49+MSG_OFFSET
    _erroneous_ "100 µs ISR BLOCK IS ON" _NL_ _SEP_
#define MSG_SET_UNIDIRECTIONAL_STEPPER 50+MSG_OFFSET
    _set_ "unidirectional " _stepper _NL_ _SEP_
#define MSG_CLEAR_PULSE_COUNTER 51+MSG_OFFSET
    _clear_ _pulse_counter_ _NL_ _SEP_
#define MSG_SET_SPEED_TO_HZ 52+MSG_OFFSET
    _set_ _speed_ "(" _steps_ "/s) " _to_ _SEP_
#define MSG_PASS_STATUS 53+MSG_OFFSET
    _test_ "passed" _NL_ _SEP_
#define MSG_TEST_COMPLETED 54+MSG_OFFSET
    _test_ "completed" _NL_ _SEP_
#define MSG_ENTER_CONFIG_MODE 55+MSG_OFFSET
    _Enter_ "config " _mode_ _NL_ _SEP_
#define MSG_DIRECTION_PIN 56+MSG_OFFSET
    _direction_ _pin_ _SEP_
#define MSG_SET_TO_PIN 57+MSG_OFFSET
    _set_  _to_  _pin_ _SEP_
#define MSG_DISABLED 58+MSG_OFFSET
    _disable "d" _NL_ _SEP_
#define MSG_HIGH_COUNT_UP 59+MSG_OFFSET
    _high_counts_ "up" _NL_ _SEP_
#define MSG_HIGH_COUNT_DOWN 60+MSG_OFFSET
    _high_counts_ "down" _NL_ _SEP_
#define MSG_DELAY 61+MSG_OFFSET
    _delay_ "in us = " _SEP_
#define MSG_UNKNOWN_COMMAND 62 + MSG_OFFSET
    "Cannot interpret this command: " _SEP_
#define MSG_SET_SPEED_TO_MILLI_HZ 63+MSG_OFFSET
    _set_ _speed_ "(" _steps_ "/1000s) " _to_ _SEP_
#define MSG_USAGE_NORMAL 64+MSG_OFFSET
#define MSG_USAGE_TEST 65+MSG_OFFSET
#define MSG_USAGE_CONFIG 66+MSG_OFFSET
#if MSG_USAGE_CONFIG >= 128+32
#error "TOO MANY ENTRIES"
#endif
    /* USAGE NORMAL */
    _Enter_command_seperated_by_space_carriage_return_or_newline_NL
	_m1_m2_to_select_stepper_
    ____ "c" ________ _ooo_ _Enter_ _configuration_ _mode_ _NL_
    ____ "V" _I_speed_I_  _ooo_set_selected_stepper_s_ _speed_ "in us/" _step _NL_
    ____ "H" _I_speed_I_  _ooo_set_selected_stepper_s_ _speed_ "in " _steps_ "/s" _NL_
    ____ "h" _I_speed_I_  _ooo_set_selected_stepper_s_ _speed_ "in " _steps_ "/1000s" _NL_
    ____ "A" _I_accel_I_  _ooo_set_selected_stepper_s_ _acceleration_ _NL_
    ____ "a" _I_accel_I_  _ooo_ _acceleration_ "control with +/-" _acceleration_ "values" _NL_
    ____ "U" ________ _ooo_ "Update " _selected_stepper "'s " _speed_ "/ " _acceleration_ " while "
    "running" _NL_
    ____ "P<pos>   " _ooo_ _Move_ _selected_stepper " " _to_ "+/- " _position_ _NL_
    ____ "R<n> " ____ _ooo_ _Move_ _selected_stepper " by +/- n " _steps_ _NL_
    ____ "f" ________ _ooo_ _run_ _forward_ _comma__counting_ "up" _NL_
    ____ "b" ________ _ooo_ _run_ _backward_ _comma__counting_ "down" _NL_
    ____ "K" ________ _ooo_ "Keep " _selected_stepper " running in current " _direction_ _NL_
    ____ "@<pos>   " _ooo_ _set_ _selected_stepper " " _to_ _position_ "(can be "
    "negative)" _NL_
    ____ "E<us>" ____ _ooo_set_selected_stepper_s_ _delay_ _from_ _enable_ _to_ _steps_ _NL_
    ____ "D<ms>" ____ _ooo_set_selected_stepper_s_ _delay_ _from_ _steps_ _to_ _disable _NL_
    ____ "N" ________ _ooo_ _Turn_ _selected_stepper " " _output_ "on, " __disable_auto_enable_nl
    ____ "F" ________ _ooo_ _Turn_ _selected_stepper " " _output_ "off," __disable_auto_enable_nl
    ____ "O" ________ _ooo_ "Put " _selected_stepper " into auto " _enable_ _mode_ _NL_
    ____ "S" ________ _ooo_ _stop " " _selected_stepper " with deceleration" _NL_
    ____ "X" ________ _ooo_ "Immediately " _stop " " _stepper " and " _set_ "zero position" _NL_
    ____ "I" ________ _ooo_ _Toggle_ _stepper " info, while any " _stepper " is running" _NL_
    ____ "W" ________ _ooo_ "Blocking wait until " _selected_stepper " is " _stop "ped, will "
    "deadlock if " _stepper " never " _stop "s" _NL_
    ____ "w<ms>" ____ _ooo_ "Wait time in ms" _NL_
    ____ "+" ________ _ooo_ _Perform_ _one_step_ _forward_ _of_the_ _selected_stepper _NL_
    ____ "-" ________ _ooo_ _Perform_ _one_step_ _backward_ _of_the_ _selected_stepper _NL_
#if !defined(__AVR_ATmega32U4__)
    ____ "T" ________ _ooo_ _test_  _select "ed " _stepper " with direct port access" _NL_
#endif
#if defined(ARDUINO_ARCH_ESP32)
    ____ "r" ________ _ooo_ "Call ESP.restart()" _NL_
    ____ "reset" ____ _ooo_ _Perform_ "reset" _NL_
    ____ "p<n> " ____ _ooo_ _attach " " _pulse_counter_ "n<=7" _NL_
    ____ "p<n>,l,h " _ooo_ _attach " " _pulse_counter_ "n<=7 with low,high limits" _NL_
    ____ "pc   " ____ _ooo_ _clear_ _pulse_counter_ _NL_
#endif
    ____ "t" ________ _ooo_ _Enter_ _test_ _mode_ _NL_
    ____ "u" ________ _ooo_ "Unidirectional " _mode_ "(need reset " _to_ "restore)" _NL_
#if defined(ARDUINO_ARCH_AVR)
    ____ "r" ________ _ooo_ _Toggle_ _erroneous_ _digitalRead_ _of_the_ _stepper " pin" _NL_
#endif
    ____ "e" ________ _ooo_ _Toggle_ _erroneous_ "long 100us interrupt block" _NL_
    _toggle_print_usage_after_stepper_stop
	_print_this_usage_
    _SEP_


    /* USAGE TEST */
#ifndef SIM_TEST_INPUT
    _Enter_command_seperated_by_space_carriage_return_or_newline_NL
	_m1_m2_to_select_stepper_
    ____ "R" ________ _ooo_ "start all " _select "ed tests" _NL_
    ____ "I" ________ _ooo_ _Toggle_ _stepper " info, while " _test_sequence_ "is running" _NL_
    ____ "01   " ____ _ooo_ _select " " _test_sequence_ "01 for " _selected_stepper _NL_
    ____ ":" _NL_
    ____ "13   " ____ _ooo_ _select " " _test_sequence_ "13 for " _selected_stepper _NL_
    ____ "W" ________ _ooo_ "Blocking wait until test is finished" _NL_
    ____ "x" ________ _ooo_ "Exit test mode" _NL_
    _toggle_print_usage_after_stepper_stop
	_print_this_usage_
#endif
    _SEP_


    /* USAGE CONFIG */
#if !defined(SIM_TEST_INPUT) && !defined(__AVR_ATmega32U4__)
    _Enter_command_seperated_by_space_carriage_return_or_newline_NL
	_m1_m2_to_select_stepper_
    ____ "d<p> " ____ _ooo_ _set_ _direction_ _pin_ _NL_
    ____ "d<p,n>" _NL_
    ____ "d<p,n,t>" _NL_
    ________ ________ ________ "p" _ooo_ _pin_ "number" _NL_
    ________ ________ ________ "n" _ooo_ "1: high counts up 0: high counts down" _NL_
    ________ ________ ________ "t" _ooo_ _delay_ _from_ "dir change " _to_ "step in us, 0 means "
    "off" _NL_
    ____ "dc   " ____ _ooo_ _clear_ _direction_ _pin_ "(unidirectional)" _NL_
    ____ "x" ________ _ooo_ "Exit " _configuration_ _mode_ _NL_
	_print_this_usage_
#endif
    _SEP_
;
// clang-format on

// The preprocessor for .ino is buggy
#include "generic.h"

void output_msg(uint8_t x) {
  char ch;
  MSG_TYPE p = messages;
  while (x > 0) {
    ch = get_char(p);
    p++;
    if (ch == _SEP_CHAR_) {
      x--;
    }
  }
  for (;;) {
    ch = get_char(p);
    p++;
    if (ch == _SEP_CHAR_) {
      break;
    }
    uint8_t y = (uint8_t)(ch ^ 0x80);
    if (y <= MSG_USAGE_CONFIG) {
      output_msg(y);
    } else {
      Serial.print(ch);
    }
  }
}

#if !defined(__AVR_ATmega32U4__)
void delay10us() { delayMicroseconds(10); }
void do3200Steps(uint8_t step) {
  for (uint16_t i = 0; i < 3200; i++) {
    digitalWrite(step, HIGH);
    delay10us();
    if (digitalRead(step) != HIGH) {
      output_msg(MSG_STEP_PIN_IS_NOT_HIGH);
    }
    digitalWrite(step, LOW);
    delayMicroseconds(190);
    if (digitalRead(step) != LOW) {
      output_msg(MSG_STEP_PIN_IS_NOT_LOW);
    }
  }
}
void setDirectionPin(uint8_t direction, bool polarity) {
  if (direction != PIN_UNDEFINED) {
    digitalWrite(direction, polarity);
    pinMode(direction, OUTPUT);
    delay10us();
    if (digitalRead(direction) != polarity) {
      output_msg(MSG_CANNOT_SET_DIRECTION_PIN);
      Serial.println(polarity ? "HIGH" : "LOW");
    }
  }
}
void test_direct_drive(const struct stepper_config_s *stepper) {
  // Check stepper motor+driver is operational
  // This is not done via FastAccelStepper-Library for test purpose only
  uint8_t step = stepper->step;
  uint8_t enableLow = stepper->enable_low_active;
  uint8_t enableHigh = stepper->enable_high_active;
  uint8_t direction = stepper->direction;
  bool direction_high_count_up = stepper->direction_high_count_up;

  pinMode(step, OUTPUT);

  if (enableLow != PIN_UNDEFINED) {
    digitalWrite(enableLow, LOW);
    pinMode(enableLow, OUTPUT);
    delay10us();
    if (digitalRead(enableLow) != LOW) {
      output_msg(MSG_ENABLE_LOW_PIN_IS_NOT_LOW);
    }
  }
  if (enableHigh != PIN_UNDEFINED) {
    digitalWrite(enableHigh, HIGH);
    pinMode(enableHigh, OUTPUT);
    delay10us();
    if (digitalRead(enableHigh) != HIGH) {
      output_msg(MSG_ENABLE_HIGH_PIN_IS_NOT_HIGH);
    }
  }
  setDirectionPin(direction, direction_high_count_up);
  do3200Steps(step);
  setDirectionPin(direction, !direction_high_count_up);
  do3200Steps(step);
  if (enableLow != PIN_UNDEFINED) {
    digitalWrite(enableLow, HIGH);
    delay10us();
    if (digitalRead(enableLow) != HIGH) {
      output_msg(MSG_ENABLE_LOW_PIN_IS_NOT_HIGH);
    }
  }
  if (enableHigh != PIN_UNDEFINED) {
    digitalWrite(enableHigh, LOW);
    delay10us();
    if (digitalRead(enableHigh) != LOW) {
      output_msg(MSG_ENABLE_HIGH_PIN_IS_NOT_LOW);
    }
  }
  // Done
}
#endif

void setup() {
  Serial.begin(115200);
  output_msg(MSG_STEPPER_VERSION);
  Serial.print("    F_CPU=");
  Serial.println(F_CPU);
  Serial.print("    TICKS_PER_S=");
  Serial.println(TICKS_PER_S);

  // If you are not sure, that the stepper hardware is working,
  // then try first direct port manipulation and uncomment the next line.
  // Alternatively use e.g. M1 T by serial command
  // test_direct_drive(&stepper_config[0]);

  engine.init();
  if (led_pin != PIN_UNDEFINED) {
    engine.setDebugLed(led_pin);
  }

  for (uint8_t i = 0; i < MAX_STEPPER; i++) {
    FastAccelStepper *s = NULL;
    const struct stepper_config_s *config = &stepper_config[i];
    if (config->step != PIN_UNDEFINED) {
      s = engine.stepperConnectToPin(config->step);
      if (s) {
        s->setDirectionPin(config->direction, config->direction_high_count_up,
                           config->dir_change_delay);
        s->setEnablePin(config->enable_low_active, true);
        s->setEnablePin(config->enable_high_active, false);
        s->setAutoEnable(config->auto_enable);
        s->setDelayToEnable(config->on_delay_us);
        s->setDelayToDisable(config->off_delay_ms);
      }
    }
    stepper[i] = s;
  }

  usage();
}

#ifdef SIM_TEST_INPUT
const char *input = SIM_TEST_INPUT
    " ";  // final space is too easy forgotten in platformio.ini test
#else
const char *input = NULL;
#endif
uint8_t write_ptr = 0;
uint8_t read_ptr = 0;
char in_buffer[256];  // This allows to let in/out_ptr wrap around
uint8_t out_ptr = 0;
char out_buffer[256];
bool stopped = true;
bool verbose = true;
bool usage_info = true;
bool speed_in_milli_hz = false;
uint32_t last_time = 0;
int selected = 0;
uint32_t pause_ms = 0;
uint32_t pause_start = 0;
#if defined(ARDUINO_ARCH_AVR)
bool simulate_digitalRead_error = false;
#endif
bool simulate_blocked_ISR = false;

void info(FastAccelStepper *s, bool long_info) {
  Serial.print('@');
#if defined(SUPPORT_ESP32_PULSE_COUNTER)
  if (s->pulseCounterAttached()) {
    int16_t pcnt_pos_1 = s->readPulseCounter();
    int32_t pos = s->getCurrentPosition();
    int16_t pcnt_pos_2 = s->readPulseCounter();
    Serial.print(pos);
    Serial.print(" [");
    Serial.print(pcnt_pos_1);
    Serial.print(']');
    if (pcnt_pos_1 != pcnt_pos_2) {
      Serial.print(" [");
      Serial.print(pcnt_pos_2);
      Serial.print(']');
    }
  } else {
    int32_t pos = s->getCurrentPosition();
    Serial.print(pos);
  }
#else
  int32_t pos = s->getCurrentPosition();
  Serial.print(pos);
#endif
  if (s->isRunning()) {
    if (s->isRunningContinuously()) {
      Serial.print(" nonstop");
    } else {
      Serial.print(" => ");
      Serial.print(s->targetPos());
    }
    Serial.print(" QueueEnd=");
    Serial.print(s->getPositionAfterCommandsCompleted());
    if (speed_in_milli_hz) {
      Serial.print(" v=");
      Serial.print(s->getCurrentSpeedInMilliHz());
      Serial.print("mSteps/s");
    } else {
      Serial.print('/');
      Serial.print(s->getPeriodInUsAfterCommandsCompleted());
      Serial.print("us/");
      Serial.print(s->getPeriodInTicksAfterCommandsCompleted());
      Serial.print("ticks");
    }
    if (s->isRampGeneratorActive()) {
      switch (s->rampState() & RAMP_STATE_MASK) {
        case RAMP_STATE_IDLE:
          Serial.print(" IDLE ");
          break;
        case RAMP_STATE_ACCELERATE:
          Serial.print(" ACC  ");
          break;
        case RAMP_STATE_DECELERATE_TO_STOP:
          Serial.print(" DEC  ");
          break;
        case RAMP_STATE_DECELERATE:
          Serial.print(" RED  ");  // Reduce
          break;
        case RAMP_STATE_COAST:
          Serial.print(" COAST");
          break;
        case RAMP_STATE_REVERSE:
          Serial.print(" REV  ");
          break;
        default:
          Serial.print(s->rampState());
      }
    } else {
      Serial.print(" MANU");
    }
  } else {
    if (long_info) {
      Serial.print(" Acceleration [Steps/s^2]=");
      Serial.print(s->getAcceleration());
      if (speed_in_milli_hz) {
        Serial.print(" Speed [mStep/s]=");
        Serial.print(s->getSpeedInMilliHz());
      } else {
        Serial.print(" Speed [us/step]=");
        Serial.print(s->getSpeedInUs());
      }
    }
  }
  Serial.print(' ');
}

void stepper_info() {
  for (uint8_t i = 0; i < MAX_STEPPER; i++) {
    if (stepper[i]) {
      if (i == selected) {
        Serial.print(">> ");
      } else {
        Serial.print("   ");
      }
      Serial.print('M');
      Serial.print(i + 1);
      Serial.print(": ");
      info(stepper[i], true);
      Serial.println();
    }
  }
  if (simulate_blocked_ISR) {
    output_msg(MSG_LONG_INTERRUPT_BLOCK_ENABLED);
  }
#if defined(ARDUINO_ARCH_AVR)
  if (simulate_digitalRead_error) {
    output_msg(MSG_STRAY_DIGITAL_READ_ENABLED);
  }
#endif
}

void usage() {
  switch (mode) {
    case normal:
      output_msg(MSG_USAGE_NORMAL);
      break;
    case test:
      output_msg(MSG_USAGE_TEST);
      break;
    case config:
      output_msg(MSG_USAGE_CONFIG);
      break;
  }
  stepper_info();
}

void output_info(bool only_running) {
  bool need_ln = false;
  for (uint8_t i = 0; i < MAX_STEPPER; i++) {
    if (stepper[i]) {
      if (!only_running || stepper[i]->isRunning()) {
        need_ln = true;
        Serial.print('M');
        Serial.print(i + 1);
        Serial.print(": ");
        info(stepper[i], false);
      }
    }
  }
  if (need_ln) {
    Serial.println();
  }
}

#define MODE(mode, CMD) ((mode << 8) + CMD)

long val1, val2, val3;
int8_t get_val1_val2_val3(char *cmd) {
  char *endptr;
  val1 = strtol(cmd, &endptr, 10);
  if (endptr == cmd) {
    return -1;
  }
  cmd = endptr;
  if (*cmd == 0) {
    return 1;
  }
  if (*cmd++ != ',') {
    return -1;
  }
  val2 = strtol(cmd, &endptr, 10);
  if (endptr == cmd) {
    return -1;
  }
  cmd = endptr;
  if (*cmd == 0) {
    return 2;
  }
  if (*cmd++ != ',') {
    return -1;
  }
  val3 = strtol(cmd, &endptr, 10);
  if (endptr == cmd) {
    return -1;
  }
  cmd = endptr;
  if (*cmd == 0) {
    return 3;
  }
  return -1;
}

bool process_cmd(char *cmd) {
  FastAccelStepper *stepper_selected = stepper[selected];
  uint16_t s = *cmd++;
  char *endptr;
  int8_t res;
  switch (MODE(mode, s)) {
    case MODE(normal, 'M'):
    case MODE(test, 'M'):
    case MODE(config, 'M'):
      if (get_val1_val2_val3(cmd) == 1) {
        if ((val1 > 0) && (val1 <= MAX_STEPPER)) {
          output_msg(MSG_SELECT_STEPPER);
          selected = val1 - 1;
          Serial.println(selected + 1);
          return true;
        }
      }
      break;
    case MODE(normal, 'r'):
#if defined(ARDUINO_ARCH_ESP32)
      if (strcmp(cmd, "eset") == 0) {
        Serial.println("ESP reset");
        esp_task_wdt_init(1, true);
        esp_task_wdt_add(NULL);
        while (true)
          ;
      }
      Serial.println("ESP restart");
      ESP.restart();
#endif
#if defined(ARDUINO_ARCH_AVR)
      if (*cmd == 0) {
        output_msg(MSG_STRAY_DIGITAL_READ_TOGGLE);
        simulate_digitalRead_error ^= true;
        return true;
      }
#endif
      break;
    case MODE(normal, 'I'):
    case MODE(test, 'I'):
      if (*cmd == 0) {
        output_msg(MSG_TOGGLE_MOTOR_INFO);
        verbose = !verbose;
        return true;
      }
      break;
    case MODE(normal, 'Q'):
    case MODE(test, 'Q'):
      if (*cmd == 0) {
        output_msg(MSG_TOGGLE_USAGE_INFO);
        usage_info = !usage_info;
        return true;
      }
      break;
    case MODE(normal, '?'):
    case MODE(test, '?'):
    case MODE(config, '?'):
      if (*cmd == 0) {
        usage();
        return true;
      }
      break;
    case MODE(normal, 't'):
      if (*cmd == 0) {
        output_msg(MSG_ENTER_TEST_MODE);
        mode = test;
        usage();
        return true;
      }
      break;
    case MODE(normal, 'c'):
      if (*cmd == 0) {
        output_msg(MSG_ENTER_CONFIG_MODE);
        mode = config;
        usage();
        return true;
      }
      break;
    case MODE(normal, 'e'):
      if (*cmd == 0) {
        output_msg(MSG_LONG_INTERRUPT_BLOCK_TOGGLE);
        simulate_blocked_ISR ^= true;
        return true;
      }
      break;
    case MODE(normal, 'A'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        output_msg(MSG_SET_ACCELERATION_TO);
        Serial.println(val1);
        res = stepper_selected->setAcceleration(val1);
        if (res < 0) {
          output_msg(MSG_ERROR_INVALID_VALUE);
        }
        return true;
      }
      break;
    case MODE(normal, 'V'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        speed_in_milli_hz = false;
        output_msg(MSG_SET_SPEED_TO_US);
        Serial.println(val1);
        res = stepper_selected->setSpeedInUs(val1);
        if (res < 0) {
          output_msg(MSG_ERROR_INVALID_VALUE);
        }
        return true;
      }
      break;
    case MODE(normal, 'H'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        speed_in_milli_hz = true;
        output_msg(MSG_SET_SPEED_TO_HZ);
        Serial.println(val1);
        res = stepper_selected->setSpeedInHz(val1);
        if (res < 0) {
          output_msg(MSG_ERROR_INVALID_VALUE);
        }
        return true;
      }
      break;
    case MODE(normal, 'h'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        speed_in_milli_hz = true;
        output_msg(MSG_SET_SPEED_TO_MILLI_HZ);
        Serial.println(val1);
        res = stepper_selected->setSpeedInMilliHz(val1);
        if (res < 0) {
          output_msg(MSG_ERROR_INVALID_VALUE);
        }
        return true;
      }
      break;
    case MODE(normal, 'a'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        output_msg(MSG_SET_ACCELERATION_TO);
        Serial.println(val1);
        res = stepper_selected->moveByAcceleration(val1);
        output_msg(MSG_MOVE_OK + res);
        return true;
      }
      break;
    case MODE(normal, 'R'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        output_msg(MSG_MOVE_STEPS);
        Serial.println(val1);
        res = stepper_selected->move(val1);
        output_msg(MSG_MOVE_OK + res);
        return true;
      }
      break;
    case MODE(normal, 'P'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        output_msg(MSG_MOVE_TO_POSITION);
        Serial.println(val1);
        res = stepper_selected->moveTo(val1);
        output_msg(MSG_MOVE_OK + res);
        return true;
      }
      break;
    case MODE(normal, '@'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        output_msg(MSG_SET_POSITION);
        Serial.println(val1);
        stepper_selected->setCurrentPosition(val1);
        return true;
      }
      break;
    case MODE(normal, 'E'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        output_msg(MSG_SET_ENABLE_TIME);
        Serial.println(val1);
        res = stepper_selected->setDelayToEnable(val1);
        output_msg(MSG_RETURN_CODE);
        Serial.println(res);
        return true;
      }
      break;
    case MODE(normal, 'D'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        output_msg(MSG_SET_DISABLE_TIME);
        Serial.println(val1);
        stepper_selected->setDelayToDisable(val1);
        return true;
      }
      break;
    case MODE(normal, 'w'):
      val1 = strtol(cmd, &endptr, 10);
      if (*endptr == 0) {
        Serial.print(val1);
        output_msg(MSG_WAIT_MS);
        pause_ms = val1;
        pause_start = millis();
        return true;
      }
      break;
    case MODE(config, 'd'):
      if (strcmp(cmd, "c") == 0) {
        output_msg(MSG_DISABLED);
        stepper_selected->setDirectionPin(PIN_UNDEFINED);
        return true;
      }
      switch (get_val1_val2_val3(cmd)) {
        case 1:
          output_msg(MSG_DIRECTION_PIN);
          output_msg(MSG_SET_TO_PIN);
          Serial.println(val1);
          stepper_selected->setDirectionPin(val1);
          return true;
        case 2:
          output_msg(MSG_DIRECTION_PIN);
          output_msg(MSG_SET_TO_PIN);
          Serial.println(val1);
          output_msg(MSG_DIRECTION_PIN);
          if (val2 != 0) {
            output_msg(MSG_HIGH_COUNT_DOWN);
          } else {
            output_msg(MSG_HIGH_COUNT_UP);
          }
          stepper_selected->setDirectionPin(val1, val2);
          return true;
        case 3:
          val3 = strtol(cmd, &endptr, 10);
          output_msg(MSG_DIRECTION_PIN);
          output_msg(MSG_SET_TO_PIN);
          Serial.println(val1);
          output_msg(MSG_DIRECTION_PIN);
          if (val2 != 0) {
            output_msg(MSG_HIGH_COUNT_DOWN);
          } else {
            output_msg(MSG_HIGH_COUNT_UP);
          }
          output_msg(MSG_DELAY);
          Serial.println(val3);
          stepper_selected->setDirectionPin(val1, val2, val3);
          return true;
        default:
          break;
      }
    case MODE(normal, 'N'):
      if (*cmd == 0) {
        output_msg(MSG_OUTPUT_DRIVER_ON);
        stepper_selected->setAutoEnable(false);
        stepper_selected->enableOutputs();
        return true;
      }
      break;
    case MODE(normal, 'F'):
      if (*cmd == 0) {
        output_msg(MSG_OUTPUT_DRIVER_OFF);
        stepper_selected->setAutoEnable(false);
        stepper_selected->disableOutputs();
        return true;
      }
      break;
    case MODE(normal, 'O'):
      if (*cmd == 0) {
        output_msg(MSG_OUTPUT_DRIVER_AUTO);
        stepper_selected->setAutoEnable(true);
        return true;
      }
      break;
    case MODE(normal, 'S'):
      if (*cmd == 0) {
        output_msg(MSG_STOP);
        stepper_selected->stopMove();
        return true;
      }
      break;
    case MODE(normal, 'K'):
      if (*cmd == 0) {
        output_msg(MSG_KEEP_RUNNING);
        stepper_selected->keepRunning();
        return true;
      }
      break;
    case MODE(normal, 'f'):
      if (*cmd == 0) {
        output_msg(MSG_RUN_FORWARD);
        res = stepper_selected->runForward();
        output_msg(MSG_RETURN_CODE);
        Serial.println(res);
        return true;
      }
      break;
    case MODE(normal, 'b'):
      if (*cmd == 0) {
        output_msg(MSG_RUN_BACKWARD);
        res = stepper_selected->runBackward();
        output_msg(MSG_RETURN_CODE);
        Serial.println(res);
        return true;
      }
      break;
    case MODE(normal, 'X'):
      if (*cmd == 0) {
        output_msg(MSG_IMMEDIATE_STOP);
        stepper_selected->forceStopAndNewPosition(0);
        return true;
      }
      break;
    case MODE(normal, 'U'):
      if (*cmd == 0) {
        output_msg(MSG_UPDATE_SPEED_ACCELERATION);
        stepper_selected->applySpeedAcceleration();
        return true;
      }
      break;
    case MODE(normal, 'u'):
      if (*cmd == 0) {
        output_msg(MSG_SET_UNIDIRECTIONAL_STEPPER);
        stepper_selected->setDirectionPin(PIN_UNDEFINED);
        return true;
      }
      break;
    case MODE(normal, 'W'):
    case MODE(test, 'W'):
      if (*cmd == 0) {
#ifdef SIM_TEST_INPUT
        if (stepper_selected->isRunning() || test_ongoing) {
          read_ptr -= 2;
        }
#else
        output_msg(MSG_BLOCKING_WAIT);
        if (!stepper_selected->isRunningContinuously() ||
            stepper_selected->isStopping()) {
          // Wait for stepper stop
          while (stepper_selected->isRunning()) {
            // do nothing
          }
          Serial.println("STOPPED");
        }
#endif
        return true;
      }
      break;
#if !defined(__AVR_ATmega32U4__)
    case MODE(normal, 'T'):
      if (*cmd == 0) {
        if (!stepper_selected->isRunning()) {
          output_msg(MSG_TEST_DIRECT_DRIVE);
          stepper_selected->detachFromPin();
          test_direct_drive(&stepper_config[selected]);
          stepper_selected->reAttachToPin();
        }
        return true;
      }
      break;
#endif
    case MODE(normal, '+'):
      if (*cmd == 0) {
        if (!stepper_selected->isRunning()) {
          stepper_selected->forwardStep(true);
          output_msg(MSG_STEPPED_FORWARD);
        }
        return true;
      }
      break;
    case MODE(normal, '-'):
      if (*cmd == 0) {
        if (!stepper_selected->isRunning()) {
          stepper_selected->backwardStep(true);
          output_msg(MSG_STEPPED_BACKWARD);
        }
        return true;
      }
      break;
    case MODE(test, 'x'):
    case MODE(config, 'x'):
      if (*cmd == 0) {
        output_msg(MSG_EXIT_TO_MAIN_MENU);
        test_ongoing = false;
        mode = normal;
        usage();
        return true;
      }
      break;
    case MODE(test, 'R'):
      if (*cmd == 0) {
        output_msg(MSG_RUN_TESTS);
        test_ongoing = true;
        return true;
      }
      break;
#if defined(SUPPORT_ESP32_PULSE_COUNTER)
    case MODE(normal, 'p'):
      if (strcmp(cmd, "c") == 0) {
        output_msg(MSG_CLEAR_PULSE_COUNTER);
        stepper_selected->clearPulseCounter();
        return true;
      }
      if (get_val1_val2_val3(cmd) == 3) {
        output_msg(MSG_ATTACH_PULSE_COUNTER);
        Serial.println(val1);
        if (!stepper_selected->attachToPulseCounter(val1, val2, val3)) {
          output_msg(MSG_ERROR_ATTACH_PULSE_COUNTER);
        }
        return true;
      }
      if (get_val1_val2_val3(cmd) == 1) {
        output_msg(MSG_ATTACH_PULSE_COUNTER);
        Serial.println(val1);
        if (!stepper_selected->attachToPulseCounter(val1)) {
          output_msg(MSG_ERROR_ATTACH_PULSE_COUNTER);
        }
        return true;
      }
      break;
#endif
    case MODE(test, '0'):
    case MODE(test, '1'):
      for (uint8_t i = 0; i < NUM_TEST_SEQUENCE; i++) {
        const struct test_seq_def_s *ts = &test_sequence[i];
        if (strcmp(out_buffer, ts->code) == 0) {
          output_msg(MSG_SELECT_TEST_SEQUENCE);
          Serial.println(out_buffer);
          test_seq[selected].test = ts->test;
          test_seq[selected].state = 0;
          return true;
        }
      }
      break;
  }

  return false;
}

void loop() {
  char ch = 0;
  if (input == NULL) {
    if (Serial.available()) {
      ch = Serial.read();
    }
  } else {
    ch = *input;
    if (ch == 0) {
      if (read_ptr == write_ptr) {
        input = NULL;
#ifdef SIM_TEST_INPUT
        delay(1000);
        noInterrupts();
        sleep_cpu();
#endif
      }
    } else {
      input++;
    }
  }

  if (ch != 0) {
    if ((uint8_t)(write_ptr + 1) == read_ptr) {
      read_ptr++;
    }
    in_buffer[write_ptr++] = ch;
  }
  if (pause_ms > 0) {
    if ((uint32_t)(millis() - pause_start) >= pause_ms) {
      pause_ms = 0;
      output_msg(MSG_WAIT_COMPLETE);
    }
  } else if (read_ptr != write_ptr) {
    ch = in_buffer[read_ptr++];
    if ((ch != ' ') && (ch != '\n') && (ch != '\r')) {
      if (out_ptr == 255) {
        out_ptr = 0;
      }
      out_buffer[out_ptr++] = ch;
    } else if ((ch == ' ') || (ch == '\n') || (ch == '\r')) {
      out_buffer[out_ptr] = 0;

      if (out_ptr > 0) {
        if (!process_cmd(out_buffer)) {
          output_msg(MSG_UNKNOWN_COMMAND);
          Serial.println(out_buffer);
        }
      }
      out_ptr = 0;
    }
  }

  uint32_t ms = millis();

#if defined(ARDUINO_ARCH_AVR)
  if (simulate_digitalRead_error) {
    digitalRead(stepPinStepperA);
    digitalRead(stepPinStepperB);
#ifdef stepPinStepperC
    digitalRead(stepPinStepperC);
#endif
  }
#endif
  if (simulate_blocked_ISR) {
    if ((ms & 0xff) < 0x40) {
      noInterrupts();
      delayMicroseconds(100);
      interrupts();
    }
  }

  if (mode == test) {
    if (test_ongoing) {
      bool finished = true;
      for (uint8_t i = 0; i < MAX_STEPPER; i++) {
        struct test_seq_s *s = &test_seq[i];
        if ((s->test != NULL) && (s->state != TEST_STATE_ERROR)) {
          bool res = s->test(stepper[i], &test_seq[i], ms);
          if (res) {
            s->test = NULL;
          }
          finished &= res;
        }
      }
      if (finished) {
        test_ongoing = false;
        bool test_failed = false;
        stepper_info();
        for (uint8_t i = 0; i < MAX_STEPPER; i++) {
          struct test_seq_s *s = &test_seq[i];
          s->test = NULL;
          if (s->state == TEST_STATE_ERROR) {
            test_failed = true;
          }
        }
        if (test_failed) {
          output_msg(MSG_FAILED_STATUS);
        } else {
          output_msg(MSG_PASS_STATUS);
        }
        output_msg(MSG_TEST_COMPLETED);
      } else {
        uint32_t now = millis();
        if (now - last_time >= 100) {
          if (verbose) {
            output_info(true);
          }
          last_time = now;
        }
      }
    }
  } else {
    bool running = false;
    for (uint8_t i = 0; i < MAX_STEPPER; i++) {
      if (stepper[i]) {
        running |= stepper[i]->isRunning();
      }
    }
    if (running) {
      if (ms - last_time >= 100) {
        if (verbose) {
          output_info(true);
        }
        last_time = ms;
      }
    }
    if (!stopped && !running) {
      output_info(true);
      if (usage_info) {
        usage();
      }
    }
    stopped = !running;
  }
}
