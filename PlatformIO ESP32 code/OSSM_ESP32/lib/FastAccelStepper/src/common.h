#ifndef COMMON_H
#define COMMON_H

#define TICKS_FOR_STOPPED_MOTOR 0xffffffff

//	ticks is multiplied by (1/TICKS_PER_S) in s
//	If steps is 0, then a pause is generated
struct stepper_command_s {
  uint16_t ticks;
  uint8_t steps;
  bool count_up;
};

struct queue_end_s {
  volatile int32_t pos;  // in steps
  volatile bool count_up;
  volatile bool dir;
};

// use own min/max/abs function, because the lib versions are messed up
#define fas_min(a, b) ((a) > (b) ? (b) : (a))
#define fas_max(a, b) ((a) > (b) ? (a) : (b))
#define fas_abs(x) ((x) >= 0 ? (x) : (-x))

//==============================================================================
// All architecture specific definitions should be located here
//==============================================================================

// disable inject_fill_interrupt() for all real devices
#ifndef TEST
#define inject_fill_interrupt(x)
#endif

//==========================================================================
//
// The TEST "architecture" is in use with pc_based testing.
//
//
//==========================================================================
#if defined(TEST)
// For pc-based testing like to have assert-macro
#include <assert.h>

// and some more includes
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../extras/tests/pc_based/stubs.h"

// For pc-based testing, the macro TEST is defined. The pc-based testing does
// not support the concept of interrupts, so provide an empty definition
#define fasEnableInterrupts()
#define fasDisableInterrupts()

// The TEST target needs a couple of arduino like definitions
#define LOW 0
#define HIGH 1

// queue definitions for pc based testing
#define MAX_STEPPER 2
#define NUM_QUEUES 2
#define fas_queue_A fas_queue[0]
#define fas_queue_B fas_queue[1]
#define QUEUE_LEN 16

// timing definitions for pc-based testing
#define TICKS_PER_S 16000000L
#define MIN_CMD_TICKS (TICKS_PER_S / 5000)
#define MIN_DIR_DELAY_US (MIN_CMD_TICKS / (TICKS_PER_S / 1000000))
#define MAX_DIR_DELAY_US (65535 / (TICKS_PER_S / 1000000))
#define DELAY_MS_BASE 1

#define noop_or_wait

#define SUPPORT_QUEUE_ENTRY_END_POS_U16

//==========================================================================
//
// This for ESP32 derivates using arduino core
//
//==========================================================================
#elif defined(ARDUINO_ARCH_ESP32)
// this is an arduino platform, so include the Arduino.h header file
#include <Arduino.h>
#include <sdkconfig.h>

#define SUPPORT_ESP32
#define SUPPORT_EXTERNAL_DIRECTION_PIN

// Some more esp32 specific includes
#include <driver/gpio.h>
#include <esp_task_wdt.h>

//==========================================================================
//
// ESP32 derivate - the first one
//
//==========================================================================

#if CONFIG_IDF_TARGET_ESP32
#define SUPPORT_ESP32_MCPWM_PCNT
#include <driver/mcpwm.h>
#include <driver/pcnt.h>
#include <soc/mcpwm_reg.h>
#include <soc/mcpwm_struct.h>
#include <soc/pcnt_reg.h>
#include <soc/pcnt_struct.h>

#define SUPPORT_ESP32_RMT
#include <driver/rmt.h>
#define QUEUES_MCPWM_PCNT 6
#define QUEUES_RMT 8

// have support for pulse counter
#define SUPPORT_ESP32_PULSE_COUNTER
#define FAS_RMT_MEM(channel) ((uint32_t *)RMT_CHANNEL_MEM(channel))

//==========================================================================
//
// ESP32 derivate - ESP32S2
//
//==========================================================================
#elif CONFIG_IDF_TARGET_ESP32S2
#define SUPPORT_ESP32_RMT
#include <driver/periph_ctrl.h>
#include <driver/rmt.h>
#define QUEUES_MCPWM_PCNT 0
#define QUEUES_RMT 4
#define FAS_RMT_MEM(channel) ((uint32_t *)RMTMEM.chan[channel].data32)

