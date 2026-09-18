#ifndef OSSM_STATE_NETWORK_H
#define OSSM_STATE_NETWORK_H

#include <Arduino.h>

/**
 * Outcome of the last network job (update check, pairing) so it can be
 * shown on the display and reported over the BLE state characteristic.
 *
 * error codes: "wifi" | "low-memory" | "check-failed" | "install-failed" |
 *              "pairing-failed"
 */
struct NetworkStatus {
    String error;
    String pairingCode;
    bool isPaired = false;
    // Version the resolver offered; set while waiting in update.available.
    String targetVersion;

    void clearError() { error = ""; }
    void clearUpdate() { targetVersion = ""; }
    void clearPairing() {
        pairingCode = "";
        isPaired = false;
    }
};

extern NetworkStatus networkStatus;

#endif  // OSSM_STATE_NETWORK_H
