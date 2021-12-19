# ESP32_Battery_Analyzer
Firmware for ESP32 - based battery analyzer

Video about the analyzer: https://youtu.be/Q0dDcJXHCDk

Schematics is very simple and can be found in "schematics" folder.

How to use:
1) Assemble the circuit. Please use 10W or more resistor for the current limiting/measurement. In this firmware I assume that you use one for 0.22 Ohms, if you use other - please adjust #define in the sketch. You can use almost any bipolar NPN transistor, just make sure that it is powerful enough, so use big one.
As a display you should use something based on SSD1306 controller, I used 128x32px one.
2) Upload the sketch, open the serial port monitor
3) When ESP32 will connect to the Wifi, you'll able to see its IP address on the display and in the serial console. Open it with browser (just enter IP address of the ESP32 in the browser's address box)
4) Connect battery that you want to test (it will discharge it, so if it is non-rechargeable, it will be useless after the test!), enter desired discharge current and end voltage in the web page (after entering current and voltage, don't forget to press corresponding "submit" button each time!). After adjusting the values, press the "Run" button.
5) Wait until voltage will drop down to one that you specified, and test will be over
6) Download CSV file(s) from the ESP32 web page. If there is 0-1000 values in archive, just use first link. If there are more than 1000 values - use both link and glue up files to big one on the PC. This limitation comes from ESP32 firmware, if you'll try to download all in one piece - watchdog can be triggered and ESP can reboot. If you'll able to overcome this - please make a PR :)
7) Optional step - replace "." delimiter of decimal part in CSV files to the ",". This might be needed for your spreadsheet program.
8) Import the data to your spreadsheet program, build the graphs, analyze the data
9) Enjoy! 