//==========================================================================
//
// ESP32 derivate - ESP32C3 - NOT SUPPORTED
//
//==========================================================================
#elif CONFIG_IDF_TARGET_ESP32C3
#error "esp32c3 is not supported"
#define SUPPORT_ESP32_RMT
#include <driver/periph_ctrl.h>
#include <driver/rmt.h>
#include <soc/rmt_periph.h>
#include <soc/rmt_reg.h>
#include <soc/rmt_struct.h>
#define QUEUES_MCPWM_PCNT 0
#define QUEUES_RMT 2

//==========================================================================
//
// For all unsupported ESP32 derivates
//
//==========================================================================
#else
#error "Unsupported derivate"
#endif

// For esp32 using arduino, just use arduino definition
#define fasEnableInterrupts interrupts
#define fasDisableInterrupts noInterrupts

#if ESP_IDF_VERSION_MAJOR == 4
#define __ESP32_IDF_V44__
#include <driver/periph_ctrl.h>
#include <soc/periph_defs.h>
#include <soc/rmt_periph.h>
#include <soc/rmt_reg.h>
#include <soc/rmt_struct.h>
#elif ESP_IDF_VERSION_MAJOR == 3
#endif

// Esp32 queue definitions
#define NUM_QUEUES (QUEUES_MCPWM_PCNT + QUEUES_RMT)
#define MAX_STEPPER (NUM_QUEUES)
#define QUEUE_LEN 32

// Esp32 timing definition
#define TICKS_PER_S 16000000L
#define MIN_CMD_TICKS (TICKS_PER_S / 2000)
#define MIN_DIR_DELAY_US (MIN_CMD_TICKS / (TICKS_PER_S / 1000000))
#define MAX_DIR_DELAY_US (65535 / (TICKS_PER_S / 1000000))
#define DELAY_MS_BASE 4

#define SUPPORT_QUEUE_ENTRY_START_POS_U16

// debug led timing
#define DEBUG_LED_HALF_PERIOD 50

#define noop_or_wait vTaskDelay(1)

// have more than one core
#define SUPPORT_CPU_AFFINITY

//==========================================================================
//
// This for ESP32 derivates using espidf
//
// This is most likely broken and not tested on github actions
//
//==========================================================================
#elif defined(ESP_PLATFORM)

#define SUPPORT_ESP32

// esp32 specific includes
#include <driver/gpio.h>
#include <driver/mcpwm.h>
#include <driver/pcnt.h>
#include <esp_task_wdt.h>
#include <math.h>
#include <soc/mcpwm_reg.h>
#include <soc/mcpwm_struct.h>
#include <soc/pcnt_reg.h>
#include <soc/pcnt_struct.h>

// on espidf need to use portDISABLE/ENABLE_INTERRUPTS
//
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#define fasDisableInterrupts portDISABLE_INTERRUPTS
#define fasEnableInterrupts portENABLE_INTERRUPTS

// Only since esp-idf v4.4 MCPWM_TIMER0_PHASE_DIRECTION_S is defined. So use
// this to distinguish between the two versions
#if defined(MCPWM_TIMER0_PHASE_DIRECTION_S)
#define __ESP32_IDF_V44__
#endif

// Esp32 queue definitions
#define MAX_STEPPER 6
#define NUM_QUEUES 6
#define QUEUE_LEN 32

// Esp32 timing definition
#define TICKS_PER_S 16000000L
#define MIN_CMD_TICKS (TICKS_PER_S / 2000)
#define MIN_DIR_DELAY_US (MIN_CMD_TICKS / (TICKS_PER_S / 1000000))
#define MAX_DIR_DELAY_US (65535 / (TICKS_PER_S / 1000000))
#define DELAY_MS_BASE 4

// The espidf-platform needs a couple of arduino like definitions
#define LOW 0
#define HIGH 1
#define OUTPUT GPIO_MODE_OUTPUT
#define pinMode(pin, mode) gpio_set_direction((gpio_num_t)pin, mode)
#define digitalWrite(pin, level) gpio_set_level((gpio_num_t)pin, level)

// debug led timing
#define DEBUG_LED_HALF_PERIOD 50

#define noop_or_wait vTaskDelay(1)

#define SUPPORT_QUEUE_ENTRY_START_POS_U16

