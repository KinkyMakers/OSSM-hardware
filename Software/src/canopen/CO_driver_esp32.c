/*
 * CAN module object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @author      Alexander Miller
 * @author      Mathias Parys
 * @copyright   2004 - 2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "soc/soc.h"

#include "CANopen.h"
#include "CO_config_target.h"
#include "301/CO_driver.h"
#include "301/CO_Emergency.h"

#include "driver/twai.h"
#include "driver/gpio.h"

CO_CANmodule_t *CANmodulePointer = NULL;

//CAN Timing configuration
static twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_500KBITS();     //Set Baudrate to 1Mbit
                                                                          //CAN Filter configuration
static twai_filter_config_t filterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL(); //Disable Message Filter
                                                                          //CAN General configuration
                                                                          
static twai_general_config_t generalConfig = {.mode = TWAI_MODE_NORMAL,
                                             .tx_io = CAN_TX_IO,                  /*TX IO Pin (CO_config.h)*/
                                             .rx_io = CAN_RX_IO,                  /*RX IO Pin (CO_config.h)*/
                                             .clkout_io = TWAI_IO_UNUSED,          /*No clockout pin*/
                                             .bus_off_io = TWAI_IO_UNUSED,         /*No busoff pin*/
                                             .tx_queue_len = CAN_TX_QUEUE_LENGTH, /*ESP TX Buffer Size (CO_config.h)*/
                                             .rx_queue_len = CAN_RX_QUEUE_LENGTH, /*ESP RX Buffer Size (CO_config.h)*/
                                             .alerts_enabled = TWAI_ALERT_ALL | TWAI_ALERT_AND_LOG,    /*Disable CAN Alarms TODO: Enable for CO_CANverifyErrors*/
                                             .clkout_divider = 0};                /*No Clockout*/

//Timer Interrupt Configuration
const esp_timer_create_args_t CO_CANinterruptArgs = {
    .callback = &CO_CANinterrupt, /*Timer Interrupt Callback */
    .name = "CO_CANinterrupt"};   /* Optional Task Name for debugging */

//Timer Handle
esp_timer_handle_t CO_CANinterruptPeriodicTimer;

/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANdriverState)
{
    /* Put CAN module in configuration mode */
    (void)CANdriverState;
}

