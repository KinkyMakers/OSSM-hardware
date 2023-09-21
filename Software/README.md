# OSSM Software
This is the recommended software for use with OSSM designs

# Loading a pre-built image onto your OSSM v2 PCB
The OSSM v2 PCB should come already pre-loaded with the OSSM Software. Occasional updates are released and are available as pre-built binaries.

These binaries can be loaded onto your OSSM v2 PCB (or other hardware) using the built-in DFU Upgrade option on the Web Interface. If for some reason your hardware is bricked, you can reload the binary using the [ESP32 Flash Tool](https://github.com/hanhdt/esp32-flash-tool)

<!-- ESP32 Flash Tool is not a final choice, but looks like a potential suggestion -->

# Code Style

#  Directory Structure
## Addons
## CANopen
## Controllers
## Fonts
## Images
## Motors
## Screens


### Requirements:
- Arduino IDE or PlatformIO
- ESP32 microcontroller

To use with Arduino, simply open the .ino file in the src directory. You will also need to copy the folders in the lib directory to your arduino library folder on your PC.

Use the tutorial below to install the ESP32 board into your Arduino IDE so you're ready to program!

[Setting up ESP32 on Arduino IDE](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)

If you aren't sure what ESP board to select in Arduino IDE, choose "ESP32 Dev Module" - it works on all ESP32 I have tested so far.



![ESP32 Architecture](https://github.com/KinkyMakers/OSSM-hardware/blob/master/PlatformIO%20ESP32%20code/OSSM_ESP32/OSSM%20ESP32%20Architecture.png)
