#ifndef OSSM_SOFTWARE_FORMAT_H
#define OSSM_SOFTWARE_FORMAT_H

#include <Arduino.h>

#include "constants/UserConfig.h"

String formatTime(unsigned int totalSeconds) {
    String formattedTime = "";

    // Calculate time components
    unsigned int days = totalSeconds / 86400;
    unsigned int hours = (totalSeconds % 86400) / 3600;
    unsigned int minutes = (totalSeconds % 3600) / 60;
    unsigned int seconds = totalSeconds % 60;

    // Construct formatted time string
    if (days > 0) {
        formattedTime += String(days) + "d";
        // If the number of days is mentioned, we don't print the rest unless
        // there are also hours.
        if (hours > 0) {
            formattedTime += " " + String(hours) + "h";
        }
    } else if (hours > 0) {
        formattedTime += String(hours) + "h";
        if (minutes > 0 || seconds > 0) {
            formattedTime += " ";
        }
    }

    if (days == 0 && (minutes > 0 || (hours > 0 && seconds > 0))) {
        if (hours > 0) {  // If there are hours, we want to print minutes even
                          // if they're zero.
            formattedTime += String(minutes / 10) +
                             String(minutes % 10);  // Two digits for minutes
        } else {
            formattedTime += String(
                minutes);  // No need for a leading zero if there are no hours.
        }
        if (seconds > 0 || hours > 0) {  // If there are hours, we print seconds
                                         // even if they're zero.
            formattedTime += ":";
            formattedTime += String(seconds / 10) +
                             String(seconds % 10);  // Two digits for seconds
        }
    } else if (days == 0 && hours == 0 && minutes == 0) {
        formattedTime += String(seconds) + "s";
    }

    return formattedTime;
}

String formatImperial(double meters) {
    // Convert meters to feet
    float feet = meters * 3.28084f;

    if (feet < 1.0f) {
        // Convert feet to inches and format
        int inches = roundf(feet * 12);
        return String(inches) + " in";
    } else if (feet < 5280.0f) {  // Less than a mile
        // Format to no decimal places for feet if less than a mile
        return String(roundf(feet)) + " ft";
    } else {
        // Convert feet to miles and format
        return String(feet / 5280.0f, 2) + " mi";
    }
}

String formatMetric(double meters) {
    if (meters < 1.0f) {
        // Convert to centimeters and format
        return String(meters * 100.0f, 1) + " cm";
    } else if (meters < 100.0f) {
        // Keep as meters, but format
        return String(meters, 1) + " m";
    } else if (meters < 1000.0f) {
        // Round to nearest meter
        return String(roundf(meters)) + " m";
    } else {
        // Convert to kilometers and format
        return String(meters / 1000.0f, 2) + " km";
    }
}

String formatDistance(double meters) {
    if (UserConfig::displayMetric) {
        return formatMetric(meters);
    } else {
        return formatImperial(meters);
    }
}

#endif  // OSSM_SOFTWARE_FORMAT_H
