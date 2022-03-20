# OSSM - Open Source Sex Machine
OSSM (pronounced like "Awesome") is a user friendly every day sex machine for the people.

This project aims to help people curious about sex machines explore their interest. A second objective is optionally learning how mechanics, electronics, physics and computing are involved in your sexual pleasure.

Please note that this is a _work in progress_ and we have attempted to keep compatibility with the current BOM going forward, but it's not guaranteed.

*Primary design goals* are to make a machine that is Compact, Quiet, Moderate cost, 3D printable (no cutting/machining), High performance, flexible, Easily sourced components, Doesn't look like a giant machine.

## Getting Started

<h3><p align="center"><a href="FAQ.md">Read the Frequently Asked Questions</a></p></h3>

There are a few hardware flavours to choose from, we've included user modified versions in case that fits your use case better!

<h3><p align="center"><a href="https://github.com/KinkyMakers/OSSM-hardware/blob/master/Documentation/Assembly%20Instructions.pdf">Build Instructions</a></p></h3>

Join our Discord to be part of the discussion and get help with your build. https://discord.gg/MmpT9xE

### Software

The software is available in this github repository, the software is written and compiled utilizing PlatformIO on Visual Studio Code. <a href="OSSM PlatformIO Readme.md">Reference for working with the code in PlatformIO here</a>

We recommend using the ESP32 microcontroller. This code is still arduino IDE compatible but offers many times better performance and a nice internet dashboard you can use!
Control your OSSM at https://app.researchanddesire.com/ossm !

### Eagle PCB
Simple PCB to power an ESP32 (wifi enabled microcontroller) from 24V and breakout the pins for the OSSM control

### Mechanical design
The OSSM use a compact belt design with components that have become widely available due to 3D printing popularity.
It is driven by a Nema23 motor of **your** choosing, although we reccomend small integrated closed loop steppers for their cost to performance ratio.

<img src="https://github.com/KinkyMakers/OSSM-hardware/blob/5cbd1378f6389e5d8ece273931e7b261c27d1871/Documentation/OSSM%20mechanical%20overview.png" width="750" >

### Safety

The safety of the OSSM build is yet to be fully characterized as it is a work in progress. The OSSM is a work in progress and as it is a framework for building your own sex machine your specific combination may have risks not inherent to other builds. These risks may be undocumented. 

While using the OSSM we can suggest the following heiracrhy of safety, however it is up to you and your build to decide what risks exist and how to mitigate them. 

- A) Have ability to move away
- B) Have ability to remove the power
- C) If in bondage, that is responsibility of the Top




## Bill Of Materials

We are calling this the reference build, when deviating from it please check compatability with existing BOM

