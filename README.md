# Smart Thermostat

The idea of this project is to create a truly smart thermostat.
Main features are:

* Can be integrated into a home automation system (such as Home Assistant)
* Cloud connection not required
* Wifi connected
* Bluetooth LE connected (required for initial setup)
* Will act similarly to a person's preferences when it comes to HVAC. For example:
    * the temperature set should be dependent upon local forecast
    * If the outside temp is close to the set thermostat temp, suggest opening a window
    * Detect if a window or door is open and disable the heat/AC
* Use Matter protocol (thermostat V2)
* Provide local web site to control/configure thermostat
* Allow ssh login
* Provide diagnostic logging (rsyslog as well?)
* Detect degradation of temp rise/fall to identify clogged filter or service required
* Employ remote temp sensors to balance temp within home
* Detect lower air quality to suggest opening a window (and disable HVAC)
* No mechanical buttons - soft buttons on the TouchScreen only ... except maybe a master reset if things go wrong!

Basic layout of thermostat:

<img src="./Block%20Diagram.drawio.png">

## Operating environment

FreeRTOS will be the OS. Events will be driven by timers and interrupts (see list below under Specs...). A state machine will make up the primary task that runs on a continuous, low-priority loop. All events will have higher priorities.

There is a concern around capabilites to run the thermostat code with wifi, ssh, web server, Thread / Zigbee, etc. and all required libraries. Will there be enough space for the code and processing power to successfully run everything?

## MCU

The processor will be either one or more microcontrollers (ESP32) or a SBC (RPi CM4). The CM4 will provide a full Linux implementation allowing for ssh, web site, dev environment, package management, etc. but will have higher power requirements. The microcontroller will be more limited when it comes to implementing funtionalty and developing locally on the t-stat.

* [ ] Raspberry Pi Zero W 2
* [ ] Raspberry Pi CM4
* [x] ESP32
* [ ] Other

The coice to use an Espressif MCU was made since it is cheaper, smaller and more simple to integrate into the circuit. It also provides many GPIO pins to control everything.

Eventually, the ESP32-C6 will be used as it has support for [802.15.4 (Zigbee / Thread)](https://en.wikipedia.org/wiki/IEEE_802.15.4). But it does not have enough GPIO pins, so a multiplexer must be added to the PCB.

## Touch screen

There are many choices for touchscreens available. The display should be \~3" diag and use SPI, I2C or DSI. Some SBCs/uControllers allow for 1 SPI bus. At the same time, some touch screens require 2 (1 for the TFT display; 1 for the touch interface). So this must be considered.

The TFT display chosen is the [MSP3218](http://www.lcdwiki.com/3.2inch_SPI_Module_ILI9341_SKU:MSP3218) with the ILI9341 TFT driver and the XPT2046 touch controller.

## Power Supply

Built into the PCB will be a power supply capable of providing stable 5V DC for the processor and sensors. An LM2576HVT-5 will be used to allow for 24V in and still remain stable regulating the power.

[LM2576HVT Datasheet](https://www.ti.com/general/docs/suppproductinfo.tsp?distId=10&gotoUrl=https%3A%2F%2Fwww.ti.com%2Flit%2Fgpn%2Flm2576hv)

## Sensors

Other than an onboard Temp/Humidity/Air quality sensor setup, there can also be remote sensors (maybe connected via MQTT or proprietary ethernet protocol?) that will provide data to make various decisions. These could be inside the home, outside or even from online sources (such as local weather sites).

A light sensor (LDR) and motion sensor (RCWL-0516) will also be incoporated into the design. The TFT display has a built in touch sensor for user selections.

## Possibilities

#### ~~Use CSI connected camera to detect light level and person approaching.~~

~~When person approaches, turn on Touchscreen backlight. When detected light level is low, turn off backlight after a delay.~~
~~Could also use proximity sensor (PiR or RADAR) for detecting person. A simple LDR could also be used to detect light level.~~

#### Integrate SMS/chat ability

This can be used to notify user of events/alerts. Could also be Slack or other notifications. An onboard annunciator (like a speaker) feels wrong.

#### Use super capacitor to maintain power during power outage

This would maintain power during brief power outages

***

Task list:

* [x] Build 24VAC to 5VDC power supply with minimal ripple (< 2%)
* [x] Decide on MCU/SBC
* [x] Determine sensors to be used (temp, humidity, air quality)
* [ ] Develop V1 of host app (including MQTT/HA communications)
* [x] Generate schematic & PCB
* [x] Generate PCB BOM (compatible with JLCPCB parts list)
* [x] Have PCB manufactured (JLCPCB)

V2 task list:

* [ ] Design device web page
* [ ] Design 3D printed case (Polycase may be good supplier)
* [ ] Implement Matter
* [ ] Develop Home Assistant integration
* [ ] Add OTA update ability
* [ ] Add air quality monitoring (to PCB and app)

## Specs...
 
#### FreeRTOS Elements:
 
* Interrupt: Touch
* When: User touches display; Asserts line LO
<br>

* Interrupt: Motion
* When: RCWL-0516 senses motion; asserts line HI
<br>

* Task: aht
* Freq: 10 secs
* Purpose: update temp & humidity
<br/>

* Task: touch
* Freq: int (triggered indirectly via TFT touch int)
* Purpose:
  - Update last touch point
  - Play audible beep
<br>

* Task: motion
* Freq: int (triggered directly via motion int)
* Purpose: update last motion detected timestamp
<br/>

* Task: state-machine
* Freq: 1 sec
* Purpose:
  - Pump state machine
  - Using temp & humidity, motion, last touch pt,
  - Call routines to navigate menus (options / settings)
  - Adjust display brightness (when on) based on LDR
  - Turn off display with no motion

#### States:

* Init (start)
* Idle
* Fan only
* Heat
* Cool
* Error

#### To be added:

* [ ] Wifi
* [ ] Web server
* [ ] Telnet / ssh server
* [ ] Bluetooth
* [ ] Zigbee / Thread (update for ESP32-C6)
* [ ] Add air quality monitoring device
* [ ] OTA updates

#### Parameters set by user:

* Wifi credentials
* Location (zip code) for outdoor temp - or URI for local/private temp sensor
* Home Assistant FQDN / IP
* Temp swing
* Max / min temp (when to alert user)
* email list / SMS for notifications
* c / f
* reversing valve
* auto changeover (A/C to/from Heat)

#### Calculated params:

* Average temp change rate vs temp differential (inside / outside temps)
    * When value exceeded (based on HVAC mode), ask if window/door is open
* Average temp change rate
    * Detects when filter / furnace needs servicing
    * Used to calculate time to temp
