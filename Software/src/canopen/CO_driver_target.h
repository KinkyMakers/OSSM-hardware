/*
 * Device and application specific definitions for CANopenNode.
 *
 * @file        CO_driver_target.h
 * @author      Janez Paternoster
 * @copyright   2021 Janez Paternoster
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
#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H

/* This file contains device and application specific definitions.
 * It is included from CO_driver.h, which contains documentation
 * for common definitions below. */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#undef CO_CONFIG_STORAGE_ENABLE  // In this port the storage functionality has not been implemented

#ifdef CO_DRIVER_CUSTOM
#include "CO_driver_custom.h"
#endif

#include "CO_config_target.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stack configuration override default values.
 * For more information see file CO_config.h. */

/* Basic definitions. If big endian, CO_SWAP_xx macros must swap bytes. */
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) x
#define CO_SWAP_32(x) x
#define CO_SWAP_64(x) x

/* NULL is defined in stddef.h */
/* true and false are defined in stdbool.h */
/* int8_t to uint64_t are defined in stdint.h */
typedef uint_fast8_t            bool_t;
typedef float                   float32_t;
typedef double                  float64_t;

/**
 * \brief           CAN RX message for platform
 *
 * This is platform specific one
 */
typedef struct {
    uint32_t ident;                             /*!< Standard identifier */
    uint8_t dlc;                                /*!< Data length */
    uint8_t data[8];                            /*!< Received data */
} CO_CANrxMsg_t;

/* Access to received CAN message */
//#define CO_CANrxMsg_readIdent(msg)          ((uint16_t)(((CO_CANrxMsg_t *)(msg)))->ident)
//#define CO_CANrxMsg_readDLC(msg)            ((uint8_t)(((CO_CANrxMsg_t *)(msg)))->dlc)
//#define CO_CANrxMsg_readData(msg)           ((uint8_t *)(((CO_CANrxMsg_t *)(msg)))->data)

static inline uint16_t CO_CANrxMsg_readIdent(void *rxMsg) {
    return (uint16_t)((CO_CANrxMsg_t*)rxMsg)->ident;
}

static inline uint8_t CO_CANrxMsg_readDLC(void *rxMsg) {
    return (uint16_t)((CO_CANrxMsg_t*)rxMsg)->dlc;
}

static inline uint8_t *CO_CANrxMsg_readData(void *rxMsg) {
    return (uint8_t *)((CO_CANrxMsg_t*)rxMsg)->data;
}

/* Received message object */
typedef struct {
    uint16_t ident;
    uint16_t mask;
    void *object;
    void (*CANrx_callback)(void *object, void *message);
} CO_CANrx_t;

/* Transmit message object */
typedef struct {
    uint32_t ident;
    uint32_t mask;           //Add MASK-Flag for RTR-bit
    bool rtr;            //RTR-Flag
    uint8_t DLC;
    uint8_t data[8];
    volatile bool_t bufferFull;
    volatile bool_t syncFlag;
} CO_CANtx_t;

/* CAN module object */
typedef struct {
	 void *CANptr;
    CO_CANrx_t *rxArray;
    uint16_t rxSize;
    CO_CANtx_t *txArray;
    uint16_t txSize;
    uint16_t CANerrorStatus;
    volatile bool_t CANnormal;
    volatile bool_t useCANrxFilters;
    volatile bool_t bufferInhibitFlag;
    volatile bool_t firstCANtxMessage;
    volatile uint16_t CANtxCount;
    uint32_t errOld;

    /* ESP32 specific features */
    
} CO_CANmodule_t;

/* Data storage object for one entry */
typedef struct {
    void *addr;
    size_t len;
    uint8_t subIndexOD;
    uint8_t attr;
    /* Additional variables (target specific) */
    void *addrNV;
} CO_storage_entry_t;

/* (un)lock critical section in CO_CANsend() */
#define CO_LOCK_CAN_SEND(CAN_MODULE)            
#define CO_UNLOCK_CAN_SEND(CAN_MODULE)          

/* (un)lock critical section in CO_errorReport() or CO_errorReset() */
#define CO_LOCK_EMCY(CAN_MODULE)                
#define CO_UNLOCK_EMCY(CAN_MODULE)              

/* (un)lock critical section when accessing Object Dictionary */
#define CO_LOCK_OD(CAN_MODULE)                  
#define CO_UNLOCK_OD(CAN_MODULE)                

/* Synchronization between CAN receive and message processing threads. */
#define CO_MemoryBarrier()
#define CO_FLAG_READ(rxNew)                     ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew)                      do { CO_MemoryBarrier(); rxNew = (void*)1L; } while (0)
#define CO_FLAG_CLEAR(rxNew)                    do { CO_MemoryBarrier(); rxNew = NULL; } while (0)

/*
 * Use custom library for allocation of core CanOpenNode objects
 *
 * LwMEM is optimized for embedded systems
 * and supports operating systems.
 *
 * When OS feature is enabled, LWMEM_CFG_OS is defined in compiler settings.
 */
//#include "lwmem/lwmem.h"
#if defined(CO_USE_GLOBALS)
#undef CO_USE_GLOBALS
#endif
//#define CO_alloc(num, size)             lwmem_calloc((num), (size))
//#define CO_free(ptr)                    lwmem_free((ptr))

/*
 * Enable TIMERNEXT feature
 *
 * This features allows CANopen application threads,
 * to sleep for known time interval before next processing should occur
 */
#define CO_CONFIG_GLOBAL_FLAG_TIMERNEXT         CO_CONFIG_FLAG_TIMERNEXT

/* External FDCAN handle object */
#if defined(STM32H7xx)
extern FDCAN_HandleTypeDef hfdcan1;         /* Global FDCAN instance for HAL */
#endif /* defined(STM32H7xx) */

/*
 * Operating system use case.
 *
 * When OS feature is enabled, USE_OS option is defined in compiler settings.
 */
#if defined(USE_OS)
#include "cmsis_os2.h"

/* Functions to lock access to shared services with mutex */
uint8_t co_drv_create_os_objects(void);
uint8_t co_drv_mutex_lock(void);
uint8_t co_drv_mutex_unlock(void);

/* Semaphore for main app thread synchronization */
extern osSemaphoreId_t co_drv_app_thread_sync_semaphore;

/* Semaphore for periodic thread synchronization */
extern osSemaphoreId_t co_drv_periodic_thread_sync_semaphore;

/* Wakeup specific threads */
#define CO_WAKEUP_APP_THREAD()                          osSemaphoreRelease(co_drv_app_thread_sync_semaphore)
#define CO_WAKEUP_PERIODIC_THREAD()                     osSemaphoreRelease(co_drv_periodic_thread_sync_semaphore)
#define CO_WAIT_SYNC_APP_THREAD(max_time_in_ms)         osSemaphoreAcquire(co_drv_app_thread_sync_semaphore, (max_time_in_ms))
#define CO_WAIT_SYNC_PERIODIC_THREAD(max_time_in_ms)    osSemaphoreAcquire(co_drv_periodic_thread_sync_semaphore, (max_time_in_ms))

#else /* defined(USE_OS) */

/* Empty definitions for non-OS implementation */
#define CO_WAKEUP_APP_THREAD()
#define CO_WAKEUP_PERIODIC_THREAD()

#endif /* !defined(USE_OS) */

void CO_CANinterrupt(void *args);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DRIVER_TARGET_H */
