# Getting Started

> NOTE: These steps have not been fully verified or validated yet. Please add any modifications to them!!

So far, all dev has been done on Debian 11 (Bullseye)

## Steps to build environment:

1. Of sourse, update the system first:
    * `sudo apt update --allow-releaseinfo-change && sudo apt upgrade`
2. Install the following packages
    a. git
    b. curl
    c. python3-pip
3. Now start building up the dev environment:
    a. VS Code from .deb [see hints below]
    b. PlatformIO extension [see hints below]
    c. Add `pio` to the path: `echo "export PATH=\$PATH:/home/$USER/.platformio/penv/bin" >> ~/.profile`
4. Install the udev rules found in the ESP-IDF:
    * `~/.platformio/penv/lib/python3.9/site-packages/platformio/assets/system/99-platformio-udev.rules`
    * [PlatformIO doc for 99-platformio-udev.rules](https://docs.platformio.org/en/stable/core/installation/udev-rules.html)
    * Alternative:
    * `curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules`
    * Be sure to restart udev: `sudo service udev restart`
5. Add user to the proper groups
    * `sudo usermod -a -G dialout $USER`
    * `sudo usermod -a -G plugdev $USER`
6. Clone the thermostat repo
    a. `git clone https://github.com/smeisner/smart-thermostat`
7. Plug in an ESP32-S3 based thermostat and try a build / flash!!

## Hints for setup & install

* Set up git properly to do pulls & commits
* Be sure sym link for python invokes python3 (I am using 3.9). Check "`which python`".
* The file wifi-credentials.h is missing and is currently required to build. Temporarily, it contains the SSID & passpharse to start wifi with. For now, manually create it. Eventually it will be kept in the NVS.

```
const char *hostname = "thermostat";
const char *wifiSsid = "your-ssid";
const char *wifiPass = "your-passphrase";
```

* Install VS Code
    * Manually via copying .deb file
        * [Download .deb file] [`wget https://code\.visualstudio\.com/sha/download?build=stable&os=linux\-deb\-x64\`]
        * Install it: `sudo apt install ./code\_1.79.0-1686149120\_amd64.deb`
    * [Recommended] Add repo to apt sources.list and install with apt. Add the following to `/etc/apt/sources.list.d/vscode.list`.
        * Then, `sudo apt update && sudo apt install code`

```
### THIS FILE IS AUTOMATICALLY CONFIGURED ###
# You may comment out this entry, but any other modifications may be lost.
deb [arch=amd64,arm64,armhf] http://packages.microsoft.com/repos/code stable main
```

* Installing PlatformIO
    * This will also install the ESP-IDF.
    * [A Quick script-based method can be found here](https://docs.platformio.org/en/stable/core/installation/methods/installer-script.html)
    * The "Super-Quick (macOS / Linux)" method worked fine for me

In the cloned repo, go into the 'app' folder and open the VS Code workspace smart-thermostat.code-workspace With the included platformio.ini and the board definition files, you should be able to compile and flash to an ESP32-S3.

```
code smart-thermostat.code-workspace
```

Notes:
a. Any PSRAM will be disabled. This is due to a pin conflict on the MCU.
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


#### Google Sheet to determine GPIO pin usage

***

* [https://docs.google.com/spreadsheets/d/10CfVb4AoQPJP5FjagokVNWyHDSV7czo9InDzF5tszXg/edit#gid=0](https://docs.google.com/spreadsheets/d/10CfVb4AoQPJP5FjagokVNWyHDSV7czo9InDzF5tszXg/edit#gid=0)