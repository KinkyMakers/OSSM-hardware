
**Note**: The AIO version of this machine displayed in the pictures, including the mounting of the control board in the front with cables routing to the Motor in the back, is based on @thearmpit idea. The AIO Backpack Cover part in his design is covered by CC BY-NC-SA 4.0 license. The part used in this machine is **not** the same part, it is its own custom part, designed by me for this machine . It merely uses a similar design concept as his part. My AIO Cover part is not covered under CC BY-NC-SA 4.0 license.


# Capstan OSSM XL
![IMG20251109164233](Pictures/HG20/IMG20251109164233.jpg)
Revamp of the OSSM drive mechanism to use a capstan based version, enabling higher load use with a more powerful motor, namely the 60AIM40F.

A Capstan Drive is based on rope and a pulley. For a nice video explaining the mechanism, check this awesome video by Aaed Musa: [https://www.youtube.com/watch?v=MwIBTbumd1Q](https://www.youtube.com/watch?v=MwIBTbumd1Q "https://www.youtube.com/watch?v=MwIBTbumd1Q")

**Why Capstan?**

The belt driven Version has shown for myself and quite a few others limitations. At higher torques, it would sometimes slip. Bear in mind, it could be that is the result from build or other errors.

**Comparison to the standard OSSM**

Pro:

 - same form factor
 - as quiet or quieter
 - only 13cm of effective rail length loss (e.g. 450mm rail -> 32cm stroke)
 - 2.7 times stronger (32kg compared to 12kg)
 - same speed
 - no slipping
 - PitClamp compatible
 - compatible with all custom end effectors

Con:

 - harder to build
 - bigger motor -> more expensive (~110€ shipped incl. tax to germany)
 - more to print
 - in extreme use cases there may be slipping of the rail over time, is fixed by rerunning homing
 
This machine has served me very well for the past months now, I am personally very happy with it. Provide feedback an suggestions in the discord thread.

Thread MGN12H: https://discord.com/channels/559409652425687041/1395456804464623817
Thread HG20: https://discord.com/channels/559409652425687041/1437149434143182959
# MGN12H vs HG20
MGN12H rails and carriages get to their limit with 30cm+ cantilevers combined with heavy toys ~1.5kg and up. This results in louder operation, damaging of the bearings over time and deflection issues. So I recommend going with the HG20 Version if you plan to use big heavy toys.
Additionally, the HG Version comes with a number of improvements such as a bigger drum and better rope guiding for even smoother operation.

I do consider the MGN12H version to be outdated so I do recommend going with HG20 if you can afford the +~40€ in cost.
# AIO vs Normal
The AIO mod version mounts the control board in the front of the motor head. For the normal version there is no mounting at all, and no specific build instructions. I recommend building the AIO version, but if you want to make your own mounting solution, you can print the normal version.
# Pulley Versions
The bushing pulley is better. Make that one if you can find a bushing of the appropriate size and don't mind gluing with epoxy. It is better in every way. If you can't or decide against, the 3d printed pulley does its job, but it will deform over time under the constant force. You will need to replace it when that happens. Also only supports rail length up to 400mm.

# Rail Lengths
With the bushing pulley, it was tested for 450mm. Shorter is always possible. 500mm is probably doable as well, has not been tested. Even higher lengths I do not recommend.
# Rope
Use the rope I list (Liros DC pro 161). If you can't, try to source another dyneema dm20, sk99, sk75 max. diameter 1mm rope.

**DO NOT** use generic uhwmpe from aliexpress. Experiments from kind @neos have shown that the quality is shit. It stretches and breaks with load. It is essential that the rope stretches as little as possible. Lots of research has been done to find the correct rope. Stick to it. Or deviate at your own risk and cost.
# Printing
I have included .3mf file with configured settings. As for the AIO_cover part, you can try printing without supports as included. it's jsut bridges. If it gets too messy or doesn't work feel free to enable supports of course. Some files don't have .3mf files. Those are quite straight forward to print. I always recommend 6 walls 20% infill.
# BOM
see /3D printed parts/MGN12H or /3D printed parts/HG20 respecively

Finally, special thanks to everyone that helped me and gave suggestions when I hit a wall!
