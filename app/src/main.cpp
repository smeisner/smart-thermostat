#include "thermostat.hpp"  // For function definitions

void setup(void)
{
  // Start serial debugger
  serialStart();
  // Load configuration from EEPROM
  eepromInit();
  // Start wifi
  OperatingParameters.wifiConnected = 
    wifiStart(WifiCreds.hostname, WifiCreds.ssid, WifiCreds.password);
  // Initialize indicators (relays, LEDs, buzzer)
  indicatorsInit();
  // Play the startup sound
  audioStartupBeep();
  // Initialize sensors (temp, humidity, motion ... air quality)
  sensorsInit();
  // Start web server
  webInit();
  // Initialize the TFT display
  tftInit();
  // Show the splash screen
//  displaySplash();
  // Do a quick test of the relays
  testToggleRelays();
  // Create the RTOS task to drive the touchscreen
  tftCreateTask();
  // Create the RTOS task to drive the state machine
  stateCreateTask();
}

extern void tftPump();

void loop(void)
{
  // Nothing to do here since everything is done via RTOS tasks & interrupts

  tftPump();
}