// have support for pulse counter
#define SUPPORT_ESP32_PULSE_COUNTER

// have more than one core
#define SUPPORT_CPU_AFFINITY

//==========================================================================
//
// This for SAM-architecture
//
//==========================================================================
#elif defined(ARDUINO_ARCH_SAM)
#define SUPPORT_SAM

// this is an arduino platform, so include the Arduino.h header file
#include <Arduino.h>
// on SAM just use the arduino macros
#define fasEnableInterrupts interrupts
#define fasDisableInterrupts noInterrupts

// queue definitions for SAM
#define MAX_STEPPER 6
#define NUM_QUEUES 6
#define QUEUE_LEN 32

// timing definitions for SAM
#define TICKS_PER_S 21000000L
#define MIN_CMD_TICKS (TICKS_PER_S / 5000)
#define MIN_DIR_DELAY_US (MIN_CMD_TICKS / (TICKS_PER_S / 1000000))
#define MAX_DIR_DELAY_US (65535 / (TICKS_PER_S / 1000000))
#define DELAY_MS_BASE 2

// debug led timing
#define DEBUG_LED_HALF_PERIOD 50

#define noop_or_wait

#define SUPPORT_DIR_PIN_MASK uint32_t

// TO BE CHECKED
#define SUPPORT_QUEUE_ENTRY_END_POS_U16

//==========================================================================
//
// This for the AVR family
//
//==========================================================================
#elif defined(ARDUINO_ARCH_AVR)
#define SUPPORT_AVR

// this is an arduino platform, so include the Arduino.h header file
#include <Arduino.h>
#include "AVRStepperPins.h"
// for AVR processors a reentrant version of disabling/enabling interrupts is
// used
#define fasDisableInterrupts() \
  uint8_t prevSREG = SREG;     \
  cli()
#define fasEnableInterrupts() SREG = prevSREG

// Here are shorthand definitions for number of queues, the queues/channel
// relation and queue length This definitions are derivate specific
#define QUEUE_LEN 16
#define TICKS_PER_S F_CPU
#define MIN_CMD_TICKS (TICKS_PER_S / 25000)
#define MIN_DIR_DELAY_US 40
#define MAX_DIR_DELAY_US (65535 / (TICKS_PER_S / 1000000))
#define DELAY_MS_BASE (65536000 / TICKS_PER_S)

// debug led timing
#define DEBUG_LED_HALF_PERIOD (TICKS_PER_S / 65536 / 2)

#define noop_or_wait

#define SUPPORT_DIR_PIN_MASK uint8_t

#define SUPPORT_QUEUE_ENTRY_END_POS_U16

//==========================================================================
//
// AVR derivate ATmega 328P
//
//==========================================================================
#if defined(__AVR_ATmega328P__)
#define MAX_STEPPER 2
#define NUM_QUEUES 2
#define fas_queue_A fas_queue[0]
#define fas_queue_B fas_queue[1]
enum channels { channelA, channelB };
//==========================================================================
//
// AVR derivate ATmega 2560
//
//==========================================================================
#elif defined(__AVR_ATmega2560__)
#define MAX_STEPPER 3
#define NUM_QUEUES 3
#define fas_queue_A fas_queue[0]
#define fas_queue_B fas_queue[1]
#define fas_queue_C fas_queue[2]
enum channels { channelA, channelB, channelC };
//==========================================================================
//
// AVR derivate ATmega 32U4
//
//==========================================================================
#elif defined(__AVR_ATmega32U4__)
#define MAX_STEPPER 3
#define NUM_QUEUES 3
#define fas_queue_A fas_queue[0]
#define fas_queue_B fas_queue[1]
#define fas_queue_C fas_queue[2]
enum channels { channelA, channelB, channelC };
//==========================================================================
//
// For all unsupported AVR derivates
//
//==========================================================================
#else
#error "Unsupported AVR derivate"
#endif

//==========================================================================
//
// For all unsupported devices
//
//==========================================================================
#else
#error "Unsupported devices"

#endif

#ifdef __ESP32_IDF_V44__
#include <driver/periph_ctrl.h>
#include <soc/periph_defs.h>
#endif /* __ESP32_IDF_V44__ */

#endif /* COMMON_H */
