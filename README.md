# Humid-Hot-Spot
A temperature, humidity and location tracking device.

Components:

-NodeMCU
-DHT-22
-DS-18B
-Neo-6M GPS Module
-A Battery
-Power Module
-PCB or connectors

Getting started with the project!

1. Download Arduino IDE and install it in the OS directory to avoid complications. Then download NodeMCU drivers from here (Select your OS accordingly):
https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers

2. Configure ESP8266 with the IDE through the following tutorial:
https://www.instructables.com/id/Quick-Start-to-Nodemcu-ESP8266-on-Arduino-IDE/

3. Copy the "Arduino" folder in this directory to "Your-Username/Documents/". 
The complete path would look like "Your-Username/Documents/Arduino/".

4. Restart Arduino IDE. Under "Tools" menu set Flash size to 2MB SPIFFS.

5. Connect NodeMCU to USB port and select the desired COM port under "Tools" > "Port". Example: COM1, COM2, COM3, COM4 etc.

6. Under "Tools" menu select SPIFFS sketch data upload and wait until it's completed. Make sure serial monitor is closed and NodeMCU is disconnected from PCB.

7. Finally press the Upload button (Symbol: Right Arrow) on the top left of the IDE to burn the code onto NodeMCU.

