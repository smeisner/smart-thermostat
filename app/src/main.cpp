// SPDX-License-Identifier: GPL-3.0-only
/*
 * main.cpp
 *
 * Primary entry point for starting the thermostat firmware. This module
 * calls all of the start/initialization functions of the thermostat. 
 * A side effect of initialiing the module is all RTOS tasks are started.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * Notes:
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 *  11-Oct-2023: Steve Meisner (steve@meisners.net) - Add suport for home automation (MQTT & Matter)
 *  16-Oct-2023: Steve Meisner (steve@meisners.net) - Add restriction for enabling both MQTT & Matter & removed printf's
 * 
 */

#if defined(MATTER_ENABLED) && defined(MQTT_ENABLED)
  #error "Do not enable Matter/CHIP and MQTT at the same time"
#endif

#include "thermostat.hpp"
#include "esp_timer.h"
#include "version.h"
#include "logger.h"

#define TAG "Main"

// Build version strings for other modules to use
const char *VersionString = VERSION_STRING;
const char *VersionCopyright = VERSION_COPYRIGHT;
const char *VersionBuildDateTime = VERSION_BUILD_DATE_TIME;

int64_t millis() { return esp_timer_get_time() / 1000;}

void app_main()
{
  vTaskDelay(pdMS_TO_TICKS(10000)); //@@@

  // Set default log level for all components
  // esp_log_level_set("*", ESP_LOG_WARN);
  // esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("*", ESP_LOG_DEBUG);

  // Set up logging function to customize the console, telnet and SD Card
  // Logging plumbing and log levels.
  loggerEnableLocalLogging();

  ESP_LOGI (TAG, "IDF version: %s", esp_get_idf_version());
  ESP_LOGD (TAG, "- Free memory: %d bytes", esp_get_free_heap_size());

  // Load configuration from EEPROM
  ESP_LOGI (TAG, "Reading EEPROM");
  eepromInit();

  // Initialize the TFT display
  ESP_LOGI (TAG, "Initializing TFT");
  tftInit();
  // Create the RTOS task to drive the touchscreen
  ESP_LOGI (TAG, "Starting TFT task");
  tftCreateTask();

  ESP_LOGI (TAG, "Initialize SD memory card");
  sd_init();
  // Check if the system is recovering from a crash loop
  sdcard_check_and_save_coredump("/sdcard/coredump.bin");

  // Initialize sensors (temp, humidity, motion, etc)
  ESP_LOGI (TAG, "Initializing sensors");
  sensorsInit();

#ifdef MATTER_ENABLED
  // Start Matter
  ESP_LOGI (TAG, "Starting Matter");
  OperatingParameters.MatterStarted = MatterInit();
#endif

  // Start wifi
  ESP_LOGI (TAG, "Starting wifi (\"%s\", \"%s\")", WifiCreds.ssid, WifiCreds.password);
  ESP_ERROR_CHECK(esp_netif_init());
  OperatingParameters.wifiConnected = 
    // wifiStart(WifiCreds.hostname, WifiCreds.ssid, WifiCreds.password);
    WifiStart(OperatingParameters.DeviceName, WifiCreds.ssid, WifiCreds.password);

#ifdef MQTT_ENABLED
  // Start Matter
  ESP_LOGI (TAG, "Starting MQTT");
  MqttInit();
#endif

  // Called when wifi is started, so no need to call it here
  // // Start SNTP connection to get local time
  // initTimeSntp();

  // Initialize indicators (relays, LEDs, buzzer)
  ESP_LOGI (TAG, "Initializing indicators");
  indicatorsInit();
  initRelays();

  // Start web server
  ESP_LOGI (TAG, "Starting web server");
  webStart();

#ifdef TELNET_ENABLED
  // Start telnet server
  telnetStart();
#endif

  // Create the RTOS task to drive the state machine
  ESP_LOGI (TAG, "Starting state machine task");
  stateCreateTask();

  ESP_LOGI (TAG, "Saving config info");
  saveConfig();

  // Play the startup sound
  audioStartupBeep();
  ESP_LOGI (TAG, "Startup done");
}
