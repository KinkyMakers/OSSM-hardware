# OSSM - Open Source Sex Machine

[![Discord](https://img.shields.io/discord/559409652425687041)](https://discord.gg/wrENMKb3)


OSSM (pronounced like "Awesome") is a user friendly every day sex machine for the people.

This project aims to help people curious about sex machines explore their interest. A second objective is optionally
learning how mechanics, electronics, physics and computing are involved in your sexual pleasure.

Please note that this is a _work in progress_ and we have attempted to keep compatibility with the current BOM going
forward, but it's not guaranteed.

_Our Primary design goals_ are to make a machine that is compact, quiet, of moderate cost, 3D printable (no
cutting/machining required), flexible, highly performant, with easily sourced components, and doesn't look like a giant
machine.
<p align="center">
<img src="https://user-images.githubusercontent.com/12459679/200219198-df577cbc-8503-47af-bbc8-9cb65de7ea49.jpg" width="750">
</p>

### Is it actually Open Source?

It is one of a kind Certified, Actual, Factual, Open Source

[<img width="287" alt="image" src="https://github.com/user-attachments/assets/af168015-b80a-4464-8778-e391278d9748">](https://certification.oshwa.org/ca000057.html)



## Getting Started

### [Read the Frequently Asked Questions](FAQ.md)

There are a few hardware flavours to choose from, we've included community modified versions in case that fits your use
case better!

### [Build Instructions](Documentation/Assembly%20Instructions.pdf)

### [Build Videos]("https://youtube.com/playlist?list=PLzSK7OAu3KNQsFo6WJGT8P28lfkD3xpps")

[Join our Discord](https://discord.gg/MmpT9xE) to be part of the discussion and get help with your build. We have a huge community of
makers!

<p align="center">
<img width="750" alt="image" src="https://github.com/KinkyMakers/OSSM-hardware/assets/43324815/a756a8d5-c075-4e86-8206-b553a0b77127">
</p>

### Software

The software is available in this github repository. It is written and compiled utilizing PlatformIO on Visual Studio
Code. [Reference for working with the code in PlatformIO here](OSSM PlatformIO Readme.md)

We recommend using
the [Research and Desire Reference Board](https://shop.researchanddesire.com/products/ossm-reference-board) as Do It
Yourself by someone without extensive electronics knowledge creates a lot of support overhead. This code is still
arduino IDE compatible but offers many times better performance. Web based control is projected to be available in Feb 2025.  
### Eagle PCB

Simple PCB to power an ESP32 (wifi enabled microcontroller) from 24V and breakout the pins for the OSSM control

### Mechanical design

The OSSM use a compact belt design with components that have become widely available due to 3D printing popularity.
It is driven by a Nema23 motor of **your** choosing, although we reccomend small integrated closed loop servos for their
silent operation and very high performance.
<p align="center">
<img src="https://user-images.githubusercontent.com/12459679/200219706-f93fa014-7fea-4f31-b5d0-d89d4e6ab130.png">
</p>

### Mounting

We have a new stand design you can check out on onshape!
[OnShape link](https://cad.onshape.com/documents/d520ea9a8cadb4ae8681f59b/w/00b211a6fa3b76c59ef28f4e/e/5f2c80d9d9c2433a7be7f789)

![OSSM on mounting solution](https://github.com/KinkyMakers/OSSM-hardware/assets/12459679/fa8a99c4-935e-4774-9f97-a4f88e453722)

### Safety

The safety of the OSSM build is yet to be fully characterized as it is a work in progress. The OSSM is a framework for
building your own sex machine and as such your specific combination may have risks not inherent to other builds. These
risks may be undocumented or undiscovered. Please see the [Hazards](Documentation/Hazards.md) file for more
information.

While using the OSSM we can suggest the following hierarchy of safety, however it is up to you and your build to decide
what risks exist and how to mitigate them.

1. Have ability to move away
2. Have ability to remove the power
3. If you are in bondage, safety is responsibility of the Top

## Bill Of Materials

We are calling this the reference build, when deviating from it please check compatability with existing Bill Of
Materials (BOM)

1.  **[3D Printed Parts](Hardware/OSSM%20Printed%20Parts)**
    - This has recently had significant changes
    - [Make sure to choose one of the options for the toy adapters](Hardware/OSSM%20Printed%20Parts/end%20effector%20options)
    - There are several mounting options available, the most popular being  
      the [Ulanzi Ball head mount](Hardware/OSSM%20Mounting) or  
      the [Shicks mount](Hardware/OSSM%20Mounting/Shicks%204040%20mount)
    - Thank you @Elims for the [belt tensioner design]( https://github.com/theelims )

2.  **IHSV57 NEMA23 Servo with 8mm shaft** _or_ **57AIM30 (Gold Motor)**:
    - **Gold Motor**
      - New standard motor for OSSM, please see [Gold Motor Info](Documentation/Gold-Motor.md) while docs are being updated.
        This motor is cheaper, more compact and has improved features which are being implemented in OSSM firmware.
      - For the latest information about this change, join the KinkyMakers Discord.
    - **iHSV57 (Previous Standard Motor)**
      - _Avoid the StepperOnline version_
      - Make sure you get something with **8mm** shaft.
      - There are **3** sizes of this motor:  
          100W = [iHSV57-30-**10**](https://www.aliexpress.com/w/wholesale-iHSV57%2525252d30%2525252d10.html)

          140W = [iHSV57-30-**14**](https://www.aliexpress.com/w/wholesale-iHSV57%2525252d30%2525252d14.html)

          180W = [iHSV57-30-**18**](https://www.aliexpress.com/w/wholesale-iHSV57%2525252d30%2525252d18.html)

      - We recommend motors with firmware version 6 (shown as `V60x`). Pay attention to
        this, [the firmware version is printed on the label on the side.](https://user-images.githubusercontent.com/131713378/234460307-1c29c18b-3bb5-4ac9-b66f-0dea9df0acac.png)
        You cannot update the motor firmware. version 5 will work, but it is not as feature rich for potential new
        features.
      - Search around for the best deal for you - we reccommend searching "ihsv57" on Aliexpress.com and choosing the -10
        -14 or -18. Some example listings:

        [AliExpress - US](https://www.aliexpress.us/item/2251832528412325.html)

        [AliExpress - CA & AU](https://www.aliexpress.us/item/32714727077.html)


      - For details on picking the right motor for your use case - check
          this [FAQ](FAQ.md#q--what-strength-of--motor-do-i-need)
      - If you are using the 140W or 180W version it is recommended to use a 10mm wide belt and pulley (see the next two items)


3.  **GT2 Pulley 8mm Bore 20 Tooth, 10mm width** :

    [Amazon - CA](https://www.amazon.ca/Timing-Pulley-8mm-bore-10mm-belt-20-Tooth/dp/B09L7D7HHT)

    [Amazon - US](https://www.amazon.com/WINSINN-Aluminum-Synchronous-Timing-Printer/dp/B07BTDRW5Z)

    [Banggood - AU](https://au.banggood.com/5MM-or-6_35MM-or-8MM-Bore-20TeethGT2-Alumium-Timing-Pulley-For-Width-10mm-GT2-Belt-p-1106314.html)

4.  **GT2 Timing Belt - 10mm width** :

    [Amazon - CA](www.amazon.ca/Timing-Meters-Creality-Anycubic-Printer/dp/B097T4DFM6)

    [Amazon - US](https://www.amazon.com/Timing-Meters-Creality-Anycubic-Printer/dp/B097T4DFM6)

    [Amazon - AU]()

    - Your desired stroke length plus about 200mm should be your minimum order length


5.  **Bearings** MR115-2RS 5x11x4mm :
    [Amazon - CA](https://www.amazon.ca/gp/product/B07CVBW44R)

    [Amazon - US](https://www.amazon.com/Miniature-Bearings-MR115-2RS-Double-Shielded-5x11x4mm/dp/B08PFT72RQ)

    [Aliexpress - AU](https://www.aliexpress.us/item/2251832628223170.html)

6.  **MGN12H Rail and bearing** :

    [Amazon - CA](https://www.amazon.ca/MGN12H-Stainless-Carriage-Precision-Machine/dp/B09TWKWCZR)

    [Amazon - US](https://www.amazon.com/ReliaBot-Linear-Carriage-Printer-Machine/dp/B07B4DWWZC)

    [Aliexpress - AU](https://www.aliexpress.com/item/32840113910.html)

    - Minimum 250mm in length, suggested 350mm
    - Rail length = desired stroke + 180mm
    - Must be MGN**12H** rail - H is a longer car than C which gives greater stability. 12 indicates 12mm rail width.

7.  **Power Supply**: 24 volt 4-5 amp w/ 2.1mm barrel DC plug

    - Larger motors generally need more power!  
      180W -> 24V 5A suggested minimum  
      140W -> 24V 4A suggested minimum  
      100W -> 24V 4A suggested minimum

    - See the FAQ for more details about power supply sizing based on real world experience of OSSM users.
    - The OSSM will work with small power supplies, but will be limited on maximum force.
    - Ensure the power supply is fully enclosed and shielded to avoid cross talk from electromagnetic interference (like a laptop power supply).
    - Ensure the power supply has the correct approvals for your location - This really helps ensure performance as well! (UL, CE, etc.)
    - It is strongly recommended that you purchase a good quality PSU such as a Meanwell.

8.  **Metric Hex Cap Screws** :  
    [Amazon - CA & US](https://www.amazon.ca/Comdox-500pcs-Socket-Screws-Assortment/dp/B06XQLTLHP)

    [Amazon - AU](https://www.amazon.com.au/VIGRUE-Stainless-Hexagon-Washers-Assortment/dp/B08CK9Y971) (this is a little
    expensive, we are looking for a cheaper alternative)

    - A kit like the ones above will provide what's needed:
    - 4x m5x20
    - 1x m5x12 (can also use m5x20)
    - 10x m3x8
    - 2x m3x16
    - 8x m5 nuts
    - 8x m3 nuts

9.  **ESP32 Development Board**
    - [An official OSSM reference PCB](https://research-and-desire.myshopify.com/collections/all/products/ossm-reference-board)
    - We do not currently have a best suggestion if you are not using a reference board, most generic development boards are the same
    - We have found that the 3.3v boards may miss steps at high speed, so please use a level shifter as well.
    - To start working on the project, something like this [Adafruit board](https://www.adafruit.com/product/3405) is an excellent place to start.
    - Accessories required for prototyping
      include [Breadboard](https://www.amazon.ca/Breadboard-Solderless-Prototype-Distribution-Connecting/dp/B01EV6LJ7G/ref=sr_1_5?dchild=1&keywords=breadboard&qid=1627823170&sr=8-5)
      and [Dupont Jumpers](https://www.amazon.ca/120pcs-Multicolored-Breadboard-Arduino-raspberry/dp/B01LZF1ZSZ/ref=sr_1_5?dchild=1&keywords=dupont+jumper&qid=1627823220&sr=8-5)

### Octopart Links for ease of ordering and tracking

A list of non-3d printed supplies in one place. Most of the parts aren't available through the preferred Octopart
vendors but it still makes for a convenient list.

[Octopart - Collected Bill of Materials for US Suppliers](https://octopart.com/bom-tool/nfUsbySS)

## Official OSSM Wiring

![OSSM reference board front](https://user-images.githubusercontent.com/43324815/150361448-80e9fdaf-4a8c-4054-a920-6eab9aa68678.png)
The above image is of a version 1 reference board.

### OSSM PCB Connections

![OSSM PCB connection diagram](https://user-images.githubusercontent.com/43324815/150355658-2ab2c53f-8da0-41ce-ad61-cfe9965b9ab2.png)

### GPIO Layout

![OSSM pinout diagram](https://user-images.githubusercontent.com/12459679/152600401-80b986ea-6f5b-480d-ba74-5b5001079c1b.png)
![JST Header location with pin labels](https://user-images.githubusercontent.com/12459679/226189433-db28dfc6-22ac-4fdb-8b45-afe0c3fa9a7b.png)

## Non-Official OSSM Wiring

This should be a good start for the wiring of your OSSM! However, depending on your hardware mix settings or wiring may
be different

### OSSM PCB w/ TB6600 Stepper Driver

<img width="1028" alt="image" src="https://user-images.githubusercontent.com/43324815/159145946-a9960bba-c9bc-4717-b3b4-5b5c34b4a3d2.png">

### Servo Wiring

![ServoWiring](https://github.com/NightmareSyndrome/OSSM-hardware/assets/131713378/783876cf-f5bf-4708-9b4f-bd89723713f9)

### Stepper Wiring

![wiring notes](Hardware/PCB%20Files/Archive/Alpha%20PCB%20-%20never%20widely%20used/wiring%20notes%20800.png)

