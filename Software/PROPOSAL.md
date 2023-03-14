# Proposal for Software Architecture
## Board-Level Defines
There are several hardware targets for driving a Fucking Machine currently. The primary ones are OSSM v2, CANfuck, and FuckIO. To support these, we're aiming to keep two levels of configuration, Board-Level, and User-Level. 

Board-Level defines allow turning on functionality that might be board-specific, or configuring pinouts. 

User-Level defines allow setting what type of motor you are using, addons you have configured, and other more user-facing options.
## Addon Support (such as Eject Project)
Exactly how Addon Support will work is yet to be determined, but easy integration of other projects like [Eject Cum Tube](https://github.com/ortlof/EJECT-cum-tube-project/)

## ESP32 Preferences via StatefulService (extended)
Configuration which doesn't require a re-compile will be stored using the [esp8266-react](https://github.com/rjwats/esp8266-react) [StatefulService](https://github.com/rjwats/esp8266-react/blob/master/lib/framework/StatefulService.h), but extended to save to both [ESP32 Preferences](https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/preferences.html) & JSON.

This will be used for example, WiFi Credentials, Rail Length, Max Speed/Accelerations, and other useful to configure settings.

Configuration will be available both through the Web UI & whatever protocol we end up designing for x-toys/other external interfacing

## Web UI based on Sveltekit + TypeScript
Sveltekit will allow us to keep compile sizes significantly lower than a large React bundle. We aim to keep all resources needed for the UI stored locally in flash. This allows remote usage of the OSSM where WiFi/Internet might not be available.

Typescript will be a more secure/safe setting for development than standard Javascript.

While the Sveltekit UI is being worked on, we are using ESP-Dash for a stand-in. We'll swap it out later when the UI is ready for it.

## Github Workflows for linting and pre-building images for users
We have yet to determine what type of linting will be used exactly, but there is a test using clangtidy. We use GitHub Workflows to automatically test the linting on PRs before allowing them to be accepted. This mimics a fairly standard industry Continous Integration system.

In addition, each time master is updated, GitHub Workflows will re-compile several pre-built images for each hardware target. This should allow users to easily upgrade their device without concerning themselves with compiling / PlatformIO

## Keeping shared functionality useful for sex-tech in Libraries
OSSM is only one of the various sex-tech projects the Kinky Makers discord members are working on. Because of this, we are aiming to extract as much useful common code out of this repository. This will hopefully allow others to quickly get their devices up and running, and even potentially integrating with OSSM.

## Functionality we want to add
### Virtual Com Port (Already built, needs integration)
This would allow anyone to remotely re-configure their iHSV or LinMot motor over a RS232 connection. The ESP32 would expose a TCP socket, and the user can connect with software like [HW VSP3](https://www.hw-group.com/software/hw-vsp3-virtual-serial-port)

### Screen Support for displaying WiFi AP credentals, and other useful information (Already built, needs polish)
Only the CANfuck hardware currently has support for this functionality.
It's based around a tiny [1.69" 240x280 IPS LCD w/ ST7789 Controller over SPI](https://www.aliexpress.us/item/3256803567938962.html)

It currently allows displaying WiFi AP credentals for a user to connect to for first-time configuration. It will also show basic information about the state of the OSSM, such as IP on the local network

### mDNS support for local networks (Not yet built)
mDNS would allow us to advertise a easy to remember DNS entry on any local network the OSSM connects to. For example, we could advertise "ossm.local" and any network user would be able to load the UI.

This helps reduce the complexity of knowing the IP, or looking up the DHCP lease of the ESP32 when doing setup