/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule)
{
    
      ESP_LOGI("CANOpen.init", "install");
    /*Install CAN driver*/
    ESP_ERROR_CHECK(twai_driver_install(&generalConfig, &timingConfig, &filterConfig));
    /*Start CAN Controller*/
      ESP_LOGI("CANOpen.init", "start");
    ESP_ERROR_CHECK(twai_start());

      ESP_LOGI("CANOpen.init", "add handler");
    /*WORKAROUND: INTERRUPT ALLOCATION FÜR CAN NICHT MÖGLICH DA BEREITS IM IDF TREIBER VERWENDET*/
    /* Configure Timer interrupt function for execution every CO_CAN_PSEUDO_INTERRUPT_INTERVAL */
    ESP_ERROR_CHECK(esp_timer_create(&CO_CANinterruptArgs, &CO_CANinterruptPeriodicTimer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(CO_CANinterruptPeriodicTimer, CO_CAN_PSEUDO_INTERRUPT_INTERVAL));
    /*Set Canmodule to normal mode*/
    CANmodule->CANnormal = true;
}

/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
    CO_CANmodule_t *CANmodule,
    void *CANptr,
    CO_CANrx_t rxArray[],
    uint16_t rxSize,
    CO_CANtx_t txArray[],
    uint16_t txSize,
    uint16_t CANbitRate)
{
    CANmodulePointer = CANmodule;

    /* verify arguments */
    if (CANmodule == NULL || rxArray == NULL || txArray == NULL)
    {
        ESP_LOGE("CO_CANmodule_init", "Verify arguments! (Nullpointer in CANmodule|rxArray|txArray)");
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->CANptr = CANptr;
    CANmodule->rxSize = rxSize;
    CANmodule->rxArray = rxArray;
    CANmodule->txSize = txSize;
    CANmodule->txArray = txArray;
    CANmodule->useCANrxFilters = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANnormal = false;
    CANmodule->bufferInhibitFlag = false;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;

    /*Init RX-Array*/
    for (uint16_t i = 0U; i < rxSize; i++)
    {
        CANmodule->rxArray[i].ident = 0U;
        CANmodule->rxArray[i].mask = (uint16_t)0xFFFFFFFFU;
        CANmodule->rxArray[i].object = NULL;
        CANmodule->rxArray[i].CANrx_callback = NULL;
    }
    /*Init TX-Array*/
    for (uint16_t i = 0U; i < txSize; i++)
    {
        CANmodule->txArray[i].bufferFull = false;
    }

    /* Configure CAN module registers */
    generalConfig.mode = TWAI_MODE_NORMAL;
    generalConfig.tx_io = CAN_TX_IO;
    generalConfig.rx_io = CAN_RX_IO;
    generalConfig.clkout_io = TWAI_IO_UNUSED;
    generalConfig.bus_off_io = TWAI_IO_UNUSED;
    generalConfig.tx_queue_len = CAN_TX_QUEUE_LENGTH;
    generalConfig.rx_queue_len = CAN_RX_QUEUE_LENGTH;
    generalConfig.alerts_enabled = TWAI_ALERT_ALL;
    generalConfig.clkout_divider = 0;

    /* Configure CAN timing */
    switch (CANbitRate)
    {
    case 25:
        //Set Baudrate to 25kbit;
        timingConfig.brp = 128;
        timingConfig.tseg_1 = 16;
        timingConfig.tseg_2 = 8;
        timingConfig.sjw = 3;
        timingConfig.triple_sampling = false;
        break;
    case 50:
        //Set Baudrate to 50kbit;
        timingConfig.brp = 80;
        timingConfig.tseg_1 = 15;
        timingConfig.tseg_2 = 4;
        timingConfig.sjw = 3;
        timingConfig.triple_sampling = false;
        break;
    case 100:
        //Set Baudrate to 100kbit;
        timingConfig.brp = 40;
        timingConfig.tseg_1 = 15;
        timingConfig.tseg_2 = 4;
        timingConfig.sjw = 3;
        timingConfig.triple_sampling = false;
        break;
    case 125:
        //Set Baudrate to 125kbit;
        timingConfig.brp = 32;
        timingConfig.tseg_1 = 15;
        timingConfig.tseg_2 = 4;
        timingConfig.sjw = 3;
        timingConfig.triple_sampling = false;
        break;
    case 250:
        //Set Baudrate to 250kbit;
        timingConfig.brp = 16;
        timingConfig.tseg_1 = 15;
        timingConfig.tseg_2 = 4;
        timingConfig.sjw = 3;
        timingConfig.triple_sampling = false;
        break;
    case 500:
        //Set Baudrate to 500kbit;
        timingConfig.brp = 8;
        timingConfig.tseg_1 = 15;
        timingConfig.tseg_2 = 4;
        timingConfig.sjw = 3;
        timingConfig.triple_sampling = false;
        break;
    case 800:
        //Set Baudrate to 800kbit;
        timingConfig.brp = 4;
        timingConfig.tseg_1 = 16;
        timingConfig.tseg_2 = 8;
        timingConfig.sjw = 3;
        timingConfig.triple_sampling = false;
        break;
    case 1000:
        //Set Baudrate to 1Mbit;
        timingConfig.brp = 4;
        timingConfig.tseg_1 = 15;
        timingConfig.tseg_2 = 4;
        timingConfig.sjw = 3;
        timingConfig.triple_sampling = false;
        break;
    default:
        ESP_LOGE("CO_driver", "%d => Invalid Baudrate! Using 1Mbit as default!", CANbitRate);
        //Set Baudrate to 1Mbit;
        timingConfig.brp = 4;
        timingConfig.tseg_1 = 15;
        timingConfig.tseg_2 = 4;
        timingConfig.sjw = 3;
        timingConfig.triple_sampling = false;
    }

    /* Configure CAN module hardware filters */
    if (CANmodule->useCANrxFilters)
    {
        /* CAN module filters are used, they will be configured with */
        /* CO_CANrxBufferInit() functions, called by separate CANopen */
        /* init functions. */
        /* Configure all masks so, that received message must match filter */

        //Don't filter messages
        ESP_LOGI("CO_driver", "RxFilters are active, but no filter configured.");
        filterConfig.acceptance_code = 0;
        filterConfig.acceptance_mask = 0xFFFFFFFF;
        filterConfig.single_filter = true;
    }
    else
    {
        /* CAN module filters are not used, all messages with standard 11-bit */
        /* identifier will be received */
        /* Configure mask 0 so, that all messages with standard identifier are accepted */
        //Don't filter messages
        filterConfig.acceptance_code = 0;
        filterConfig.acceptance_mask = 0xFFFFFFFF;
        filterConfig.single_filter = true;
    }
    return CO_ERROR_NO;
}

/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    /* turn off the module */
    ESP_ERROR_CHECK(twai_stop());
}

