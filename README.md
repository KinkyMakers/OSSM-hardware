# OSSM - Open Source Sex Machine

<div style="display: flex; justify-content: space-between;">
  <img src="Documentation/_images/Overview Low ALT - R2P - Actuator Removed.png" style="width: 50%; height: auto;" />
  <img src="Documentation/_images/Overview ALT - R2P - Unfolded.png" style="width: 50%; height: auto;" />
</div>

### [**KinkyMakers Discord Community**]([Files/](https://discord.gg/wrENMKb3))  
![Discord](https://img.shields.io/discord/559409652425687041)  
Most OSSM development takes place through ideas and CAD contributions made by enthusiastic discord members, join the discussion or show off your build!  

### [**Read the Frequently Asked Questions**](FAQ.md)

### [**Build Videos**]("https://youtube.com/playlist?list=PLzSK7OAu3KNQsFo6WJGT8P28lfkD3xpps")

## **Summary**   
OSSM (pronounced like "Awesome") is a user friendly every day sex machine for the people.

The OSSM's primary feature is that it is a servo powered belt-driven linear rail.  
This allows for quiet, high torque operation at speeds up to 1 meter per second, as well as for software-defined stroke and depth. 

**Default/Recommended build at a glance**
 - 32 lbs (14 kg) of force
 - 8" Stroke (20 cm) with 350mm rail

This project aims to help people curious about sex machines explore their interest.   
A second objective is optionally learning how mechanics, electronics, physics and computing are involved in your sexual pleasure.

### **Primary** Design Goals
- Compact & quiet
- Highly performant
- Moderate cost
- 3D printable
- Easily-sourced components
- Moddable
  
<p align="center">
<img width="750" alt="image" src="https://github.com/KinkyMakers/OSSM-hardware/assets/43324815/a756a8d5-c075-4e86-8206-b553a0b77127">
</p>

Note: This is a _work in progress_ and we have attempted to keep compatibility with the current BOM going
forward, but it's not guaranteed.



### Is it *actually* Open Source?
    CERN Open Hardware Licence Version 2 - Strongly Reciprocal
It is one of a kind Certified, Actual, Factual, Open Source  
[<img width="287" alt="image" src="https://github.com/user-attachments/assets/af168015-b80a-4464-8778-e391278d9748">](https://certification.oshwa.org/ca000057.html)

Contributions to this GitHub repository assume the same license.

## **Safety**

The safety of the OSSM is yet to be fully characterized as it is a work in progress.  
The OSSM is a framework for building your own sex machine and as such your specific combination may have risks not inherent to other builds.  
**These risks may be undocumented or undiscovered. Please see the [Hazards](Documentation/Hazards.md) file for more information.**

Use of experimental/modified code increases the level of risk.

While using the OSSM we can suggest the following hierarchy of safety, however it is up to you and your build to decide
what risks exist and how to mitigate them.

1. Have ability to move away
2. Have ability to remove the power
3. If you are in bondage, safety is responsibility of the Top

## **Building Your OSSM**
### Motor Support  
 - 57AIM30 "Gold Motor" (Recommended/Default)
 - 42AIM30 "Round Motor" (Smaller form factor, limited mounting support)
 - iHSV57 "Legacy Motor" (Pre-2024 recommended motor)

The OSSM Software is written to support closed loop servo type motors and tested on the above motors.  
Most OSSM actuator and mounting hardware is designed for NEMA23 form factor motors.  
Attempting to use a stepper or other motor is not recommended; while it may be technically possible in some cases, this usually requires custom design work and advanced-level custom code. 

### Bill Of Materials for Recommended/Default Build

### [**Printed Parts**](Printed%20Parts/) 
 - [Actuator](Printed%20Parts/Actuator/)
   - Body
   - Belt Tensioner
   - Threaded End Effector, Clamp + Nut  
 - [Remote](Printed%20Parts/Remote/)
   - Body
   - Knobs
 - [Toy Mounting](Printed%20Parts/Toy%20Mounting/)
   - Tiedown/suction plate with clamping ring
 - [Mounting](Printed%20Parts/Mounting/)
   - Mounting Ring (PitClamp Mini Ring)
   - Base (PitClamp Mini Base)
   - PCB Enclosure
 - [Stand](Printed%20Parts/Stand/)
   - 3030 Extrusion Base

### Electronics  

 - Motor 
   - 57AIM30 "Gold Motor"
     - **Important note:** In order to operate with OSSM Software, motors must receive programming to set their "steps per revolution" to 800
     - Flashing requires a 4 pin USB to RS485 connector, cables, and use of the [Gold Motor Porgramming Tool](/Hardware/Servo%20Tools/Gold%20Motor/)
 - OSSM Reference Board + Wiring
 - OSSM Remote
 - 20-36v DC Power Supply (5.5 x 2.1 Barrel Plug)
   - 24 volt 4-5 amp is recommended
   - USB Power Banks capable of true 100w USB PD also generally seem to operate well
     - [TESTED: ✓] INIU Power Bank B63 100w
     - [TESTED: Powers down on high load] INIU B62 Power Bank 65W

### Hardware
 - **GT2 Pulley** 
   - 8mm Bore 
   - 20 Tooth
   - 10mm width
 - **GT2 Timing Belt** 
   - 10mm width
   - 500mm length
 - **MGN12H Rail and bearing**
   - Minimum 250mm in length, suggested 350mm
   - Must be MGN**12H** rail - H is a longer bearing than C which gives greater stability. 12 indicates 12mm rail width.
 - **Bearings** (Qty: 6) 
   - MR115-2RS 5x11x4mm
 - **Metric Hex Cap Screws**
   - //TODO: 
   - List (Qty: 6) 
   - Of (Qty: 6) 
   - Screws (Qty: 6) 
 - **ESP32 Development Board or [OSSM Reference PCB](https://research-and-desire.myshopify.com/collections/all/products/ossm-reference-board)**
    - More information for board development [here](/Documentation/Board_Design.md).

## Assembly
![](Printed%20Parts/Actuator/_images/Exploded%20-%20Actuator%20Default.png)  
### [**Build Instructions**](Documentation/Assembly%20Instructions.pdf)
(03.30.2025) Note: There have been recent improvements to the OSSM Recommended/Default build that are not yet reflected in this build document.  
Refer to exploded views from the Printed Parts folders for assembly of each major component.
### [**Build Videos**]("https://youtube.com/playlist?list=PLzSK7OAu3KNQsFo6WJGT8P28lfkD3xpps")