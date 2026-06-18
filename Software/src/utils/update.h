#ifndef SOFTWARE_UPDATE_H
#define SOFTWARE_UPDATE_H

#include <cstdint>

// OTA configuration. Single source of truth shared with the webflasher: the
// Supabase "production" channel. The device fetches version.json, decides for
// itself whether a newer build exists, and downloads firmware.bin from the same
// place over validated HTTPS. To move hosts later (e.g. GitHub Pages), only
// OTA_BASE_URL changes.
//
// The actual check + download run in a dedicated FreeRTOS task (see
// ossmStartUpdate in update.cpp), because a TLS handshake needs far more stack
// than the button task — where the state machine runs — has available.
namespace OtaConfig {
static const char *OTA_BASE_URL =
    "https://acjajruwevyyatztbkdf.supabase.co/storage/v1/object/public/"
    "ossm-firmware/";
static const char *PRODUCTION_PATH = "production/";
static const char *VERSION_JSON = "version.json";
static const char *FIRMWARE_BIN = "firmware.bin";
}  // namespace OtaConfig

#endif  // SOFTWARE_UPDATE_H