/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
    CO_CANmodule_t *CANmodule,
    uint16_t index,
    uint16_t ident,
    uint16_t mask,
    bool_t rtr,
    void *object,
    void (*CANrx_callback)(void *object, void *message))
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    if ((CANmodule != NULL) && (object != NULL) && (CANrx_callback != NULL) && (index < CANmodule->rxSize))
    {
        /* buffer, which will be configured */
        CO_CANrx_t *buffer = &CANmodule->rxArray[index];

        /* Configure object variables */
        buffer->object = object;
        buffer->CANrx_callback = CANrx_callback;

        /* CAN identifier and CAN mask, bit aligned with CAN module. Different on different microcontrollers. */
        buffer->ident = ident & 0x07FFU;
        if (rtr)
        {
            buffer->ident |= 0x0800U;
        }
        buffer->mask = (mask & 0x07FFU) | 0x0800U;

        /* Set CAN hardware module filter and mask. */
        if (CANmodule->useCANrxFilters)
        {
            ESP_LOGE("CO_CANrxBufferInit", "No driver implementation for Filters");
        }
    }
    else
    {
        ESP_LOGE("CO_CANrxBufferInit", "((CANmodule!=NULL) && (object!=NULL) && (pFunct!=NULL) && (index < CANmodule->rxSize))==FALSE");
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}

/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit(
    CO_CANmodule_t *CANmodule,
    uint16_t index,
    uint16_t ident,
    bool_t rtr,
    uint8_t noOfBytes,
    bool_t syncFlag)
{
    CO_CANtx_t *buffer = NULL;

    if ((CANmodule != NULL) && (index < CANmodule->txSize))
    {
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

        /* CAN identifier, DLC and rtr, bit aligned with CAN module transmit buffer. */
        /* Convert data from library message into esp message values */
        buffer->ident = ((uint32_t)ident & 0x07FFU); /* Set Message ID (Standard frame), delete other informations */
        buffer->rtr = rtr;                           /* Set RTR Flag */
        buffer->DLC = noOfBytes;                     /* Set number of bytes */
        buffer->bufferFull = false;                  /* Set buffer full flag */
        buffer->syncFlag = syncFlag;                 /* Set sync flag */
    }
    else
    {
        ESP_LOGE("CO_CANtxBufferInit", "CANmodule not initialized or index out of bound of txSize");
    }
    return buffer;
}

/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t err = CO_ERROR_NO;
    twai_status_info_t esp_can_hw_status;                      /* Define variable for hardware status of esp can interface */
    ESP_ERROR_CHECK(twai_get_status_info(&esp_can_hw_status)); /* Get hardware status of esp can interface */

    /* Verify overflow */
    if (buffer->bufferFull)
    {
        err = CO_ERROR_TX_OVERFLOW;
        ESP_LOGE("CO_CANsend", "CAN-tx-buffer overflow 1");
        if (!CANmodule->firstCANtxMessage)
        {
            /* don't set error, if bootup message is still on buffers */
            // TODO - CO_errorReport((CO_EM_t *)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, buffer->ident);
            // TODO - Temporary fix for buffer overflow if LinMot loses power
            esp_restart();
        }
    }

    CO_LOCK_CAN_SEND();
    /* if CAN TX buffer is free, copy message to it */
    if ((esp_can_hw_status.msgs_to_tx < CAN_TX_QUEUE_LENGTH) && (CANmodule->CANtxCount == 0))
    {
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        twai_message_t temp_can_message; /* generate esp can message for transmission */
        /*MESSAGE MIT DATEN FÜLLEN*/
        temp_can_message.identifier = buffer->ident;     /* Set message-id in esp can message */
        temp_can_message.data_length_code = buffer->DLC; /* Set data length in esp can message */
        temp_can_message.flags = TWAI_MSG_FLAG_NONE;      /* reset all flags in esp can message */
        //temp_can_message.flags = TWAI_MSG_FLAG_SELF;
        if (buffer->rtr)
        {
            temp_can_message.flags |= TWAI_MSG_FLAG_RTR; /* set rtr-flag if needed */
        }
        for (uint8_t i = 0; i < buffer->DLC; i++)
        {
            temp_can_message.data[i] = buffer->data[i]; /* copy data from buffer in esp can message */
        }

        /* Transmit esp can message.  */
        esp_err_t send_err = twai_transmit(&temp_can_message, pdMS_TO_TICKS(CAN_MS_TO_WAIT));
        if (send_err == ESP_OK)
        {
            // TODO - Filter log system 
            // ESP_LOGI("CANsend", "ID: %d , Data %d,%d,%d,%d,%d,%d", temp_can_message.identifier, temp_can_message.data[0], temp_can_message.data[1], temp_can_message.data[2], temp_can_message.data[3], temp_can_message.data[4], temp_can_message.data[5]);
        }
        else
        {
            err = CO_ERROR_TIMEOUT;
            ESP_LOGE("CO_CANsend", "Failed to queue message for transmission - %u", send_err);
        }
    }
    /* if no buffer is free, message will be sent by interrupt */
    else
    {
        ESP_LOGE("CO_CANsend", "CAN-tx-buffer overflow 2 - %u - %u", esp_can_hw_status.msgs_to_tx, CANmodule->CANtxCount);
        // TODO - Fix this and have logic for bufferFull to be set to false
        //buffer->bufferFull = true;
        //CANmodule->CANtxCount++;
    }
    CO_UNLOCK_CAN_SEND();

    return err;
}

