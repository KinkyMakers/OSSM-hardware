#ifndef SOFTWARE_RECUSIVEMUTEX_H
#define SOFTWARE_RECUSIVEMUTEX_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"

#include <boost/sml.hpp>

/**
 * @brief ESP32RecursiveMutex class provides a recursive mutex functionality
 * using FreeRTOS primitives. It mimics the behavior of std::recursive_mutex.
 *
 * This is primarily used to make the OSSMState machine thread safe.
 */
class ESP32RecursiveMutex {
  public:
    // Constructor: Creates a new recursive mutex
    ESP32RecursiveMutex() { mutex = xSemaphoreCreateRecursiveMutex(); }

    // Destructor: Deletes the recursive mutex
    ~ESP32RecursiveMutex() { vSemaphoreDelete(mutex); }

    // Locks the mutex. If the mutex is already locked by the same task,
    // the function will return immediately instead of blocking.
    void lock() { xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }

    // Unlocks the mutex.
    void unlock() { xSemaphoreGiveRecursive(mutex); }

    // Tries to lock the mutex. If the mutex is not available, the function
    // will return immediately with 'false'. If the mutex is available,
    // the function will lock the mutex and return 'true'.
    bool try_lock() { return xSemaphoreTakeRecursive(mutex, 0) == pdTRUE; }

    // Copy constructor and copy assignment operator are deleted
    // to prevent copying of ESP32RecursiveMutex objects.
    ESP32RecursiveMutex(const ESP32RecursiveMutex&) = delete;
    ESP32RecursiveMutex& operator=(const ESP32RecursiveMutex&) = delete;

  private:
    // Handle for the recursive mutex
    SemaphoreHandle_t mutex;
};
#endif  // SOFTWARE_RECUSIVEMUTEX_H