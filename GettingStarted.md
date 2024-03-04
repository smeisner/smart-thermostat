# Getting Started

> NOTE: These steps have not been fully verified or validated yet. Please add any modifications to them!!

Tested on Debian 11 (Bullseye) and 12 (Bookworm)

## Steps to build environment:

1. Of sourse, update the system first:

`sudo apt update --allow-releaseinfo-change && sudo apt upgrade`

2. Install the prerequisite packages
  * git
  * curl
  * wget
  * python3-pip
  * python3.11-venv
  * apt-transport-https
  * gpg

`sudo apt install git curl wget python3-pip python3.11-venv apt-transport-https gpg`

3. Build the dev environment:

Install Visual Studio Code<br>
The following info was mostly got from https://code.visualstudio.com/docs/setup/linux

```
wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > packages.microsoft.gpg
sudo install -D -o root -g root -m 644 packages.microsoft.gpg /etc/apt/keyrings/packages.microsoft.gpg
sudo sh -c 'echo "deb [arch=amd64,arm64,armhf signed-by=/etc/apt/keyrings/packages.microsoft.gpg] https://packages.microsoft.com/repos/code stable main" > /etc/apt/sources.list.d/vscode.list'
rm -f packages.microsoft.gpg
```

The above will add the repository key locally. Next we need to install the VS Code package itself:<br>

`sudo apt install code`

Now install PlatformIO:<br>

`curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py`

Add `pio` to the path:<br>

`echo "export PATH=\$PATH:/home/$USER/.platformio/penv/bin" >> ~/.profile`

4. Install the udev rules found in the ESP-IDF

