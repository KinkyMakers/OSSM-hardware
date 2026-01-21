# OSSM - Open Source Sex Machine

**Maintained by [Research and Desire](https://researchanddesire.com), supported by the community.**

![OSSM Overview Image](Documentation/ossm/images/_images/OSSM%20Banner%20Image.webp)

## What is OSSM?

**OSSM** (pronounced like "awesome") is a user-friendly, open-source sex machine designed for everyday use. Whether you're curious about sex machines or looking to build your own, OSSM provides a powerful, customizable solution you can assemble at home.

OSSM uses a servo-powered belt-driven linear rail, enabling quiet operation, high torque, and software-defined stroke and depth control at speeds up to 1 meter per second.

### Performance Specifications

| Specification | Standard (20V DC) | High Power (36V DC) |
|---------------|-------------------|---------------------|
| Force output  | 32 lbs (14 kg)    | 50 lbs (22 kg)      |
| Stroke length | 8" (20 cm)        | 8" (20 cm)          |
| Rail size     | 350mm             | 350mm               |

### Why Build an OSSM?

- **Full control** over stroke length, depth, and speed through software
- **Quiet operation** suitable for shared living spaces
- **Customization options** through community-developed mods
- **Learning opportunities** in mechanics, electronics, and computing

## Quick Links

| Resource | Description |
|----------|-------------|
| [Documentation](https://docs.researchanddesire.com/ossm) | Complete build guides, hardware specs, and software reference |
| [R+D Store](https://researchanddesire.com) | Purchase motors, PCBs, wire harnesses, and complete kits |
| [KinkyMakers Discord](https://discord.gg/VtZcudpxT6) | Community discussion, build help, and mod development |
| [FAQs](https://docs.researchanddesire.com/ossm/guides/housekeeping/faqs) | Common questions about hardware, motors, printing, and control |

![Discord](https://img.shields.io/discord/559409652425687041)

---

## Building Your OSSM

For complete step-by-step instructions, see the [Build Guide](https://docs.researchanddesire.com/ossm/guides/getting-started/build-guide/step-0.0_introduction).

### Bill of Materials

For the complete parts list with supplier links, see [Required Tools and Parts](https://docs.researchanddesire.com/ossm/guides/getting-started/build-guide/step-1.0_required_tools_and_parts).

### Electronics

| Component | Description | Documentation |
|-----------|-------------|---------------|
| Motor | 57AIM30 "Gold Motor" | [Motor Documentation](https://docs.researchanddesire.com/ossm/Hardware/motor/standard-build-motor) |
| Reference Board | OSSM PCB or ESP32 Development Board | [Board Design](https://docs.researchanddesire.com/ossm/Hardware/PCB/board-design) |
| Remote | OSSM Wired Remote | [Remote Documentation](https://docs.researchanddesire.com/ossm/Hardware/standard-printed-parts/remote/introduction) |
| Wiring | JST-PH 2.0 4-Pin data cable and 16awg power wire | [Wiring Guide](https://docs.researchanddesire.com/ossm/Hardware/motor/wiring-gold-motor) |

**Power Supply:** 20-36V DC (5.5 x 2.1 Barrel Plug). A 24V 5A supply is recommended. Higher voltage (up to 36V) provides increased force.

> **Portable Option:** USB Power Banks capable of true 100W USB PD generally work well.
> - INIU Power Bank P63-E1 100W (tested, works)
> - INIU B62 Power Bank 65W (tested, powers down on high load)

### Printed Parts

For 3D printing settings and material recommendations, see [3D Printing Parts](https://docs.researchanddesire.com/ossm/guides/getting-started/build-guide/step-1.2_3d-printing-parts).

| Assembly | Parts Included | Documentation |
|----------|----------------|---------------|
| [Actuator](Printed%20Parts/Actuator/) | Body, Belt Tensioner, Threaded End Effector | [Actuator Docs](https://docs.researchanddesire.com/ossm/Hardware/standard-printed-parts/actuator/introduction) |
| [Remote](Printed%20Parts/Remote/) | Body, Knobs, Top Cover | [Remote Docs](https://docs.researchanddesire.com/ossm/Hardware/standard-printed-parts/remote/introduction) |
| [Toy Mounting](Printed%20Parts/Toy%20Mounting/) | Flange Base, Vac-U-Lock Adapters | [Toy Mounting Docs](https://docs.researchanddesire.com/ossm/Hardware/standard-printed-parts/toy-mounting/introduction) |
| [Mounting](Printed%20Parts/Mounting/) | PitClamp Mini Ring/Base, PCB Enclosure | [Mounting Docs](https://docs.researchanddesire.com/ossm/Hardware/standard-printed-parts/pitclamp/README) |
| [Stand](Printed%20Parts/Stand/) | 3030 Extrusion Base Components | [Stand Docs](https://docs.researchanddesire.com/ossm/Hardware/standard-printed-parts/stand/introduction) |

Experimental parts are developed in the [KinkyMakers Discord](https://discord.gg/wrENMKb3) `#ossm-print-testing` channel.

### Hardware Components

**GT2 Pulley**
- Qty 1: 8mm Bore, 20 Tooth, 10mm Width

**GT2 Timing Belt**
- Qty 1: 10mm Width, 500mm length

**MGN12H Rail + Bearing Block**
- Qty 1: Minimum 250mm, Suggested 350mm, Maximum 550mm
- Rail length = desired maximum stroke + 180mm
- Must be MGN**12H** (H = longer bearing block for stability, 12 = 12mm rail width)

**Ball Bearings**
- Qty 6: MR115-2RS 5x11x4mm

**Fasteners**
| Qty | Part |
|-----|------|
| 8 | M3x8 Socket Cap Head Bolt |
| 2 | M3x16 Socket Cap Head Bolt |
| 1 | M3x20 Socket Cap Head Bolt |
| 7 | M3 Hex Nut |
| 3 | M5x20 Socket Cap Head Bolt |
| 1 | M5 Hex Nut |
| 4 | M5x35 Socket Cap Head Bolt |
| 4 | M5 20mm Hex Coupling Nut (or M5 Hex Nut) |

Additional hardware is required for Stand, Mounting, and Remote assemblies. See the respective [Printed Parts](Printed%20Parts/) folders for details.

---

## Assembly

**Important:** The actuator rail direction is critical for pattern accuracy and safety. The proper orientation has the threaded end to the right when looking at the front face of the actuator body (the "M" side of the OSSM text on the cover).

Your rail should extend the threaded end first when booted. If this doesn't match your build's behavior, reverse your rail's printed hardware.

![Actuator Assembly](Documentation/ossm/Hardware/standard-printed-parts/_images/Exploded%20-%20Actuator%20Default.webp)

### Build Resources

| Resource | Description |
|----------|-------------|
| [Complete Build Guide](https://docs.researchanddesire.com/ossm/guides/getting-started/build-guide/step-0.0_introduction) | Step-by-step documentation with images |
| [OSSM Assembly Playlist](https://youtube.com/playlist?list=PLzSK7OAu3KNQsFo6WJGT8P28lfkD3xpps) | Video tutorials for each assembly step |
| [Complete Assembly - Follow Along Guide](https://www.youtube.com/watch?v=9lVobSEw_Uw) | Full 30-minute video walkthrough |

---

## Software

For firmware flashing and configuration, see the [Software Documentation](https://docs.researchanddesire.com/ossm/Software/getting-started/introduction).

| Resource | Description |
|----------|-------------|
| [Web Flasher](https://docs.researchanddesire.com/ossm/guides/getting-started/web-flasher) | Flash firmware directly from your browser |
| [PlatformIO Setup](https://docs.researchanddesire.com/ossm/Software/getting-started/PlatformIO) | Development environment for custom builds |
| [LED Status Guide](https://docs.researchanddesire.com/ossm/Software/getting-started/LED_Status) | Understanding indicator lights |
| [StrokeEngine](https://docs.researchanddesire.com/ossm/Software/motion/stroke-engine/introduction) | Motion control library documentation |

---

## Getting Help

- [User Guide](https://docs.researchanddesire.com/ossm/guides/getting-started/user-guide/introduction) - Operating your OSSM
- [Troubleshooting](https://docs.researchanddesire.com/ossm/guides/getting-started/user-guide/troubleshooting) - Common issues and solutions
- [Safety Information](https://docs.researchanddesire.com/ossm/guides/housekeeping/safety/introduction) - Important safety guidance
- [Discord Community](https://discord.gg/VtZcudpxT6) - Real-time community support

---

## Contributing

OSSM is open-source hardware under the [CERN Open Hardware Licence Version 2 - Strongly Reciprocal](LICENSE).

- [How to Become a Contributor](https://docs.researchanddesire.com/ossm/guides/contributing/how-to-become-a-contributor)
- [Forking the Repository](https://docs.researchanddesire.com/ossm/guides/contributing/forking)
- [Reporting Issues](https://docs.researchanddesire.com/ossm/guides/contributing/reporting-issues)
- [Roadmap](https://docs.researchanddesire.com/ossm/guides/contributing/roadmap)

---

## About

- [About Research and Desire](https://docs.researchanddesire.com/ossm/guides/housekeeping/about-research-and-desire)
- [About Kinky Makers](https://docs.researchanddesire.com/ossm/guides/housekeeping/about-kinky-makers)
- [Open Source Certification](https://docs.researchanddesire.com/ossm/guides/housekeeping/open-source/introduction)
