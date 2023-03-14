#ifndef CO_CONFIG_TARGET_H
#define CO_CONFIG_TARGET_H

#include "config.h"
#include "301/CO_config.h"

#define CO_CONFIG_NMT ( \
  CO_CONFIG_NMT_MASTER | \
  CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE | \
  CO_CONFIG_GLOBAL_FLAG_TIMERNEXT \
)

#define CO_CONFIG_SDO_CLI (CO_CONFIG_SDO_CLI_ENABLE)

#define CO_CONFIG_LSS (CO_CONFIG_LSS_MASTER | CO_CONFIG_LSS_SLAVE | CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE)
#define CO_CONFIG_FIFO (CO_CONFIG_FIFO_ENABLE)

#define CAN_TX_QUEUE_LENGTH (5) /** CAN TX buffer size */
#define CAN_RX_QUEUE_LENGTH (5) /** CAN RX buffer size */

#define CO_CAN_PSEUDO_INTERRUPT_INTERVAL (1000) /** CAN "Interrupt" Interval in μs*/
#define CO_MAIN_TASK_INTERVAL (1000)   /* Interval of tmrTask thread in microseconds */

#define CAN_MS_TO_WAIT (10000) /*CAN TX/RX Timeout value*/

#define NMT_CONTROL ((CO_NMT_control_t)(CO_NMT_STARTUP_TO_OPERATIONAL | CO_NMT_ERR_ON_ERR_REG))
#define FIRST_HB_TIME 500
#define SDO_SRV_TIMEOUT_TIME 1000
#define SDO_CLI_TIMEOUT_TIME 500
#define SDO_CLI_BLOCK false
#define OD_STATUS_BITS NULL

#endif