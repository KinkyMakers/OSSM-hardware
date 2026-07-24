#ifndef SOFTWARE_UPDATE_H
#define SOFTWARE_UPDATE_H

// Starts the database-resolved firmware check in its dedicated TLS task.
void ossmStartUpdate();

// Confirms a newly booted OTA image only when ESP-IDF reports it as pending
// verification. This is a no-op on existing rollback-disabled bootloaders.
void ossmConfirmRunningImage();

#endif  // SOFTWARE_UPDATE_H
