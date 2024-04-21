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
        formattedTime += String(days) + "d ";
    }

    if (hours > 0 || (days > 0 && (minutes > 0 || seconds > 0))) {
        if (hours > 0 || days > 0) {
            formattedTime += String(hours) + "h ";
        }
    }

    // Format minutes and seconds
    if (days == 0 && hours == 0) {
        // Format as MM:SS when there are no hours and days
        formattedTime += (minutes < 10 ? "0" : "") + String(minutes) + ":";
        formattedTime += (seconds < 10 ? "0" : "") + String(seconds);
    } else if (minutes > 0 || seconds > 0) {
        // Otherwise, just append minutes and seconds normally
        formattedTime += String(minutes) + ":";
        formattedTime += (seconds < 10 ? "0" : "") + String(seconds);
    }

    // Handle the special case of only seconds less than 60 in the total time
    if (totalSeconds < 60) {
        formattedTime = String(seconds) + "s";
    }

    // Remove any trailing spaces
    if (formattedTime.endsWith(" ")) {
        formattedTime = formattedTime.substring(0, formattedTime.length() - 1);
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
    String sign = meters >= 0 ? "" : "-";
    meters = abs(meters);

    if (meters == 0) {
        return "0.0 cm";
    } else if (meters < 1.0f) {
        // Convert to centimeters and format
        return sign + String(meters * 100.0f, 1) + " cm";
    } else if (meters < 100.0f) {
        // Keep as meters, but format
        return sign + String(meters, 1) + " m";
    } else if (meters < 1000.0f) {
        // Round to nearest meter
        return sign + String(roundf(meters)) + " m";
    } else {
        // Convert to kilometers and format
        return sign + String(meters / 1000.0f, 2) + " km";
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
