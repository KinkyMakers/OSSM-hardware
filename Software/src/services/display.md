# Display Service Technical Documentation

## Overview

The display service provides a thread-safe interface for drawing to a 128x64 pixel SSD1306 OLED display using the U8G2 library. The display is implemented as a global singleton that can be accessed from anywhere in the codebase.

## Hardware Configuration

-   **Display Type**: SSD1306 OLED
-   **Resolution**: 128x64 pixels
-   **Interface**: I2C (Hardware)
-   **Rotation**: U8G2_R0 (0 degrees)
-   **I2C Address**: 0x3C (shifted left by 1 for U8G2)
-   **Contrast**: 255 (maximum)

## Display Layout

The display is organized into a grid system where U8G2 breaks the 128x64 pixel screen into 8x8 pixel cells:

-   **Grid Size**: 8 cells tall × 16 cells wide
-   **Cell Origin**: (0,0) is top-left corner
-   **Cell Dimensions**: 8×8 pixels each

### Layout Regions

```
┌─────────────────────────────────────────────────────────┐
│ Header (12 cells)                     │ Icons (4 cells) │  Row 0
├─────────────────────────────────────────────────────────┤
│                                                         │
│                                                         │
│                                                         │
│                    Page Area                            │
│                   (6 rows)                              │
│                                                         │
│                                                         │
├─────────────────────────────────────────────────────────┤
│ Footer (16 cells)                                       │  Row 7
└─────────────────────────────────────────────────────────┘
```

#### Header Row (Row 0)

-   **Cells 0-12**: Header text (13 cells = 104 pixels)
-   **Cells 13-15**: State icons (3 cells = 24 pixels)
    -   WiFi status
    -   Bluetooth status
    -   Other system indicators

#### Page Area (Rows 1-6)

-   **6 rows** of main content
-   **16 cells wide** (128 pixels)
-   **48 pixels tall** (6 × 8 pixels)
-   Content is clipped to this area when using `clearPage()`

#### Footer Row (Row 7)

-   **Cells 0-14**: Footer text (15 cells = 120 pixels)
-   **Cell 15**: Timeout indicator (1 cell = 8 pixels)

## Thread Safety

The display uses a FreeRTOS mutex to ensure thread-safe access:

```cpp
extern SemaphoreHandle_t displayMutex;
```

### Usage Pattern

```cpp
// Always acquire mutex before drawing
if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
    // Perform drawing operations
    display.drawStr(x, y, "Hello World");

    // Always release mutex when done
    xSemaphoreGive(displayMutex);
}
```

## Core Functions

### Initialization

```cpp
void initDisplay();
```

-   Creates the display mutex
-   Initializes the U8G2 display object
-   Sets I2C address and display parameters
-   Clears the display buffer

### Clearing Functions

All clearing functions use `updateDisplayArea()` for efficient partial updates:

#### `clearHeader()`

-   Clears cells 0-12 of row 0 (header text area)
-   Preserves icon area

#### `clearIcons()`

-   Clears cells 13-15 of row 0 (icon area)
-   Preserves header text

#### `clearFooter()`

-   Clears cells 0-14 of row 7 (footer text area)
-   Preserves timeout indicator

#### `clearTimeout()`

-   Clears cell 15 of row 7 (timeout indicator)

#### `clearPage(bool includeFooter = false, bool includeHeader = false)`

-   Clears the main content area (rows 1-6)
-   Optionally includes header and/or footer
-   **Important**: Sets clipping window to prevent content overflow

### Refresh Functions

Refresh functions update specific display regions:

#### `refreshHeader()`

-   Updates header text area (cells 3-15 of row 0)

#### `refreshIcons()`

-   Updates icon area (cells 0-2 of row 0)

#### `refreshFooter()`

-   Updates footer area (cells 1-15 of row 0)

#### `refreshPage(bool includeFooter = false, bool includeHeader = false)`

-   Updates main content area (rows 1-6)
-   Optionally includes header and/or footer

#### `refreshTimeout()`

-   Updates timeout indicator area

### Content Functions

#### `setHeader(String &text)`

-   Sets header text (automatically converted to uppercase)
-   Caches text to avoid unnecessary redraws
-   Uses `u8g2_font_spleen5x8_mu` font

#### `setFooter(String &left, String &right)`

-   Sets left and right footer text
-   Right text is right-aligned
-   Both texts converted to uppercase
-   Caches text to avoid unnecessary redraws

#### `drawWrappedText(int x, int y, const String &text, bool center = false)`

-   Draws text with automatic word wrapping
-   Supports literal `\n` for line breaks
-   Returns number of lines drawn
-   Optional center alignment

## Drawing Guidelines

### 1. Always Use Mutex Protection

```cpp
if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
    // Drawing operations here
    xSemaphoreGive(displayMutex);
}
```

### 2. Use Clearing Functions

Prefer the provided clearing functions over manual buffer clearing:

```cpp
// ✅ Good
clearPage();
display.drawStr(0, 16, "Content");

// ❌ Avoid
display.clearBuffer();
display.drawStr(0, 16, "Content");
```

### 3. Respect Layout Boundaries

-   Use `clearPage()` to prevent content overflow
-   Header content should stay in cells 0-12
-   Footer content should stay in cells 0-14
-   Icons should be in cells 13-15

### 4. Font Selection

-   Header/Footer: `u8g2_font_spleen5x8_mu`
-   Page content: Use appropriate fonts for readability
-   Consider font height for proper spacing

## Memory Management

-   Display buffer is managed by U8G2 library
-   Text caching prevents unnecessary redraws
-   PROGMEM strings are used for constants (following project guidelines)

## Performance Considerations

-   `updateDisplayArea()` only updates specific regions
-   Text caching reduces redundant operations
-   Clipping windows prevent unnecessary buffer operations
-   Mutex ensures atomic drawing operations

## Common Patterns

### Drawing a Complete Screen

```cpp
if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
    clearPage();
    display.setFont(u8g2_font_spleen5x8_mu);
    display.drawStr(0, 16, "Main Content");
    refreshPage();
    xSemaphoreGive(displayMutex);
}
```

### Updating Header Only

```cpp
if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
    String headerText = "NEW HEADER";
    setHeader(headerText);
    xSemaphoreGive(displayMutex);
}
```

### Drawing Wrapped Text

```cpp
if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
    clearPage();
    int lines = drawWrappedText(0, 16, "Long text that will wrap", true);
    refreshPage();
    xSemaphoreGive(displayMutex);
}
```

## Dependencies

-   **U8G2 Library**: Core display functionality
-   **FreeRTOS**: Mutex for thread safety
-   **ESP32**: Hardware I2C interface
-   **Pins.h**: Hardware pin definitions

## References

-   [U8G2 Reference](https://github.com/olikraus/u8g2/wiki/u8g2reference)
-   [U8G2 Font List](https://github.com/olikraus/u8g2/wiki/fntlistallplain)
-   [FreeRTOS Mutex Documentation](https://www.freertos.org/Real-time-embedded-RTOS-mutexes.html)
