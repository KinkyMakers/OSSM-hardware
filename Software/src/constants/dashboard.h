#ifndef SOFTWARE_DASHBOARD_H
#define SOFTWARE_DASHBOARD_H

#ifndef MQTT_BROKER
#define MQTT_BROKER "192.168.0.12"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#ifndef RAD_SERVER
#define RAD_SERVER "http://192.168.0.12:3000"
#endif

#define RAD_AUTH String(RAD_SERVER) + "/api/ossm/auth"

#endif  // SOFTWARE_DASHBOARD_H
