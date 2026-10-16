#ifndef OSSM_VERSION_H
#define OSSM_VERSION_H

#define VERSION "1.0.60"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define PATCH_VERSION 60

// Only staging and production define this in platformio.ini
#ifndef FIRMWARE_TRACK
#define FIRMWARE_TRACK "main"
#endif

#endif  // OSSM_VERSION_H
