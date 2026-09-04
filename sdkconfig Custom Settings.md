## **Project Configuration Documentation (sdkconfig)**

This document outlines the custom configurations used in this project's sdkconfig file.

There is a file in the top level (```sdkconfig-thermostat-settings.txt```) that includes the settings described below. Since the sdkconfig file can get regenerated, 
the script ```update-sdkconfig.sh``` should be used to apply these settings to any newly generated sdkconfig file.

```./update-sdkconfig.sh sdkconfig-thermostat-settings.txt sdkconfig.esp32s3```

## **🛠️ System Performance & Environment**

These settings define the core hardware speed, operating system boundaries, and framework versions.

> * **CONFIG\_ESP\_DEFAULT\_CPU\_FREQ\_MHZ=240**
> * **CONFIG\_ESP\_DEFAULT\_CPU\_FREQ\_MHZ\_240=y**  
  * **Description:** Sets the default CPU frequency of the primary chip to its maximum performance level of 240 MHz instead of the lower 160 MHz option.  
> * **CONFIG\_ESP32S3\_DEFAULT\_CPU\_FREQ\_MHZ=240**
> * **CONFIG\_ESP32S3\_DEFAULT\_CPU\_FREQ\_240=y**  
  * **Description:** Specifically forces the ESP32-S3 variant (if used) to run at its peak 240 MHz clock speed.  
> * **CONFIG\_FREERTOS\_ISR\_STACKSIZE=2096**  
  * **Description:** Allocates 2,096 bytes of memory specifically reserved for handling hardware interrupts (like pin changes or timers). This prevents memory crashes when hardware events fire rapidly.  
> * **CONFIG\_LOG\_COLORS=y**  
  * **Description:** Enables ANSI color formatting in the serial output monitor. System warnings appear in yellow and errors appear in red for easier debugging.

## **💾 Flash Memory & Partitioning**

These settings govern how the chip maps its internal storage and identifies firmware binaries.

> * **CONFIG\_PARTITION\_TABLE\_FILENAME="partitions\_two\_ota\_coredump.csv"**  
  * **Description:** Points the build system to a custom flash memory map layout. This layout allocates room for two application slots (for wireless updates) and a dedicated space to save system crash logs (core dumps).  
> * **CONFIG\_APP\_RETRIEVE\_LEN\_ELF\_SHA=16**  
  * **Description:** Allocates a 16-byte buffer to hold the unique SHA-256 build fingerprint of the application binary. This helps the bootloader track firmware integrity and version changes.

## **📡 Wi-Fi & TCP/IP Networking**

These settings optimize memory usage for handling network connections and internet data streams.

> * **CONFIG\_TCP\_SND\_BUF\_DEFAULT=5744**
> * **CONFIG\_LWIP\_TCP\_SND\_BUF\_DEFAULT=5744**  
  * **Description:** Restricts the maximum memory allocated for sending TCP data packets to 5,744 bytes per connection. This matches exactly 4 standard network packet segments, optimizing RAM.  
> * **CONFIG\_TCP\_WND\_DEFAULT=5744**
> * **CONFIG\_LWIP\_TCP\_WND\_DEFAULT=5744**  
  * **Description:** Sets the incoming network receive window buffer to 5,744 bytes. It ensures smooth network speeds without draining too much system memory.  
> * **CONFIG\_LWIP\_DHCP\_OPTIONS\_LEN=69**  
  * **Description:** Reserves 69 bytes of buffer space to read extra configuration properties (like local time or specialized server tags) sent back by a network router's DHCP server.  
> * **CONFIG\_LWIP\_HOOK\_IP6\_INPUT\_NONE=y**  
  * **Description:** Bypasses custom routing filters for IPv6 web packets, allowing the network engine to process standard incoming IPv6 traffic natively with zero extra overhead.  
> * **\# CONFIG\_ESP\_WIFI\_GMAC\_SUPPORT is not set**  
  * **Description:** Disables advanced GMAC security formatting inside the Wi-Fi stack. This saves RAM and flash space since standard home and office networks (WPA2/WPA3) do not require it.

