#include "thermostat.hpp"  // For function definitions

void setup(void)
{
  // Start serial debugger
  serialStart();
  // Load configuration from EEPROM
  Serial.printf ("Reading EEPROM\n");
  eepromInit();
  // Start wifi
  Serial.printf ("RConnecting to wifi\n");
  OperatingParameters.wifiConnected = 
    wifiStart(WifiCreds.hostname, WifiCreds.ssid, WifiCreds.password);
  // Initialize indicators (relays, LEDs, buzzer)
  Serial.printf ("Initializing indicators\n");
  indicatorsInit();
  // Initialize sensors (temp, humidity, motion ... air quality)
  Serial.printf ("Initializing sensors\n");
  sensorsInit();
  // Initialize the TFT display
  Serial.printf ("Initializing TFT\n");
  tftInit();
  // Create the RTOS task to drive the touchscreen
  Serial.printf ("Starting TFT task\n");
  tftCreateTask();
  // Create the RTOS task to drive the state machine
  Serial.printf ("Starting state machine task\n");
  stateCreateTask();
  // Start web server
  Serial.printf ("Starting web server\n");
  webInit();
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
