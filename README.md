# OSSM - Open Source Sex Machine
OSSM (pronounced like "Awesome") is a user friendly every day sex machine for the people.

This project aims to help people curious about sex machines explore their interest. A second objective is optionally learning how mechanics, electronics, physics and computing are involved in your sexual pleasure.

Please note that this is a _work in progress_ and we have attempted to keep compatibility with the current BOM going forward, but it's not guaranteed.

*Primary design goals* are to make a machine that is Compact, Quiet, Moderate cost, 3D printable (no cutting/machining), High performance, flexible, Easily sourced components, Doesn't look like a giant machine.
<p align="center">
<img src="https://user-images.githubusercontent.com/12459679/200219198-df577cbc-8503-47af-bbc8-9cb65de7ea49.jpg" width="750">
</p>

## Getting Started

<h3><p align="center"><a href="FAQ.md">Read the Frequently Asked Questions</a></p></h3>

There are a few hardware flavours to choose from, we've included user modified versions in case that fits your use case better!

<h3><p align="center"><a href="https://github.com/KinkyMakers/OSSM-hardware/blob/master/Documentation/Assembly%20Instructions.pdf">Build Instructions</a></p></h3>

Join our Discord to be part of the discussion and get help with your build. We have a huge community of makers! https://discord.gg/MmpT9xE

<p align="center">
<img src="https://user-images.githubusercontent.com/12459679/200221900-9ee3337d-2c42-4a85-90ad-25ee921a2b87.png" width="750">
</p>

### Software

The software is available in this github repository, the software is written and compiled utilizing PlatformIO on Visual Studio Code. <a href="OSSM PlatformIO Readme.md">Reference for working with the code in PlatformIO here</a>

We recommend using the ESP32 microcontroller. This code is still arduino IDE compatible but offers many times better performance and a nice internet dashboard you can use!
Control your OSSM at https://app.researchanddesire.com/ossm !

### Eagle PCB
Simple PCB to power an ESP32 (wifi enabled microcontroller) from 24V and breakout the pins for the OSSM control

### Mechanical design
The OSSM use a compact belt design with components that have become widely available due to 3D printing popularity.
It is driven by a Nema23 motor of **your** choosing, although we reccomend small integrated closed loop servos for their silent operation and very high performance.
<p align="center">
<img src="https://user-images.githubusercontent.com/12459679/200219706-f93fa014-7fea-4f31-b5d0-d89d4e6ab130.png">
</p>

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
2) **IHSV57 NEMA23 Servo with 8mm shaft** : [Amazon.ca](https://www.amazon.ca/Integrated-Servo-Motor-IHSV57-30-10-3000rpm/dp/B081CVJHC7) | [JMC](https://www.jmc-motor.com/product/953.html)
   - *Avoid the StepperOnline version until we can further test*
   - Make sure you get something with **8mm** shaft.
   - There are *3* sizes of this motor.  100W = iHSV57-30-**10**, 140W = iHSV57-30-**14**, 180W = iHSV57-30-**18** 
   - Prefer motors with firmware version 6 (shown as `V60x`)
   - Search around for the best deal for you - reccommend searching "ihsv57" on Aliexpress.com and choosing the -10 -14 or -18
   - For details on picking the right motor for your use case - check this [FAQ](https://github.com/KinkyMakers/OSSM-hardware/blob/master/FAQ.md#q--what-strength-of--motor-do-i-need)
   - If you are using the 140W or180W version it is recommended to use a 10mm wide belt and pulley (see the next two items)
3) **GT2 Pulley 8mm Bore 20 Tooth, 10mm width** : [Amazon.ca](https://www.amazon.ca/Timing-Pulley-8mm-bore-10mm-belt-20-Tooth/dp/B09L7D7HHT)
4) **GT2 Timing Belt - 10mm width** : [Amazon.ca](www.amazon.ca/Timing-Meters-Creality-Anycubic-Printer/dp/B097T4DFM6/)
* Your desired stroke length plus about 200mm should be your minimum order length
5) **Bearings** 5x11x4mm : [Amazon.ca](https://www.amazon.ca/gp/product/B07CVBW44R/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
6) **MGN12H Rail and bearing** : [Amazon.ca](https://www.amazon.ca/Usongshine-guidage-lin%C3%A9aire-MGN12H-300mm/dp/B07XLL484J/ref=pd_sbs_201_1/139-0384147-0570541?_encoding=UTF8&pd_rd_i=B07XT8ZY9H&pd_rd_r=e7dc0ab7-e244-4a6c-ba42-59d7da76e03b&pd_rd_w=jhRlq&pd_rd_wg=zovAp&pf_rd_p=0ec96c83-1800-4e36-8486-44f5573a2612&pf_rd_r=YZGA61RD95B0E3H004ZA&refRID=YZGA61RD95B0E3H004ZA&th=1)
   - Minimum 250mm in length
   - Rail length = desired stroke + 180mm
   - Must be MGN12**H** rail
7) **Power Supply** : 24v 4-5A w/ 2.1mm DC plug - (need to update reference link)
   - Larger motors generally need more power. 
   - 180W -> 24V 5A
   - 140W -> 24V 4A
   - 100W -> 24V 4A
   - The OSSM will work even with small power supplies, but will be limited on maximum force
   - Ensure the power supply is fully enclosed (like a laptop power supply)
   - Ensure the power supply has the correct approvals for your location - This really helps ensure performance as well!
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
![JST Header](https://user-images.githubusercontent.com/12459679/226189433-db28dfc6-22ac-4fdb-8b45-afe0c3fa9a7b.png)



## Non-Official OSSM Wiring

This should be a good start for the wiring of your OSSM! However, depending on your hardware mix settings or wiring may be different 

### OSSM PCB w/ TB6600 Stepper Driver

<img width="1028" alt="image" src="https://user-images.githubusercontent.com/43324815/159145946-a9960bba-c9bc-4717-b3b4-5b5c34b4a3d2.png">

### Servo Wiring

![ServoWiring](https://github.com/NightmareSyndrome/OSSM-hardware/assets/131713378/783876cf-f5bf-4708-9b4f-bd89723713f9)

### Stepper Wiring 

![wiring notes](https://github.com/KinkyMakers/OSSM-hardware/blob/44ab7a5deafa7dd3d66d521bb368959db542c164/Hardware/PCB/wiring%20notes%20800.png)

