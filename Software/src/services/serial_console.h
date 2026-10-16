#pragma once

// USB settings console on the existing 115200 baud Serial port.
// Starts after the state machine exists. Hardware tests that supply their
// own setup() do not start it, so Unity keeps the port.
void startSerialConsole();
