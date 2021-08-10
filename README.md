# OSSM - Open Source Sex Machine
OSSM (pronounced like "Awesome") is a user friendly every day sex machine for the people.

This project aims to help people curious about sex machines explore their interest. A second objective is optionally learning how mechanics, electronics, physics and computing are involved in youe sexual pleasure.

Please note that this is a _work in progress_ and we have attempted to keep compatibility with the current BOM going forward, but it's not guaranteed.

### Primary design goals:
- Compact
- Quiet
- Low cost
- 3D printable
- Easily sourced components
- Doesn't look like a giant machine

## Getting Started
There are a few hardware flavours to choose from, we've included user modified versions in case that fits your use case better!

[Build Instructions](https://github.com/KinkyMakers/OSSM-hardware/blob/master/Documentation/Assembly%20Instructions.pdf)

Join our Discord to be part of the discussion and get help with your build. https://discord.gg/MmpT9xE

### Software
We recommend using the ESP32 microcontroller. This code is still arduino IDE compatible but offers many times better performance and a nice internet dashboard you can use!
Control your OSSM at https://app.researchanddesire.com/ossm !

### Eagle PCB
Simple PCB to power an ESP32 (wifi enabled microcontroller) from 24V and breakout the pins for the OSSM control

### Mechanical design
The OSSM use a compact belt design with components that have become widely available due to 3D printing popularity.
It is driven by a Nema23 motor of **your** choosing, although we reccomend small integrated closed loop steppers for their cost to performance ratio.

<img src="https://user-images.githubusercontent.com/43324815/127772517-60358355-d0c8-42af-995c-cf83fd16199f.png" width="400" height="400">

## Bill Of Materials

We are calling this the reference build, when deviating from it please check compatability with existing BOM

1) **[3D Printed Parts](https://github.com/KinkyMakers/OSSM-hardware/tree/master/Hardware/Current%20OSSM)**
   - This has recently had significant changes
   - [Make sure to choose one of the options for the toy adapters](https://github.com/KinkyMakers/OSSM-hardware/tree/master/Hardware/Current%20OSSM/end%20effector%20options)
   - Mounting options are still something that need to be worked on
2) **NEMA23 Servo with 8mm shaft** : [Amazon.ca](https://www.amazon.ca/Integrated-Servo-Motor-IHSV57-30-10-3000rpm/dp/B081CVJHC7) | [JMC](https://www.jmc-motor.com/product/953.html)
   - *Avoid the StepperOnline version until we can furter test*
   - Make sure you get something with 8mm shaft max!
   - search around for the best deal for you - have found occasionally better deals on Aliexpress
3) **GT2 Pulley 8mm Bore 20 Tooth** : [Amazon.ca](https://www.amazon.ca/Saiper-Timing-Aluminum-Synchronous-Printer/dp/B07MDH63GX/ref=sr_1_5?dchild=1&keywords=8mm+bore+gt2&qid=1627821975&sr=8-5)
4) **GT2 Timing Belt** : [Amazon.ca](https://www.amazon.ca/Printer-Timing-Teeth-Pulley-Wrench/dp/B08PKPK4D8/ref=sr_1_8?dchild=1&keywords=gt2+timing+belt&qid=1627821669&sr=8-8)
* Only the GT2 belt is needed from this kit, however it's often cheaper with the incorrect sized pulleys in a bundle
5) **Bearings** 5x11x4mm : [Amazon.ca](https://www.amazon.ca/gp/product/B07CVBW44R/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
6) **MGN12H Rail and bearing** : [Amazon.ca](https://www.amazon.ca/Usongshine-guidage-lin%C3%A9aire-MGN12H-300mm/dp/B07XLL484J/ref=pd_sbs_201_1/139-0384147-0570541?_encoding=UTF8&pd_rd_i=B07XT8ZY9H&pd_rd_r=e7dc0ab7-e244-4a6c-ba42-59d7da76e03b&pd_rd_w=jhRlq&pd_rd_wg=zovAp&pf_rd_p=0ec96c83-1800-4e36-8486-44f5573a2612&pf_rd_r=YZGA61RD95B0E3H004ZA&refRID=YZGA61RD95B0E3H004ZA&th=1)
   - Minimum 250mm in length
   - Must be MGN12**H** rail
7) **Power Supply** : [Amazon.ca](https://www.amazon.ca/Signcomplex-Adapter-Transformers-Switching-Adaptor/dp/B079BJS3F4/ref=sr_1_3_sspa?dchild=1&keywords=24v+2a&qid=1600747030&sr=8-3-spons&psc=1&spLa=ZW5jcnlwdGVkUXVhbGlmaWVyPUExRk40ODRKMDlERlhZJmVuY3J5cHRlZElkPUEwMDQ1MDk1MVc0V1NUNlZMTUlMViZlbmNyeXB0ZWRBZElkPUEwODg0ODkyMlpNMVZKNjhQV0Y4RCZ3aWRnZXROYW1lPXNwX2F0ZiZhY3Rpb249Y2xpY2tSZWRpcmVjdCZkb05vdExvZ0NsaWNrPXRydWU=)
   - Recommended specifications; 24v 4A
   - Ensure the power supply is fully enclosed (like a laptop power supply)
   - Ensure the power supply has the correct approvals for your location
8)**Metric Cap Screws** : [Amazon.ca](https://www.amazon.ca/Comdox-500pcs-Socket-Screws-Assortment/dp/B06XQLTLHP/ref=sr_1_12?dchild=1&keywords=metric+socket+head+cap+screw+kit&qid=1600747665&sr=8-12)
   - A kit like this will provide what's needed, specific quantities TBD
9) **ESP32 Development Board**
   - We do not currently have a best suggestion, most generic development boards are the same
   - An official OSSM reference PCB is working its way through testing - this will hopefully be available before end of year 2021 
   - To start working on the project, something like this [Adafruit board](https://www.adafruit.com/product/3405) is an excellent place to start.
   - Accessories required for prototyping include [Breadboard](https://www.amazon.ca/Breadboard-Solderless-Prototype-Distribution-Connecting/dp/B01EV6LJ7G/ref=sr_1_5?dchild=1&keywords=breadboard&qid=1627823170&sr=8-5) and [Dupont Jumpers](https://www.amazon.ca/120pcs-Multicolored-Breadboard-Arduino-raspberry/dp/B01LZF1ZSZ/ref=sr_1_5?dchild=1&keywords=dupont+jumper&qid=1627823220&sr=8-5)

## Wiring Notes

This should be a good start for the wiring of your OSSM! However, depending on your hardware mix settings or wiring may be different 

### Servo Wiring

![image](https://user-images.githubusercontent.com/43324815/127773668-ebfa57bf-29da-4c6b-8785-8ae0f1ffbf3b.png)

### Stepper Wiring 

![wiring notes](https://github.com/KinkyMakers/OSSM-hardware/blob/44ab7a5deafa7dd3d66d521bb368959db542c164/Hardware/PCB/wiring%20notes%20800.png)