This will allow for recognizing the ESP32 USB JTAG device correctly.<br>
Source: [PlatformIO doc for 99-platformio-udev.rules](https://docs.platformio.org/en/stable/core/installation/udev-rules.html)

  * `curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules`
  * Restart udev: `sudo service udev restart`

5. Add user to the proper groups
  * `sudo usermod -a -G dialout $USER`
  * `sudo usermod -a -G plugdev $USER`

6. Clone the thermostat repo
  * `git clone https://github.com/smeisner/smart-thermostat`

In the cloned repo, go into the 'app' folder and open the VS Code workspace smart-thermostat.code-workspace. With the included platformio.ini and the board definition files, you should be able to compile and flash to an ESP32-S3:

```
code smart-thermostat.code-workspace
```

If the above fails to launch VSCode, go to the system menu and select Visual Studio Code under Development. Then File, Open Workspace from File and navigate to the `smart-thermostat.code-workspace` file in the `app` folder.

In VS Code install the PlatformIO IDE extension (you should be prompted to add it). This will lead to installing many packages the first time.ny packag needed by PlatformIO and the project will be installed. Once you see `Project has been successfully updated!`, all extra packages have been installed.

7. Plug in an ESP32-S3 based thermostat and try a build / flash via VSCode / PlatformIO!!


## Hints for setup & install

* Set up git properly to do pulls & commits
```
$ git config --global user.name "John Doe"
$ git config --global user.email johndoe@example.com
```
* Be sure sym link for python invokes python3 (I am using 3.9). Check "`which python`".


Notes:
a. Any PSRAM will be disabled. This is due to a pin conflicts on the MCU.
b. At least 8MB is required on the ESP32-S3 SOC. Adjust the board\_build.partitions in the platformio.ini as necessary.

### Components / Libraries used

***

LovyanGFX - Graphics driver for the MSP3218 TFT display and touch screen
lvgl - Menuing system layered on top of LovyanGFX to generate screens and menus
micro-timezonedb - Timezone database support to configure any timezone setting
Smoothed - Tool to automatically average values over a sample period

DFRobot\_AHT20 : imported class library to support the AHT20 sensor (much simpler and reliable than Adafruit's)

SquareLine Studio is used to design and develop the lvgl menuing system & screens.

### Debugging / flashing

***

The board includes a SWD port, but so far using the built in USB controller in the S3 has worked fine. This is also true for flashing new code over the USB port. The device will show up on the USB bus usually as `/dev/ttyACM0`

There is also a 4 pin UART port on the board. This is used for serial comms. All serial.print statements end up here, as well as system debug information. In the state machine loop, the USB port is checked for input (as in the user typing on the serial console). Only a few commands are supported right now:
reset
scan
temp
The method implemented is very crude, so be careful!

`/dev/ttyUSBx` is the UART
`/dev/ttyACMx` is the ESP32

<br>

### Resources/Links:

***

Install VS Code

* Download .deb file
    `wget https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-x64`
    `sudo apt install ./code_1.79.0-1686149120_amd64.deb`

Install PlatformIO

* [https://platformio.org/install](https://platformio.org/install)
* Install as an extension inside Visual Studio Code
* Currently installed: PlatformIO IDE v3.2.0

Access Thermostat web interface

* [http://thermostat.local/](http://thermostat.local/)

ESP32 Arduino Matter port

* [https://github.com/Yacubane/esp32-arduino-matter](https://github.com/Yacubane/esp32-arduino-matter)
* [Announcement] [https://www.reddit.com/r/esp32/comments/yfq5sy/i_have_created_matter_library_for_esp32_running/](https://www.reddit.com/r/esp32/comments/yfq5sy/i_have_created_matter_library_for_esp32_running/)
* Download zip: `https://github.com/Yacubane/esp32-arduino-matter/releases/download/v1.0.0-beta.5/esp32-arduino-matter.zip`
    `cd smart-thermostat/app/lib`
    `gunzip "~/esp32-arduino-matter.zip"`

SquareLine Studio

* [https://squareline.io/](https://squareline.io/)

DFRobot AHT20 library

* [https://github.com/DFRobot/DFRobot_AHT20](https://github.com/DFRobot/DFRobot_AHT20)

LD2410 uWave Human Presence sensor

* [https://github.com/ncmreynolds/ld2410](https://github.com/ncmreynolds/ld2410)

PlatformIO documentation

* [https://docs.platformio.org/en/stable/what-is-platformio.html](https://docs.platformio.org/en/stable/what-is-platformio.html)

NTP Support library

* [https://mischianti.org/2020/08/08/network-time-protocol-ntp-timezone-and-daylight-saving-time-dst-with-esp8266-esp32-or-arduino/](https://mischianti.org/2020/08/08/network-time-protocol-ntp-timezone-and-daylight-saving-time-dst-with-esp8266-esp32-or-arduino/)

Arduino ESP32 doc

* [https://docs.espressif.com/_/downloads/arduino-esp32/en/latest/pdf/](https://docs.espressif.com/_/downloads/arduino-esp32/en/latest/pdf/)


#### FreeRTOS

***

FreeRTOS and Task Priorities

* [https://www.freertos.org/RTOS-task-priority.html](https://www.freertos.org/RTOS-task-priority.html)

Amazon FreeRTOS User Guide

* [https://thingspace.verizon.com/content/dam/thingspace-portal/files/pdf/CloudConectors_Amazon_freertos-ug.pdf](https://thingspace.verizon.com/content/dam/thingspace-portal/files/pdf/CloudConectors_Amazon_freertos-ug.pdf)

Multitasking on ESP32 with Arduino and FreeRTOS

* [https://savjee.be/blog/multitasking-esp32-arduino-freertos/](https://savjee.be/blog/multitasking-esp32-arduino-freertos/)

Implement Freertos With Arduino Ide On Esp32

* [https://www.electromaker.io/project/view/implement-freertos-with-arduino-ide-on-esp32-1](https://www.electromaker.io/project/view/implement-freertos-with-arduino-ide-on-esp32-1)

ESP32 Interrupts and Timers with PIR Sensor using Arduino IDE

* [https://microcontrollerslab.com/esp32-interrupts-timers-pir-sensor-arduino-ide/](https://microcontrollerslab.com/esp32-interrupts-timers-pir-sensor-arduino-ide/)


#### Misc Hardware Info

***

ESP32 Espressif Hardware page

* [https://www.espressif.com/en/products/modules](https://www.espressif.com/en/products/modules)
* [https://www.espressif.com/en/products/socs/esp32-s3](https://www.espressif.com/en/products/socs/esp32-s3)

ESP32 device with thread (matter) support?

* [https://www.reddit.com/r/esp32/comments/1079h3a/esp32_device_with_thread_matter_support/](https://www.reddit.com/r/esp32/comments/1079h3a/esp32_device_with_thread_matter_support/)

Wifi management tricks

* [https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/](https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/)

Wifi connect using lvgl

* [https://github.com/xpress-embedo/ESP32/blob/master/ConnectToWiFi/src/main.cpp](https://github.com/xpress-embedo/ESP32/blob/master/ConnectToWiFi/src/main.cpp)
* [https://www.youtube.com/watch?v=X-Xv36ZO460](https://www.youtube.com/watch?v=X-Xv36ZO460)
* [https://github.com/pjaos/mgos_esp32_littlevgl_wifi_setup/blob/master/src/setup_wifi/setup_wifi_gui.c](https://github.com/pjaos/mgos_esp32_littlevgl_wifi_setup/blob/master/src/setup_wifi/setup_wifi_gui.c)

Article on PSRAM

* [https://thingpulse.com/esp32-how-to-use-psram/](https://thingpulse.com/esp32-how-to-use-psram/)

Touchscreen Controller (XPT2046)

* [https://github.com/PaulStoffregen/XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen)

Switching Regulator Noise Reduction with an LC Filter

* [https://www.analog.com/media/en/technical-documentation/tech-articles/switching-regulator-noise-reduction-with-an-lc-filter.pdf](https://www.analog.com/media/en/technical-documentation/tech-articles/switching-regulator-noise-reduction-with-an-lc-filter.pdf)

I2C Scanner

* [https://learn.adafruit.com/adafruit-esp32-s3-feather/i2c-scan-test](https://learn.adafruit.com/adafruit-esp32-s3-feather/i2c-scan-test)

RCWL-0516 Range (Historical as this sensor is no longer planned for fab)

* [https://github.com/jdesbonnet/RCWL-0516/issues/11#issuecomment-771810294](https://github.com/jdesbonnet/RCWL-0516/issues/11#issuecomment-771810294)
* [https://github.com/jdesbonnet/RCWL-0516/issues/22](https://github.com/jdesbonnet/RCWL-0516/issues/22)


#### ESP32-S3 reference designs

***

* [https://learn.adafruit.com/assets/110822](https://learn.adafruit.com/assets/110822)
* [https://easyeda.com/editor#id=d056f47be7564dccbc8593766bcbeab5](https://easyeda.com/editor#id=d056f47be7564dccbc8593766bcbeab5)
* [https://easyeda.com/editor#id=4c9a270201564f79a8d898b16cb32a4a|030d87584b3241f4acb42f6090e3f78e|ad3bd37529b84bd0b1b817e14244c56d|10bc426910154bc18a781dce0e926303|ff30c5c9faf3437b93b7cbbef5ea03e9](https://easyeda.com/editor#id=4c9a270201564f79a8d898b16cb32a4a%7C030d87584b3241f4acb42f6090e3f78e%7Cad3bd37529b84bd0b1b817e14244c56d%7C10bc426910154bc18a781dce0e926303%7Cff30c5c9faf3437b93b7cbbef5ea03e9)
* [https://oshwlab.com/thecharge/esp-32-s3-wroom-1-board-redesign](https://oshwlab.com/thecharge/esp-32-s3-wroom-1-board-redesign)
* [https://www.reddit.com/r/PrintedCircuitBoard/comments/110phjq/repostpcb_review_an_iot_playground_board_based_on/](https://www.reddit.com/r/PrintedCircuitBoard/comments/110phjq/repostpcb_review_an_iot_playground_board_based_on/)


#### USB Port sourcing (for PCB fab)

***

* [https://www.lcsc.com/product-detail/USB-Connectors_G-Switch-GT-USB-7010ASV_C2988369.html](https://www.lcsc.com/product-detail/USB-Connectors_G-Switch-GT-USB-7010ASV_C2988369.html)
* [https://www.jae.com/en/topics/detail/id=92529](https://www.jae.com/en/topics/detail/id=92529)
* [https://www.mouser.com/ProductDetail/JAE-Electronics/DX07S016JA1R1500?qs=GBLSl2Akirucb2YMMGCxCQ%3D%3D](https://www.mouser.com/ProductDetail/JAE-Electronics/DX07S016JA1R1500?qs=GBLSl2Akirucb2YMMGCxCQ%3D%3D)


#### Google Sheets

***

* Sheet to determine GPIO pin usage
   * [https://docs.google.com/spreadsheets/d/10CfVb4AoQPJP5FjagokVNWyHDSV7czo9InDzF5tszXg/edit#gid=0](https://docs.google.com/spreadsheets/d/10CfVb4AoQPJP5FjagokVNWyHDSV7czo9InDzF5tszXg/edit#gid=0)
* BOM Analysis
   * [https://docs.google.com/spreadsheets/d/14w7VYzvXJafxjo8qtsvuFqKP_Zvz_vr7KMgLZYts2Cs/edit?pli=1#gid=310427562](https://docs.google.com/spreadsheets/d/14w7VYzvXJafxjo8qtsvuFqKP_Zvz_vr7KMgLZYts2Cs/edit?pli=1#gid=310427562)


## Sample output from build (using ProxMox, passing thru the ESP32 USB device)
```
*  Executing task in folder SmartThermostat: platformio run --target upload 

Processing esp32dev (platform: espressif32; board: esp32-s3-thermostat; framework: arduino)
----------------------------------------------------------------------------------------------------------------
Tool Manager: Installing platformio/tool-mkspiffs @ ~2.230.0
Downloading  [####################################]  100%
Unpacking  [####################################]  100%
Tool Manager: tool-mkspiffs@2.230.0 has been installed!
Tool Manager: Installing platformio/tool-mklittlefs @ ~1.203.0
Downloading  [####################################]  100%
Unpacking  [####################################]  100%
Tool Manager: tool-mklittlefs@1.203.210628 has been installed!
Tool Manager: Installing platformio/tool-mkfatfs @ ~2.0.0
Downloading  [####################################]  100%
Unpacking  [####################################]  100%
Tool Manager: tool-mkfatfs@2.0.1 has been installed!
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32-s3-thermostat.html
PLATFORM: Espressif 32 (6.2.0) > Espressif ESP32-S3-Thermostat (16 MB QD, No PSRAM)
HARDWARE: ESP32S3 240MHz, 320KB RAM, 16MB Flash
DEBUG: Current (esp-builtin) On-board (esp-builtin) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - framework-arduinoespressif32 @ 3.20008.0 (2.0.8) 
 - tool-esptoolpy @ 1.40501.0 (4.5.1) 
 - tool-mkfatfs @ 2.0.1 
 - tool-mklittlefs @ 1.203.210628 (2.3) 
 - tool-mkspiffs @ 2.230.0 (2.30) 
 - toolchain-riscv32-esp @ 8.4.0+2021r2-patch5 
 - toolchain-xtensa-esp32s3 @ 8.4.0+2021r2-patch5
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ deep, Compatibility ~ soft
Found 37 compatible libraries
Scanning dependencies...
Dependency Graph
|-- LovyanGFX @ 1.1.7
|-- Smoothed @ 1.2.0
|-- micro-timezonedb @ 1.0.2
|-- lvgl @ 8.3.7
|-- Wire @ 2.0.0
|-- Preferences @ 2.0.0
|-- ESPmDNS @ 2.0.0
|-- Update @ 2.0.0
|-- WebServer @ 2.0.0
|-- WiFi @ 2.0.0
Building in release mode
Retrieving maximum program size .pio/build/esp32dev/firmware.elf
Checking size .pio/build/esp32dev/firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [===       ]  33.4% (used 109528 bytes from 327680 bytes)
Flash: [====      ]  42.8% (used 1431745 bytes from 3342336 bytes)
Configuring upload protocol...
AVAILABLE: cmsis-dap, esp-bridge, esp-builtin, esp-prog, espota, esptool, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa
CURRENT: upload_protocol = esptool
Looking for upload port...
Auto-detected: /dev/ttyACM0
Uploading .pio/build/esp32dev/firmware.bin
esptool.py v4.5.1
Serial port /dev/ttyACM0
Connecting...
Chip is ESP32-S3 (revision v0.1)
Features: WiFi, BLE
Crystal is 40MHz
MAC: f4:12:fa:ee:3f:e4
Uploading stub...
Running stub...
Stub running...
Changing baud rate to 921600
Changed.
Configuring flash size...
Flash will be erased from 0x00000000 to 0x00003fff...
Flash will be erased from 0x00008000 to 0x00008fff...
Flash will be erased from 0x0000e000 to 0x0000ffff...
Flash will be erased from 0x00010000 to 0x0016dfff...
Compressed 15040 bytes to 10333...
Writing at 0x00000000... (100 %)
Wrote 15040 bytes (10333 compressed) at 0x00000000 in 0.2 seconds (effective 498.7 kbit/s)...
Hash of data verified.
Compressed 3072 bytes to 146...
Writing at 0x00008000... (100 %)
Wrote 3072 bytes (146 compressed) at 0x00008000 in 0.0 seconds (effective 543.8 kbit/s)...
Hash of data verified.
Compressed 8192 bytes to 47...
Writing at 0x0000e000... (100 %)
Wrote 8192 bytes (47 compressed) at 0x0000e000 in 0.1 seconds (effective 737.3 kbit/s)...
Hash of data verified.
Compressed 1432112 bytes to 828286...
Writing at 0x00010000... (1 %)
Writing at 0x00018cb1... (3 %)
Writing at 0x00020829... (5 %)
Writing at 0x0002946b... (7 %)
Writing at 0x00032cc9... (9 %)
Writing at 0x0003fa80... (11 %)
Writing at 0x00050787... (13 %)
Writing at 0x000605a9... (15 %)
Writing at 0x00070969... (17 %)
Writing at 0x0007c4d1... (19 %)
Writing at 0x0008ab5a... (21 %)
Writing at 0x00093f36... (23 %)
Writing at 0x00099473... (25 %)
Writing at 0x0009ee0f... (27 %)
Writing at 0x000a445d... (29 %)
Writing at 0x000a9b47... (31 %)
Writing at 0x000aeb6f... (33 %)
Writing at 0x000b3e61... (35 %)
Writing at 0x000b8fb1... (37 %)
Writing at 0x000be0ab... (39 %)
Writing at 0x000c32ba... (41 %)
Writing at 0x000c8eae... (43 %)
Writing at 0x000ced8c... (45 %)
Writing at 0x000d4afd... (47 %)
Writing at 0x000da61a... (49 %)
Writing at 0x000dfca6... (50 %)
Writing at 0x000e56f9... (52 %)
Writing at 0x000ea6d0... (54 %)
Writing at 0x000ef6bb... (56 %)
Writing at 0x000f44b6... (58 %)
Writing at 0x000f9419... (60 %)
Writing at 0x000fe34e... (62 %)
Writing at 0x00103267... (64 %)
Writing at 0x001083ba... (66 %)
Writing at 0x0010d398... (68 %)
Writing at 0x00112d03... (70 %)
Writing at 0x00118394... (72 %)
Writing at 0x0011d177... (74 %)
Writing at 0x00122281... (76 %)
Writing at 0x00127165... (78 %)
Writing at 0x0012c6ad... (80 %)
Writing at 0x00131b31... (82 %)
Writing at 0x0013787c... (84 %)
Writing at 0x0013ce67... (86 %)
Writing at 0x001424a3... (88 %)
Writing at 0x0014a6b3... (90 %)
Writing at 0x00153576... (92 %)
Writing at 0x00158e7c... (94 %)
Writing at 0x0015f0e6... (96 %)
Writing at 0x00164997... (98 %)
Writing at 0x0016a73c... (100 %)
Wrote 1432112 bytes (828286 compressed) at 0x00010000 in 9.9 seconds (effective 1156.4 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
========================================= [SUCCESS] Took 28.32 seconds =========================================
 *  Terminal will be reused by tasks, press any key to close it. 
```