/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    //TODO: Implement this function
    ESP_LOGE("CO_CANclearPendingSyncPDOs", "This function is NOT implemented!");
    uint32_t tpdoDeleted = 0U;

    // CO_LOCK_CAN_SEND();
    // /* Abort message from CAN module, if there is synchronous TPDO.
    //  * Take special care with this functionality. */
    // if(/*messageIsOnCanBuffer && */CANmodule->bufferInhibitFlag){
    //     /* clear TXREQ */
    //     CANmodule->bufferInhibitFlag = false;
    //     tpdoDeleted = 1U;
    // }
    // /* delete also pending synchronous TPDOs in TX buffers */
    // if(CANmodule->CANtxCount != 0U){
    //     uint16_t i;
    //     CO_CANtx_t *buffer = &CANmodule->txArray[0];
    //     for(i = CANmodule->txSize; i > 0U; i--){
    //         if(buffer->bufferFull){
    //             if(buffer->syncFlag){
    //                 buffer->bufferFull = false;
    //                 CANmodule->CANtxCount--;
    //                 tpdoDeleted = 2U;
    //             }
    //         }
    //         buffer++;
    //     }
    // }
    // CO_UNLOCK_CAN_SEND();

    if (tpdoDeleted != 0U)
    {
        // TODO - CO_errorReport((CO_EM_t *)CANmodule->em, CO_EM_TPDO_OUTSIDE_WINDOW, CO_EMC_COMMUNICATION, tpdoDeleted);
    }
}
//TODO: usefull error handling
/******************************************************************************/
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule)
{

    uint16_t rxErrors, txErrors, overflow;
    // TODO - CO_EM_t *em = (CO_EM_t *)CANmodule->em;
    uint32_t err;

    /* get error counters from module. Id possible, function may use different way to
     * determine errors. */
    rxErrors = 0;
    txErrors = 0;
    overflow = 0;
    // rxErrors = CANmodule->txSize;
    // txErrors = CANmodule->txSize;
    // overflow = CANmodule->txSize;

    err = ((uint32_t)txErrors << 16) | ((uint32_t)rxErrors << 8) | overflow;

    if (CANmodule->errOld != err)
    {
        CANmodule->errOld = err;

        if (txErrors >= 256U)
        { /* bus off */
            ESP_LOGE("CO_CANverifyErrors", "can bus off");
            // TODO - CO_errorReport(em, CO_EM_CAN_TX_BUS_OFF, CO_EMC_BUS_OFF_RECOVERED, err);
        }
        else
        { /* not bus off */
            ESP_LOGI("CO_CANverifyErrors", "reset bus off");
            // TODO - CO_errorReset(em, CO_EM_CAN_TX_BUS_OFF, err);

            if ((rxErrors >= 96U) || (txErrors >= 96U))
            { /* bus warning */
                ESP_LOGE("CO_CANverifyErrors", "bus warning");
                // TODO - CO_errorReport(em, CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, err);
            }

            if (rxErrors >= 128U)
            { /* RX bus passive */
                ESP_LOGE("CO_CANverifyErrors", "RX bus passive");
                // TODO - CO_errorReport(em, CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
            }
            else
            {
                ESP_LOGE("CO_CANverifyErrors", "reset RX bus passive");
                // TODO - CO_errorReset(em, CO_EM_CAN_RX_BUS_PASSIVE, err);
            }

            if (txErrors >= 128U)
            { /* TX bus passive */
                if (!CANmodule->firstCANtxMessage)
                {
                    ESP_LOGE("CO_CANverifyErrors", "TX bus passive");
                    // TODO - CO_errorReport(em, CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
                }
            }
            else
            {
                // TODO - bool_t isError = CO_isError(em, CO_EM_CAN_TX_BUS_PASSIVE);
                // TODO - if (isError)
                // TODO - {
                    ESP_LOGE("CO_CANverifyErrors", "reset TX bus passive");
                    // TODO - CO_errorReset(em, CO_EM_CAN_TX_BUS_PASSIVE, err);
                    // TODO - CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
                // TODO - }
            }

            if ((rxErrors < 96U) && (txErrors < 96U))
            { /* no error */
                ESP_LOGE("CO_CANverifyErrors", "no error");
                // TODO - CO_errorReset(em, CO_EM_CAN_BUS_WARNING, err);
            }
        }

        if (overflow != 0U)
        { /* CAN RX bus overflow */
            ESP_LOGE("CO_CANverifyErrors", "CAN RX bus overflow oO");
            // TODO - CO_errorReport(em, CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, err);
        }
    }
}

