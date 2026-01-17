# OSSM - Open Source Sex Machine

### **Maintained by Research and Desire, supported by the community!**

### Check us out at [**researchanddesire.com**](https://researchanddesire.com)

![OSSM Overview Image](Documentation/_images/OSSM%20Banner%20Image.png)

### [**KinkyMakers Discord Community**](https://discord.gg/VtZcudpxT6)

![Discord](https://img.shields.io/discord/559409652425687041)\
Most OSSM development takes place through ideas and CAD contributions made by
enthusiastic discord members, join the discussion or show off your build!

### Check out our docs at [GitBook](https://kinky-makers.gitbook.io/)

### [**Read the Frequently Asked Questions**](FAQ.md)

## **Summary**

**OSSM** (pronounced like "awesome") is a user friendly every day sex machine
for the people.

The OSSM's primary feature is that it is a servo powered belt-driven linear
rail.\
This allows for quiet, high torque operation at speeds up to 1 meter per second,
as well as for software-defined stroke and depth.

**Standard Recommended Build at a glance**

- 32 lbs (14 kg) of force @ 20v DC
  - Up to 50 lbs (22 kg) @ 36v DC
- 8" Stroke (20 cm) with 350mm rail

This project aims to help people curious about sex machines explore their
interest.\
A second objective is optionally learning how mechanics, electronics, physics
and computing are involved in your sexual pleasure.


<p align="center">
<img width="750" alt="image" src="https://github.com/KinkyMakers/OSSM-hardware/assets/43324815/a756a8d5-c075-4e86-8206-b553a0b77127">
</p>

Note: This is a _work in progress_ and we have attempted to keep compatibility
with the current BOM going forward, but it's not guaranteed.

## **Building Your OSSM**



### Bill Of Materials for Standard Recommended Build

### Electronics

- Motor
  - 57AIM30
    ["Gold Motor"](https://www.researchanddesire.com/products/ossm-motor-gold-motor)

      [Gold Motor Programming Tool](/Hardware/Servo%20Tools/Gold%20Motor/)
- OSSM Reference Board, Remote, Wiring
  - [OSSM Reference PCB](https://www.researchanddesire.com/products/ossm-reference-board)
    or ESP32 Development Board
    - More information for board development
      [here](/Documentation/Board_Design.md).
  - [OSSM Remote](https://www.researchanddesire.com/products/ossm-reference-board)
  - JST-PH 2.0 4-Pin data cable and 16awg power wire
    - [Wire harness - Gold Motor](https://www.researchanddesire.com/products/ossm-wire-harness-gold-motor)
- 20-36v DC Power Supply (5.5 x 2.1 Barrel Plug)
  - [24 volt 5 amp](https://www.researchanddesire.com/products/ossm-24v-power-supply)
    is recommended
  - Higher voltage, up to 36v, will provide increased force
  - USB Power Banks capable of true 100w USB PD also generally seem to operate
    well for a portable OSSM
    - [TESTED: ✓] INIU Power Bank P63-E1 100w (Old model: B63)
    - [TESTED: Powers down on high load] INIU B62 Power Bank 65W

### [**Printed Parts**](Printed%20Parts/)

- [Actuator](Printed%20Parts/Actuator/)
  - Body
  - Belt Tensioner
  - Threaded End Effector, Clamp + Nut
- [Remote](Printed%20Parts/Remote/)
  - Body
  - Knobs
- [Toy Mounting](Printed%20Parts/Toy%20Mounting/)
  - Flange Base Plate with Clamping Ring (Tie-Down or Suction)
  - Double Double (Vac-U-Lock)
- [Mounting](Printed%20Parts/Mounting/)
  - Mounting Ring (PitClamp Mini Ring)
  - Base (PitClamp Mini Base)
  - PCB Enclosure
- [Stand](Printed%20Parts/Stand/)
  - 3030 Extrusion Base

Experimental parts pop up in the
[KinkyMakers Discord](https://discord.gg/wrENMKb3) #ossm-print-testing channel.\
W.I.P. for the top rated to be merged into this repo.

### Standard Actuator Hardware

**GT2 Pulley**

    (Qty 1) 8mm Bore, 20 Tooth, 10mm Width

**GT2 Timing Belt**

    (Qty 1) 10mm Width, 500mm length

**MGN12H Rail + Bearing Block**

    (Qty 1)
    Minimum 250mm
    Suggested 350mm
    Maximum 550mm

Rail length = desired maximum stroke + 180mm\
Must be MGN**12H** rail.\
H is a longer bearing block than C for greater stability. 12 indicates 12mm rail
width.

**Ball Bearings**

    (Qty 6) MR115-2RS 5x11x4mm

**Fasteners**

    (Qty 8) M3x8  Socket Cap Head Bolt 
    (Qty 2) M3x16 Socket Cap Head Bolt 
    (Qty 1) M3x20 Socket Cap Head Bolt 
    (Qty 7) M3 Hex Nut
    (Qty 3) M5x20 Socket Cap Head Bolt
    (Qty 1) M5 Hex Nut
    (Qty 4) M5x35 Socket Cap Head Bolt 
    (Qty 4) M5 20mm Hex Coupling Nut (Or M5 Hex Nut)

**Additional hardware is required for Stand, Mounting, Remote\
Detailed in their respective [Printed Parts](Printed%20Parts/) folder**

## Assembly

**Important note:** The actuator rail direction is critical for pattern accuracy
and safety functions. The proper direction is with the threaded end to the right
when looking at the front face of the actuator body - the "M" side of the OSSM
text on the Cover.\
Outdated videos and documentation may show different directions. Your rail
should extend the threaded end first when booted; if this does not match the
behavior of your build you should reverse your rail's printed hardware.

![](Printed%20Parts/Actuator/_images/Exploded%20-%20Actuator%20Default.png)

### [**Build Instructions**](Documentation/Assembly%20Instructions.pdf)

(03.30.2025) Note: There have been recent improvements to the OSSM Standard
Recommended build that are not yet reflected in this build document.\
Refer to exploded views from the Printed Parts folders for assembly of each
major component.

## Build Videos

[OSSM Assembly Playlist](https://youtube.com/playlist?list=PLzSK7OAu3KNQsFo6WJGT8P28lfkD3xpps)

[OSSM Complete Assembly - Follow Along Guide](https://www.youtube.com/watch?v=9lVobSEw_Uw)
