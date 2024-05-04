#ifndef SOFTWARE_HTTPCLIENT_H
#define SOFTWARE_MOCK_H

#include <unity.h>

#include <queue>

// Mock data for analogRead
std::queue<int> mockReadings;

// Override the analogRead function
uint16_t analogRead(int pin) {
    if (!mockReadings.empty()) {
        int reading = mockReadings.front();
        mockReadings.pop();
        return reading;
    }
    return 0;  // Default to 0 if no mock data is available
}

// Function to add test data
void prepareAnalogReadData(const std::vector<int>& readings) {
    while (!mockReadings.empty()) {
        mockReadings.pop();  // Clear existing data
    }
    for (int reading : readings) {
        mockReadings.push(reading);
    }
}

#endif  // SOFTWARE_HTTPCLIENT_H
