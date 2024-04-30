**New Proposed Standard Servo Motor**

We have been testing a new motor for consideration as the build standard. It has the same mounting pattern and shaft diameter.
Gold motor  (57AIM30)

Advantages:
- Cheaper (20%)
- Smaller (40% length of 180W standard)
- Lighter (50% weight of 180W)
- Higher peak force (30% higher in real-world test)
- Better over-torque handling - can be configured to reduce torque output instead of disable like current 180w servo

Disanvantages:
- Lower top speed (top speed exceeds standard code speed limit)
- More efficient (this causes more regenerated power in some motion conditions which requires additional capacitance and handling on the PCB)
- Digital comms over RS485 (Not worse than current RS232, just current boards don't have the needed adapter)
- Requires software setup to see full benefits (default FW settings disable motor in over-torque condition
- Requires additional capacitance (recommend 1500uF) - see mounting below

![OSSM Gold Motor Wiring](https://github.com/KinkyMakers/OSSM-hardware/assets/12459679/10072632-6e04-495e-b95d-b963d1662924)

![IMG_1049](https://github.com/KinkyMakers/OSSM-hardware/assets/12459679/7bec39aa-364f-446a-8b29-4f9390e9d71e)

![image](https://github.com/KinkyMakers/OSSM-hardware/assets/12459679/37d83251-a305-4a17-a2c6-f16c2b8f5547)

![OSSM Capacitor view](https://github.com/KinkyMakers/OSSM-hardware/assets/12459679/edfe7a90-74a8-4c3e-a55a-6b22099aafb8)
