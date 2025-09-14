# eM4_clock
Arduino 12 LED ring clock firmware using NTP server, emulating eM4 wallbox HMI effects and colors.

## Target device
MakerGO ESP32-C3 Super Mini

## Pin configuration
WS2812B LED control pin = GPIO2
Buzzer control pin = GPIO3

## First steps
- Open Arduino IDE and make sure, the following libaries are installed: Adafruit_NeoPixel, ArduinoJson
- Open the eM4_clock.ino file, select the board "MakerGO ESP32 C3 SuperMini"
- Connect the target device, select the right COM port and upload the firmware
- After flashing and restarting the target device, it will open an access point with the name "eM4ClockConfig". Connect to the access point, open a browser and enter the IP address 192.168.4.1 to access the web ui
- On the web ui you can enter your wifi credentials, choose the right time zone (default is central european summer time), configure the LED brightness and color theme (ABL is the best) and you can choose if you want the buzzer sound on every hour
- After the configuration is done, press the "Save" button and wait for the restart of the device. It will now connect to your wifi credentials, connect to the NTP server pool.ntp.org and show the time (hours and minutes) on the LED ring

## Parts needed
- MakerGO ESP32-C3 Super Mini
- 12x WS2812B on seperate PCBs 1x1cm
- copper wire
- any analog buzzer
- 3D printing parts from https://www.printables.com/model/1412924-em4-clock-mod

## Assembly
See https://www.printables.com/model/1412924-em4-clock-mod
