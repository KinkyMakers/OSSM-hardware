OSSM has been developed in PlatformIO.  For people used to the Arduino IDE, moving to PlatformIO can seem daunting.  Trust us that it is simpler than it looks to operate and works far better than the very basic Arduino IDE as the project grows and changes.  

This is particularly important when it comes to managing all the libraries and dependencies. This gets very complex, very quickly. Earlier builds of OSSM it was sort of possible to mash it into the Arduino IDE and get a compile happening.  It has now past the point when it is easier to teach you to use PlatformIO than it is to repackage OSSM for the Arduino at each release. 

Why? More time for coding features and easier collaboration! OSSM is moving fast. Once you get used to VSCode there are some other great features such automatic downloads from Github and linting (autocomplete for code). 

1. Start by installing VSCode and the PlatformIO extension. There are plenty of guides for this on the web.  

2. Restart VSCode wait until PlatformIO has started and look on the sidebar for this button.

![image](https://user-images.githubusercontent.com/93972925/156351539-54a612fd-0b9b-46cb-9e86-2bab2eb65418.png)

3. Click "Open"

![image](https://user-images.githubusercontent.com/93972925/156351747-962b3f88-e07f-4b68-8da9-99085e6ee636.png)

4. This will pop up 

![image](https://user-images.githubusercontent.com/93972925/156351872-008e57a4-5e65-40f8-b65c-b663318317d8.png)

5. Click "Open Project" and search for the OSSM directory that has platformIO.ini in it

![image](https://user-images.githubusercontent.com/93972925/156352091-867148c0-e9bd-47fa-a4e9-7c99c8fce5b1.png)

![image](https://user-images.githubusercontent.com/93972925/156352222-e3c6c412-248e-40d5-9e7c-d3a895dd0db7.png)

6. In the src directiory you should find main.cpp, click on that 

![image](https://github.com/KinkyMakers/OSSM-hardware/assets/33324586/4a687ec9-8110-4500-8ee7-f51c692e2403)

7. Near the bottom of the window look for a tick (compile) and an arrow (upload) 

![image](https://user-images.githubusercontent.com/93972925/156352661-123d795b-342c-4f83-82c3-ac54d51d7e2b.png) ![image](https://user-images.githubusercontent.com/93972925/156352700-12fad91c-a8d2-48fa-97cc-ee088d0ff219.png)

8. You can then compile or skip that step and upload directly to your board.  

What if that doesn't work?  Check the debug screen but chances are you have not selected the right Com port for the OSSM board or the wrong board for the project.  

9. COM or /DEV/TTY will vary from machine to machine. It is set here in platformIO 

![image](https://user-images.githubusercontent.com/93972925/156354154-c71755e4-d19f-4387-bceb-743403777711.png)

11. The reference board for OSSM has an embedded "Espressif ESP32 Dev Module".

12. If the project fails to build and the upload fails DON'T PANIC! Chances are there has been some kind of external library update.  Try changing the following lines in platformIO.ini in the root of the project 

"platform = espressif32"   to "platform = espressif32@3.5.0" 