## **🌐 HTTP Server & Over-the-Air (OTA) Updates**

These settings manage data limits for the internal web server and define how wireless firmware updates behave.

> * **CONFIG\_HTTPD\_MAX\_REQ\_HDR\_LEN=4096**  
  * **Description:** Grants the built-in web server up to 4,096 bytes of memory to read incoming web request headers. This allows the server to safely accept large web cookies or authorization tokens.  
> * **CONFIG\_HTTPD\_MAX\_URI\_LEN=1024**  
  * **Description:** Allows the web server to process long web address URLs up to 1,024 characters long. Useful for handling complex API queries and path names.  
> * **CONFIG\_OTA\_ALLOW\_HTTP=y**
> * **CONFIG\_ESP\_HTTPS\_OTA\_ALLOW\_HTTP=y**  
  * **Description:** Permits the chip to download wireless firmware upgrades over unencrypted http:// links rather than forcing secure https:// validation. This simplifies local development testing but should be used cautiously in live products.

## **🚨 Crash Log Handling (Core Dump)**

These settings dictate how the ESP32 captures and saves debug information to flash memory whenever the software unexpectedly crashes.

> * **CONFIG\_ESP\_COREDUMP\_ENABLE=y**
> * **CONFIG\_ESP32\_ENABLE\_COREDUMP=y**  
  * **Description:** Turns on the core dump logging engine, ensuring system states are saved if a fatal crash occurs.  
> * **CONFIG\_ESP\_COREDUMP\_ENABLE\_TO\_FLASH=y**
> * **CONFIG\_ESP32\_ENABLE\_COREDUMP\_TO\_FLASH=y**  
  * **Description:** Instructs the device to save crash diagnostics directly onto the physical flash storage partition rather than throwing them away.  
> * **CONFIG\_ESP\_COREDUMP\_DATA\_FORMAT\_ELF=y**
> * **CONFIG\_ESP32\_COREDUMP\_DATA\_FORMAT\_ELF=y**  
  * **Description:** Saves the crash dump logs using the standard executable ELF format. This allows developer tools (like GDB) to show the exact file and line of code that caused the failure.  
> * **CONFIG\_ESP\_COREDUMP\_CHECKSUM\_CRC32=y**
> * **CONFIG\_ESP32\_COREDUMP\_CHECKSUM\_CRC32=y**  
  * **Description:** Uses lightweight CRC32 math verification to ensure saved crash log data does not get corrupted on the storage drive.  
> * **CONFIG\_ESP\_COREDUMP\_MAX\_TASKS\_NUM=64**
> * **CONFIG\_ESP32\_CORE\_DUMP\_MAX\_TASKS\_NUM=64**  
  * **Description:** Configures the logger to safely track snapshot details for up to 64 simultaneously running software tasks when analyzing a system failure.  
> * **CONFIG\_ESP\_COREDUMP\_STACK\_SIZE=0**
> * **CONFIG\_ESP32\_CORE\_DUMP\_STACK\_SIZE=0**  
  * **Description:** Set to 0 to automatically log the *entire* relevant memory stack area during a crash instead of limiting the snapshot to a fixed byte length.  
> * **CONFIG\_ESP\_COREDUMP\_CHECK\_BOOT=y**  
  * **Description:** Forces the device to check for newly saved crash data every time it boots up, allowing the application to print out or send old crash reports over the network.  
> * **CONFIG\_ESP\_COREDUMP\_LOGS=y**  
  * **Description:** Directs the system to print diagnostic summaries directly to the serial terminal feed immediately following a reboot after a crash.  
> * **\# CONFIG\_ESP\_COREDUMP\_CAPTURE\_DRAM is not set**  
  * **Description:** Skips capturing the entire Data RAM memory space during a crash log save. This keeps the saved crash files small enough to fit within your designated partition boundaries.