1) **[3D Printed Parts](Hardware/OSSM%20Printed%20Parts)**
   - This has recently had significant changes
   - [Make sure to choose one of the options for the toy adapters](Hardware/OSSM%20Printed%20Parts/end%20effector%20options)
   - Mounting options are still something that need to be worked on
   - Thank you @Elims for the [belt tensioner design](https://media.discordapp.net/attachments/756320102919700607/858110808281317396/unknown.png) ( https://github.com/theelims/FuckIO )
2) ** IHSV57 NEMA23 Servo with 8mm shaft** : [Amazon.ca](https://www.amazon.ca/Integrated-Servo-Motor-IHSV57-30-10-3000rpm/dp/B081CVJHC7) | [JMC](https://www.jmc-motor.com/product/953.html)
   - *Avoid the StepperOnline version until we can further test*
   - Make sure you get something with **8mm** shaft.
   - There are *3* sizes of this motor.  100W = iHSV57-30-**10**, 140W = iHSV57-30-**14**, 180W = iHSV57-30-**18** 
   - Search around for the best deal for you - reccommend searching "ihsv57" on Aliexpress.com and choosing the -10 -14 or -18
   - For details on picking the right motor for your use case - check this [FAQ](https://github.com/KinkyMakers/OSSM-hardware/blob/master/FAQ.md#q--what-strength-of--motor-do-i-need)
   - If you are using the 140W or180W version it is recommended to use a 10mm wide belt and pulley (see the next two items)
3) **GT2 Pulley 8mm Bore 20 Tooth** : [Amazon.ca](https://www.amazon.ca/Saiper-Timing-Aluminum-Synchronous-Printer/dp/B07MDH63GX/ref=sr_1_5?dchild=1&keywords=8mm+bore+gt2&qid=1627821975&sr=8-5)
4) **GT2 Timing Belt** : [Amazon.ca](https://www.amazon.ca/Printer-Timing-Teeth-Pulley-Wrench/dp/B08PKPK4D8/ref=sr_1_8?dchild=1&keywords=gt2+timing+belt&qid=1627821669&sr=8-8)
* Only the GT2 belt is needed from this kit, however it's often cheaper with the incorrect sized pulleys in a bundle
* Your desired stroke length plus about 200mm should be your minimum order length
5) **Bearings** 5x11x4mm : [Amazon.ca](https://www.amazon.ca/gp/product/B07CVBW44R/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
6) **MGN12H Rail and bearing** : [Amazon.ca](https://www.amazon.ca/Usongshine-guidage-lin%C3%A9aire-MGN12H-300mm/dp/B07XLL484J/ref=pd_sbs_201_1/139-0384147-0570541?_encoding=UTF8&pd_rd_i=B07XT8ZY9H&pd_rd_r=e7dc0ab7-e244-4a6c-ba42-59d7da76e03b&pd_rd_w=jhRlq&pd_rd_wg=zovAp&pf_rd_p=0ec96c83-1800-4e36-8486-44f5573a2612&pf_rd_r=YZGA61RD95B0E3H004ZA&refRID=YZGA61RD95B0E3H004ZA&th=1)
   - Minimum 250mm in length
   - Rail length = desired stroke + 180mm
   - Must be MGN12**H** rail
7) **Power Supply** : [Amazon.ca](https://www.amazon.ca/LEDENET-Adapter-Flexible-Lighting-5-52-5mm/dp/B078N5DC2J/ref=sr_1_9?keywords=24v+4a&qid=1636728925&sr=8-9)
   - Larger motors generally need more power. 
   - 180W -> 24V 6A
   - 140W -> 24V 6A
   - 100W -> 24V 4A
   - Choose the closest supply to the guidelines, **the ossm will still work**, but may be limited at maximum thrust force.
   - Ensure the power supply is fully enclosed (like a laptop power supply)
   - Ensure the power supply has the correct approvals for your location
8) **Metric Cap Screws** : [Amazon.ca](https://www.amazon.ca/Comdox-500pcs-Socket-Screws-Assortment/dp/B06XQLTLHP/ref=sr_1_12?dchild=1&keywords=metric+socket+head+cap+screw+kit&qid=1600747665&sr=8-12)
   - A kit like this will provide what's needed:
   - 4x m5x20
   - 1x m5x12 (can also use m5x20)
   - 10x m3x8
   - 2x m3x16
   - 8x m5 nuts
   - 8x m3 nuts
9) **ESP32 Development Board**  
  
   - [An official OSSM reference PCB](https://research-and-desire.myshopify.com/collections/all/products/ossm-reference-board) 
   - We do not currently have a best suggestion if you are not using a reference board, most generic development boards are the same
   - We have found that the 3.3v boards may miss steps at high speed, so please use a level shifter as well. 
   - To start working on the project, something like this [Adafruit board](https://www.adafruit.com/product/3405) is an excellent place to start.
   - Accessories required for prototyping include [Breadboard](https://www.amazon.ca/Breadboard-Solderless-Prototype-Distribution-Connecting/dp/B01EV6LJ7G/ref=sr_1_5?dchild=1&keywords=breadboard&qid=1627823170&sr=8-5) and [Dupont Jumpers](https://www.amazon.ca/120pcs-Multicolored-Breadboard-Arduino-raspberry/dp/B01LZF1ZSZ/ref=sr_1_5?dchild=1&keywords=dupont+jumper&qid=1627823220&sr=8-5)


## Official OSSM Wiring


![image](https://user-images.githubusercontent.com/43324815/150361448-80e9fdaf-4a8c-4054-a920-6eab9aa68678.png)


### OSSM PCB Connections

![image](https://user-images.githubusercontent.com/43324815/150355658-2ab2c53f-8da0-41ce-ad61-cfe9965b9ab2.png)


### GPIO Layout

![OSSM pinout](https://user-images.githubusercontent.com/12459679/152600401-80b986ea-6f5b-480d-ba74-5b5001079c1b.png)


## Non-Official OSSM Wiring

This should be a good start for the wiring of your OSSM! However, depending on your hardware mix settings or wiring may be different 

### OSSM PCB w/ TB6600 Stepper Driver

<img width="1028" alt="image" src="https://user-images.githubusercontent.com/43324815/159145946-a9960bba-c9bc-4717-b3b4-5b5c34b4a3d2.png">

### Servo Wiring

![image](https://user-images.githubusercontent.com/43324815/150361181-98c5375e-c517-4882-8e53-6cac407164b0.png)

### Stepper Wiring 

![wiring notes](https://github.com/KinkyMakers/OSSM-hardware/blob/44ab7a5deafa7dd3d66d521bb368959db542c164/Hardware/PCB/wiring%20notes%20800.png)

