#ifndef OSSM_SIMPLE_PENETRATION_SIMPLE_PENETRATION_H
#define OSSM_SIMPLE_PENETRATION_SIMPLE_PENETRATION_H

namespace simple_penetration {

/**
 * Start the simple penetration motion task
 * Basic back-and-forth motion at controlled speed
 */
void startSimplePenetration();

/**
 * Start the streaming motion task
 * Receives position/time commands over BLE and follows them
 */
void startStreaming();

}  // namespace simple_penetration

#endif  // OSSM_SIMPLE_PENETRATION_SIMPLE_PENETRATION_H
