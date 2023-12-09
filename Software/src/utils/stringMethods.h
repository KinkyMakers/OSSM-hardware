#ifndef DT_TRAINER_STRINGMETHODS_H
#define DT_TRAINER_STRINGMETHODS_H

#include <Arduino.h>

static bool isStringEmpty(const String &str) {
    for (unsigned int i = 0; i < str.length(); i++) {
        // Check if the character is not a whitespace character or newline or
        // return or form feed
        if (!isWhitespace(str.charAt(i)) && str.charAt(i) != '\n' &&
            str.charAt(i) != '\r' && str.charAt(i) != '\f' &&
            str.charAt(i) != '\v') {
            return false;  // Found a non-whitespace character
        }
    }
    return true;  // Only whitespace characters found
}

static String wrapText(const String &input) {
    String output;
    int lineLength = 0;

    for (unsigned int i = 0; i < input.length(); ++i) {
        output += input[i];
        lineLength++;

        if (lineLength == 12) {
            if (i + 1 < input.length() && input[i + 1] == ' ') {
                output += '\n';
                i++;  // Skip the space as we've replaced it with a newline
            } else {
                // Find the nearest previous space to replace with a newline
                int spaceIndex = output.lastIndexOf(' ', output.length() - 1);
                if (spaceIndex != -1 && spaceIndex >= output.length() - 12) {
                    output.setCharAt(spaceIndex, '\n');
                    lineLength = output.length() - spaceIndex - 1;
                } else {
                    // No space found or the word is too long, insert a newline
                    // directly
                    output += '\n';
                }
            }
            lineLength = 0;
        }
    }

    return output;
}

#endif  // DT_TRAINER_STRINGMETHODS_H
