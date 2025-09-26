# Services

Services are global singleton instances of classes that provide functionality to peripherals in the system. They are designed to be shared resources that can be accessed by different parts of the application.

## Key Characteristics

-   Services are implemented as global singleton instances
-   Typically, there is only one instance of each service running at any time
-   By default, services are not thread-safe and require proper synchronization
-   Services should be accessed using semaphores to ensure thread safety

## Creating a New Service

### Header File (ServiceName.h)

```cpp
#ifndef SERVICE_NAME_H
#define SERVICE_NAME_H

#include <Arduino.h>
#include "XYZ.h"

// Declare the global service instance, but do not initialize it
extern XYZ serviceNameInstance;

// Optional: Add service configuration or constants
#define SERVICE_NAME_MAX_CONNECTIONS 5

#endif // SERVICE_NAME_H
```

### Implementation File (ServiceName.cpp)

```cpp
#include "ServiceName.h"

// Initialize the global service instance
XYZ serviceNameInstance();

// Optional: Add initialization code in setup()
void setup() {
    serviceNameInstance.begin();
}
```
