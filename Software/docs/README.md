# OSSM Project Documentation

Welcome to the Open Source Sex Machine (OSSM) project! This guide will help you understand our project structure and get started with development.

## Project Overview

OSSM is an ESP32-based control system for automated motion devices. The project uses PlatformIO for development and leverages several key technologies:

### Core Technologies

- **State Machine**: Built with [Boost SML](https://boost-ext.github.io/sml/) for robust state management
- **Display**: OLED display using [U8G2](https://github.com/olikraus/u8g2) library for UI
- **Task Management**: [FreeRTOS](https://www.freertos.org/) for concurrent operations
- **Motion Control**: [Elims' StrokeEngine](https://github.com/theelims/StrokeEngine) modified for OSSM usage.

## Getting Started

1. Install [VSCode](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/install/ide?install=vscode), you can also follow our guide [here](PlatformIO.md).
2. Clone the repository
3. Open the project in VSCode with PlatformIO
4. Review our [PlatformIO setup guide](PlatformIO.md) for detailed instructions