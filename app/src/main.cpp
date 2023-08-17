/*!
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
 * 
 */

#include "thermostat.hpp"  // For function definitions

void setup(void)
{
  // Start serial debugger
  serialStart();

  // Show diagnostic info on the serial monitor
  showConfigurationData();

  // Initialize the TFT display
  Serial.printf ("Initializing TFT\n");
  tftInit();
  // Create the RTOS task to drive the touchscreen
  Serial.printf ("Starting TFT task\n");
  tftCreateTask();

  // Load configuration from EEPROM
  Serial.printf ("Reading EEPROM\n");
  eepromInit();
  // Start wifi
  Serial.printf ("Connecting to wifi\n");
  OperatingParameters.wifiConnected = 
    wifiStart(WifiCreds.hostname, WifiCreds.ssid, WifiCreds.password);
  // Initialize indicators (relays, LEDs, buzzer)
  Serial.printf ("Initializing indicators\n");
  indicatorsInit();
  // Initialize sensors (temp, humidity, motion ... air quality)
  Serial.printf ("Initializing sensors\n");
  sensorsInit();

  // Create the RTOS task to drive the state machine
  Serial.printf ("Starting state machine task\n");
  stateCreateTask();
  // Start web server
  Serial.printf ("Starting web server\n");
  webCreateTask();
  // Play the startup sound
  audioStartupBeep();
  // Do a quick test of the relays
  testToggleRelays();

  Serial.printf ("Startup done\n");
}

void loop(void)
{
  // Nothing to do here since everything is done via RTOS tasks & interrupts
}
