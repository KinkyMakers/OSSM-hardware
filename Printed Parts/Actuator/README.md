# Actuator
<p align="center" >
    <img src="_images/Overview%20-%20Actuator%20Default.png" width="50%">
</p>

## Printing
**Actuator Standard Print Settings:**  

    6 Walls
    Infill type: Cross Hatch (or gyroid) 15%
    Supports: None
 - **Filament estimate:** 200g
 - **Recommended filament:** PLA+
 - All parts are intended to print in the provided file default orientation. 
   - **Printing threaded components in other orientations will cause structural weakness**
     - For 24mm Clamping Thread, use 2 raft layers to promote better bed adhesion.
     - If you have issues with the 24mm Clamping Thread, use [Non-standard](Non-standard/) 24mm Thread and Belt Clamp files. 
       - (Qty 2) M3x8
       - (Qty 1) M3x16
       - (Qty 3) M3 Hex Nut

**Printed parts:**

    (Qty 1) Body - Bottom
    (Qty 1) Body - Middle
    (Qty 1) Body - Cover
    (Qty 1) Belt Tensioner
    (Qty 1) 24mm Clamping Thread - End Effector
    (Qty 1) 24mm Clamping Thread - Belt Clamp
    (Qty 2) 24mm Nut - 5 Sided

<!-- ### [Additional Documentation](3030%20Extrusion%20Base/README.md)  -->

## Bill Of Materials

### Default Actuator Hardware
**GT2 Pulley** 

    8mm Bore, 20 Tooth, 10mm Width
**GT2 Timing Belt** 

    10mm Width, 500mm length
**MGN12H Rail + Bearing Block**

    Minimum 250mm
    Suggested 350mm
    Maximum 550mm

Rail length = desired maximum stroke + 180mm  
Must be MGN**12H** rail.  
H is a longer bearing block than C for greater stability. 12 indicates 12mm rail width.

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

## Assembly
![](_images/Exploded%20ALT%20-%20Actuator%20Default.gif)

|![](_images/Exploded%20-%20Actuator%20Default.png) | ![](_images/Exploded%20ALT%20-%20Actuator%20Default.png) |
| ---- | ---- |

(Click to enlarge)

### Belt Routing

| Tensioner | Body | Belt Clamp |
| ----- | ----- | ---- |
| ![](_images/Cut%20View%20-%20Belt%20Routing%20Rear.png) | ![](_images/Cut%20View%20-%20Belt%20Routing%20Middle.png) | ![](https://github.com/KinkyMakers/OSSM-hardware/blob/master/Printed%20Parts/Actuator/_images/Cut%20View%20-%20Belt%20Routing%20Front.png) |

(Click to enlarge)