/******************************************************************************/
void CO_CANinterrupt(void *args)
{
    CO_CANmodule_t *CANmodule = CANmodulePointer;
    twai_status_info_t esp_can_hw_status;                      /* Define variable for hardware status of esp can interface */
    ESP_ERROR_CHECK(twai_get_status_info(&esp_can_hw_status)); /* Get hardware status of esp can interface */
    /* receive interrupt */
    if (esp_can_hw_status.msgs_to_rx != 0)
    {
        twai_message_t temp_can_message; //ESP data type can message
        ESP_ERROR_CHECK(twai_receive(&temp_can_message, pdMS_TO_TICKS(CAN_MS_TO_WAIT)));

/*
        ESP_LOGI(
          "CANreceive", 
          "ID: %d , Data %d,%d,%d,%d,%d,%d", 
          temp_can_message.identifier, 
          temp_can_message.data[0], 
          temp_can_message.data[1], 
          temp_can_message.data[2], 
          temp_can_message.data[3], 
          temp_can_message.data[4], 
          temp_can_message.data[5]
        );
*/

        CO_CANrxMsg_t rcvMsg;      /* pointer to received message in CAN module */
        uint16_t index;            /* index of received message */
        uint32_t rcvMsgIdent;      /* identifier of the received message */
        CO_CANrx_t *buffer = NULL; /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;

        rcvMsg.ident = temp_can_message.identifier;
        /* check if rtr flag is set in esp can message*/
        if (temp_can_message.flags & TWAI_MSG_FLAG_RTR)
        {
            rcvMsg.ident += (1 << 12); /* Set RTR flag in library message */
        }
        rcvMsg.dlc = temp_can_message.data_length_code; /* Set data length in library message */
        for (uint8_t i = 0; i < temp_can_message.data_length_code; i++)
        {
            rcvMsg.data[i] = temp_can_message.data[i]; /* copy data from esp can message to library message */
        }

        rcvMsgIdent = rcvMsg.ident;
        if (CANmodule->useCANrxFilters)
        {
            ESP_LOGE("CO_CANinterrupt", "Filter system is not implemented");
            /* CAN module filters are used. Message with known 11-bit identifier has */
            /* been received */
            index = 0; /* get index of the received message here. Or something similar */
            if (index < CANmodule->rxSize)
            {
                buffer = &CANmodule->rxArray[index];
                /* verify also RTR */
                if (((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U)
                {
                    msgMatched = true;
                }
            }
        }
        else
        {
            /* CAN module filters are not used, message with any standard 11-bit identifier */
            /* has been received. Search rxArray form CANmodule for the same CAN-ID. */
            buffer = &CANmodule->rxArray[0];
            for (index = CANmodule->rxSize; index > 0U; index--)
            {
                if (((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U)
                {
                    msgMatched = true;
                    break;
                }
                buffer++;
            }
        }
        /* Call specific function, which will process the message */
        if (msgMatched && (buffer != NULL) && (buffer->CANrx_callback != NULL))
        {
            buffer->CANrx_callback(buffer->object, &rcvMsg);
        }
    }
}

/******************************************************************************/
void CO_CANmodule_process(CO_CANmodule_t *CANmodule) {
  // TODO - Need to fill this in
}