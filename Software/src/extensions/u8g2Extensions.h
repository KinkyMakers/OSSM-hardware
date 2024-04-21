#ifndef OSSM_SOFTWARE_U8G2EXTENSIONS_H
#define OSSM_SOFTWARE_U8G2EXTENSIONS_H

#include <utility>

#include "constants/Config.h"
#include "services/display.h"

// NOLINTBEGIN(hicpp-signed-bitwise)
static int getUTF8CharLength(const unsigned char c) {
    if ((c & 0x80) == 0)
        return 1;  // ASCII
    else if ((c & 0xE0) == 0xC0)
        return 2;  // 2-byte UTF8
    else if ((c & 0xF0) == 0xE0)
        return 3;  // 3-byte UTF8
    else if ((c & 0xF8) == 0xF0)
        return 4;  // 4-byte UTF8
    return 1;      // default to 1 byte for unexpected scenarios
}
// NOLINTEND(hicpp-signed-bitwise)

/**
 * This namespace contains functions that extend the functionality of the
 * U8G2 library.
 *
 * Specifically, it contains functions that are used to draw text on the
 * display.
 */
namespace drawStr {
    static void centered(int y, String str) {
        // Convert the String object to a UTF-8 string.
        // The c_str() function ensures we're passing a null-terminated string,
        // which is required by getStrWidth().
        const char *utf8Str = str.c_str();

        // Calculate the X position where the text should start in order to be
        // centered.
        int x = (display.getDisplayWidth() - display.getUTF8Width(utf8Str)) / 2;

        // Draw the string at the calculated position.
        display.drawUTF8(x, y, utf8Str);
    }

    /**
     * u8g2E a string, breaking it into multiple lines if necessary.
     * This will attempt to break at "/n" characters, or at spaces if no
     * newlines are handlePress.
     * @param x
     * @param y
     * @param str
     */
    static void multiLine(int x, int y, String string, int lineHeight = 12) {
        const char *str = string.c_str();
        // Set the font for the text to be displayed.
        display.setFont(Config::Font::base);

        // Retrieve the width of the display.
        int displayWidth = display.getDisplayWidth();

        // Initialize a variable to keep track of the width of the string being
        // processed.
        int currentLineWidth = 0;

        // Buffer to store individual characters (glyphs) from the string.
        // A single UTF-8 character can be up to 4 bytes in length, +1 for the
        // null terminator.
        char glyph[5] = {0};

        // Pointers to keep track of the current position in the string and the
        // last space character.
        const char *currentChar = str;
        const char *lastSpace = nullptr;

        // Loop through each character in the string.
        while (*currentChar) {
            // Skip any leading spaces or newline characters at the beginning of
            // each line.
            while (x == 0 && (*str == ' ' || *str == '\n')) {
                if (currentChar == str++) ++currentChar;
            }

            // Determine the length of the current UTF-8 character.
            int charLength = getUTF8CharLength(*currentChar);

            // Copy the character to the glyph buffer.
            strncpy(glyph, currentChar, charLength);

            // Null-terminate the copied text.
            glyph[charLength] = 0;

            // Advance the current character pointer by the length of the
            // character just processed.
            currentChar += charLength;

            // Add the width of the current glyph to the current line width.
            currentLineWidth += display.getStrWidth(glyph);

            // Check for space character; if found, remember its position.
            if (*glyph == ' ') lastSpace = currentChar - charLength;

            // If not a space, increment the width (for the space between
            // characters).
            else
                ++currentLineWidth;

            // Check for a new line character or if the text exceeds the display
            // width.
            if (*glyph == '\n' || x + currentLineWidth > displayWidth) {
                int startingPosition =
                    x;  // Remember the starting horizontal position.

                // Display characters up to the last space (or current position
                // if no space).
                while (str < (lastSpace ? lastSpace : currentChar)) {
                    // Get the length of the next character to display.
                    charLength = getUTF8CharLength(*str);
                    // Copy the character to the glyph buffer.
                    strncpy(glyph, str, charLength);
                    glyph[charLength] = 0;  // Null-terminate the copied text.

                    // Draw the character on the display and advance the x
                    // position.
                    x += display.drawUTF8(x, y, glyph);

                    // Advance the main string pointer.
                    str += charLength;
                }

                // Adjust the current line width for the new line.
                currentLineWidth -= x - startingPosition;

                // Move to the next line on the display.
                y += lineHeight;

                // Reset the horizontal position for the new line.
                x = 0;

                // Reset the last space pointer for the new line.
                lastSpace = nullptr;
            }
        }

        // Process any remaining characters in the string.
        while (*str) {
            // Get the length of the next character to display.
            int charLength = getUTF8CharLength(*str);

            // Copy the character to the glyph buffer.
            strncpy(glyph, str, charLength);
            glyph[charLength] = 0;  // Null-terminate the copied text.

            // Draw the character on the display and advance the x position.
            x += display.drawUTF8(x, y, glyph);

            // Advance the main string pointer.
            str += charLength;
        }
    }

    static void title(String str) {
        display.setFont(Config::Font::bold);
        centered(8, std::move(str));
    }
};

namespace drawShape {
    static void scroll(long position) {
        int topMargin = 10;  // Margin at the top of the screen

        int scrollbarHeight = 64 - topMargin;  // Height of the scrollbar
        int scrollbarWidth = 3;                // Width of the scrollbar
        int scrollbarX =
            125;  // X position of the scrollbar (right edge of the screen)
        int scrollbarY = (64 - scrollbarHeight + topMargin) /
                         2;  // Y position of the scrollbar (centered)

        // Draw the dotted line
        for (int i = 0; i < scrollbarHeight; i += 4) {
            display.drawHLine(scrollbarX + 1, scrollbarY + i, 1);
        }

        // Calculate the Y position of the rectangle based on the current
        // position
        int rectY =
            scrollbarY + (scrollbarHeight - scrollbarWidth) * position / 100;

        // Draw the rectangle to represent the current position
        display.drawBox(scrollbarX, rectY, scrollbarWidth, scrollbarWidth);
    };

}

#endif  // OSSM_SOFTWARE_U8G2EXTENSIONS_H
