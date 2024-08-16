# Frequently Asked Questions

### Q: What is OSSM?

A: An OSSM is compact sex machine that can be customised to suit you.  Based around a NEMA23 pattern for electric motors it can be driven by stepper or best of all servo motors it uses a combination of 3D printed and off the shelf parts.

### Q: Why go to all this trouble?  Why don't you just buy a ready made machine?

A: The OSSM is open you are not tied to a specific company and it does a lot more than equivalently priced machines.  Best of all you can fully control the stroke speed and most importantly depth.  You can make patterns or even your own control software.  Maybe you want to add a stroker rather than a dildo, you can modifiy it to do that.  Not enough power? You can use a bigger motor up to a certain point.

### Q: What parts do I need?

A: Head over to [the README](./#bill-of-materials) - the BOM is maintained there.
  
### Q: I've been reading about motors and now I'm really confused
  
A: While you can theoretically use any Nema23 mount motor.  There are motors that work and some that work better.  Stepper motors are cheaper and noiser so the community recommends closed loop servo motors.  These are more efficient and more importantly easier to program.  

### Q:  What strength of motor do I need?
  
A: That depends on a couple of factors.  

- Vaginal or Anal use
- Size of toys
- Anticipated Speed
  
We don't have a lot of recorded data [Please help us with this!] however the general suggestions are;

- **NEW 57AIM30 (Gold Motor)**: Roughly equivalent to **180W** below, see [Gold Motor Info](https://github.com/KinkyMakers/OSSM-hardware/blob/master/Documentation/Gold-Motor.md) and BOM.
- **100w** IHSV57 Servo: Vaginal usage with medium size toys and Anal usage with smaller toys (10 lbs force)
- **140W** IHSV57 Servo: Vaginal usage with larger toys, Anal use with medium size toys (15 lbs force)
- **180w** IHSV57 servo works with a wide range of toys for vaginal or anal play, and really packs a punch (20 lbs force). However if you like seriously heavy fantasy dildos you might want to consider building a Squooter [see the discord] that uses and even a larger motor mount; the NEMA34 which has even more powerful motors.

A build with a servo motor will have a flat torque curve across the speed range. That's to say - it will push just as hard at the slow speeds as well as the fast.
  With a stepper, the most force will be at the slower speeds and the top speed of a stepper is much less than the servo we've chosen to work with.
  
### Q: Hold on, why does anal play need more powerful motors?

A: Butts have big powerful muscles! It's common for people to bear down, or clench when things are feeling really good. Also it can be pretty fun shoving big objects in there! It can take a big motor to handle a big toy in a poweful butt.

### Q: What sized 3D printer do I need?

A: The minimum bed size is 105mm x 105mm for the largest single piece. Approximately 125mm of height is required for the shorter vac-u-lock compatible adapter, the plate is significantly shorter.

### Q: What print material is best?

A: All of our testing has been with simple PLA, however if you do print it in an interesting material make sure to share it to the discord!

### Q: What is the infill percentage?

A: 30% has been found to work just fine with PLA for non flexible parts

### Q: How thick are the walls and top?

A: Recommend at least 3mm for non flexible parts.

### Q: What are these flexible parts and how should I print them?

A: There are mounting solutions that use clamps for the OSSM these need some flexiblity.  For these parts thinner walls 2mm, lower infill percentages and a gyroid pattern.

### Q: How do I mount toys onto the OSSM?

A: Aside from the motor this is second decision.  There is a vacc-u-lock compatible mount the double double and a plate mount for suction cup toys that has some points to tie a dildo down The OSSM Platten

### Q: What to I mount the machine on?  

A: Well there are a few designs available.  THere are standard pipe mounts for US/Canadian along with 80/20 rails that can be used like a construction set.  There are even manfrotto boom compatible adaptors.  It is really limited by you imagination.

### Q: How do I control my OSSM

A: It depends on how you are going to use it.  Basic OSSM control is via the wired remote and a more advanced remote is available from community members (M5 Remote).  If you want something specific you can create your own that is the great thing about open source hardware.

### Q: I heard somewhere you can "control via a web page?"  Can I get people to control my OSSM over the internet?

A: As of August 2023 you cannot control an OSSM over the internet.  In the past R&D offered a web page that allowed very basic control with speed and depth settings over wifi.  

### Q: X-Toy and funscript support.  Where is it?  

A: As of August 2023 there is a highly experimental X-toy firmware forked from an earlier version that requires endstops.  This code has significant issue and is not considered to be safe for use on humans. There are frequent pauses, it loses track of where it is and has a tendency to randomly extend to maximum depth. Development of an X-Toys firmware is happening but it is complicated.

### Q: Why is X-Toys/Funscripts/ButtplugIO so difficult?

A: Things are a lot more complicated once you dig down into them.

- The motion core does not yet support position streaming. This is in development and requires a complete redesign of the safety architecture as you don't want to accidentally stab yourself.
- From the experimental software we learnt that the latency margin is really thin, Bluetooth especially struggles here. This will be a challenge to overcome.
- The data models of both xtoys and Buttplug.io do not support complex data models as it would be required for OSSM out of the box. As scaling to the full length of OSSM is just not an option. There might be a workaround by emulating an other toy. But you won't be able to control all aspects of OSSM through this.

### Q: There is an OSSM reference board do I have to buy it to make an OSSM?

A: No you don't but it does make building an OSSM a lot easier and stops you having to solder anything.  

### Q: Powersupplies what size do I need?

A: That is linked to the size of motor.  The higher the wattage of motor the larger the powersupply you need.  For safety use a desktop power supply that is ideally double insulated. Power supplies from Mouser/Digikey/Conrad/RS etc are all typically more expensive than "no brand supplies" you get from AliExpress/eBay/Amazon. Experience has been mixed with unbranded supplies with occasional quality and longer term stability issues.  

- 100w JMC we currently suggest a 4A 24V Supply
- 140w /180w JMC we currently suggest a 6A 24V

### Q: How long should the H-rail be?

A: It is really up to you but a 350mm rail will give approximately 195mm of depth.  

### Q: What is the size of the 24mm Thread for the End Effector?

A: It is a M24x3 (this is the size on the machine, do a test print for your new End Effector, you may have to upsize by 3-5%)
