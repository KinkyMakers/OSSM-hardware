# New Standard Servo Motor

## 57AIM30 - Gold Motor

We have been testing and have decided on a new motor as the build standard. It has the same mounting pattern and shaft diameter as the old motor (iHSV57-30-XX).
It is sold by Research and Desire as [Gold Motor](https://www.researchanddesire.com/products/ossm-motor-gold-motor) or available generically as 57AIM30

If you wish to order a 57AIM30, you should source a model that runs at 1500 RPM and has an RS485 interface (NOT the 3000 RPM or CANBUS versions)

Also note that if you source your motor from somewhere other than R&D, you will need to reprogram the stored settings with a USB to RS485 dongle using the scripts found [here](https://github.com/KinkyMakers/OSSM-hardware/tree/master/Hardware/Servo%20Tools/Gold%20Motor) before you can use it.

Advantages:

- Cheaper (20%) - dependant on availability and location.
- Smaller (40% length of 180W standard - i.e. 60% reduction!).
- Lighter (50% weight of 180W).
- Higher peak force (30% higher in real-world test).
- Better over-torque handling - can be configured to reduce torque output instead of disable like current 180w servo.
- Is drop-in replacement for iHSV57 series making BOM compatible.

Requirements:
- Requires V2.2 PCB or newer for  simple wiring with PH-4 cable
  - Older boards will need wires soldered to 10 pin header
  - Recommend adding 1500uF capacitor, min 50V to older boards <V2.3

Disanvantages:

- More efficient (this causes more regenerated power in some motion conditions which requires additional capacitance and handling on the PCB).
  - PCB v2.3 as currently sold by R&D or listed on this repo has added capacitor. If you are upgrading your motor, you should add the capacitor as shown below.
- Digital comms over RS485 (Not worse than current RS232, just current boards don't have the needed adapter).
- Requires software setup to see full benefits (default FW settings disable motor in over-torque condition).
- If modding your software, technically lower top speed than HSV. (Standard code speed limit is lower than gold motor limit, so not a disadvantage for regular user).

## 42AIM30 - Gold Motor 42 (variant)

In addition to 57AIM30 (Gold Motor), which fits exactly to the previous designs and specs with no extra work, there is a varient motor which has exactly same internals and specs.
The 42AIM30 (see notes above for ordering the correct model) does away with the heatsink housing and is purely the cylindrical motor with connectors at the base.

Advantages (compared to 57AIM30):

- Smaller diameter (26%) - plus cylinder vs square.
- Lighter (unknown - ~25%?) - please submit mass if you have one.

Disadvantages (compared to 57AIM30):

- Much harder to mount.
  - No _mounting ears_ to bolt through, must put exact length machine screws into face of motor.
  - Current standard housings are not compatible with 42AIM30, though there are some options available via discord.
- Suspected heat issues caused by lack of heat sink and enclosure in printed housing - more data needed.
- Lack of clear advantages over Gold Motor (57AIM30)
  - Same specs.
  - Same length.
  - Same disadvantages.
  - Changes to BOM required.

![OSSM Gold Motor Wiring](https://github.com/KinkyMakers/OSSM-hardware/assets/12459679/10072632-6e04-495e-b95d-b963d1662924)

![IMG_1049](https://github.com/KinkyMakers/OSSM-hardware/assets/12459679/7bec39aa-364f-446a-8b29-4f9390e9d71e)

![image](https://github.com/KinkyMakers/OSSM-hardware/assets/12459679/37d83251-a305-4a17-a2c6-f16c2b8f5547)

![OSSM Capacitor view](https://github.com/KinkyMakers/OSSM-hardware/assets/12459679/edfe7a90-74a8-4c3e-a55a-6b22099aafb8)
