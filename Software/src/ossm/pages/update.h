#ifndef OSSM_PAGES_UPDATE_H
#define OSSM_PAGES_UPDATE_H

#include <Arduino.h>

namespace pages {

/**
 * Draw the "checking for update" page
 */
void drawUpdate();

/**
 * Draw the "no update available" page
 */
void drawNoUpdate();

/**
 * Draw the "updating" page
 */
void drawUpdating();

/**
 * Draw the "update failed" page with the reason from networkStatus.error
 */
void drawUpdateFailed();

/**
 * Draw the "update available, press to install" page (subtitle = version)
 */
void drawUpdateAvailable();

/**
 * Human-readable reason for a NetworkStatus::error code
 */
const char* networkErrorReason(const String& code);

}  // namespace pages

#endif  // OSSM_PAGES_UPDATE_H
