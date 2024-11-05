# Software
This is the recommended software for use with OSSM designs

## Requirements:
- Arduino IDE or PlatformIO
- ESP32 microcontroller

To use with Arduino, simply open the .ino file in the src directory. You will also need to copy the folders in the lib directory to your arduino library folder on your PC.

Use the tutorial below to install the ESP32 board into your Arduino IDE so you're ready to program!

[Setting up ESP32 on Arduino IDE](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)

If you aren't sure what ESP board to select in Arduino IDE, choose "ESP32 Dev Module" - it works on all ESP32 I have tested so far.

![ESP32 Architecture](https://github.com/KinkyMakers/OSSM-hardware/blob/master/PlatformIO%20ESP32%20code/OSSM_ESP32/OSSM%20ESP32%20Architecture.png)


## Logging

This project uses ESP32's [native logging library](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/log.html).


## Commands

**Upload and monitor**
```bash
pio run -t upload -t monitor
```

**Run Unit Test tests**
```bash
pio test -e test
```

## Other Documentation
- [Git Branching Strategy](docs/Git Branching Strategy)