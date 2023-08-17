/*!
// SPDX-License-Identifier: GPL-3.0-only
/*
 * eeprom.cpp
 *
 * Provide API to access data stored in, and write to, non-voltaile storage (NVS)
 * also referred to as EEPROM.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * Notes:
 * clearNVS() will not only initialize the partition that holds the NVS data,
 * but also wipe it clean ... including wifi credentials and Matter state. Use with caution!
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 * 
 */

#include "thermostat.hpp"
#include <Preferences.h>
#include <nvs_flash.h>

Preferences prefs;

//@@@
//Timezone info, such as "America/New York"
//Zipcode, for outdoor temp & weather
//
// Timezone
// wifi SSID
// Wifi PSK
// HVAC Mode cool
// hvac mode fan
// HVAC heat 2-stage
// HVAC reverse valve
//
//@@@ Enable/disable beep
// time to sleep display

void clearNVS()
{
  nvs_flash_erase(); // erase the NVS partition and...
  nvs_flash_init(); // initialize the NVS partition.
}

#define NVS_TAG "thermostat"
#define NVS_WIFI_TAG "wificreds"

void setDefaultThermostatParams()
{
  prefs.begin(NVS_TAG, false);
  prefs.putInt("currMode", IDLE);
  prefs.putInt("setMode", OFF);
  prefs.putFloat("setTemp", 70.0);
  prefs.putFloat("setTempAutoMin",68.0);
  prefs.putFloat("setTempAutoMax",74.0);
  prefs.putFloat("currTemp", 0.0);
  prefs.putFloat("currHumid", 50.0);
  prefs.putChar("setUnits", 'F');
  prefs.putFloat("setSwing", 3.0);
  prefs.putFloat("setTempCorr", -4.8);
  prefs.putFloat("setHumidityCorr", 10);
  prefs.putInt("sleepTime", 30);
  prefs.putInt("timezoneSel", 15);
  prefs.putBool("Beep", true);
  prefs.end();
}

void updateThermostatParams()
{
  prefs.begin(NVS_TAG, false);
  prefs.putInt("currMode", OperatingParameters.hvacOpMode);
  prefs.putInt("setMode", OperatingParameters.hvacSetMode);
  prefs.putFloat("setTemp", OperatingParameters.tempSet);
  prefs.putFloat("setTempAutoMin", OperatingParameters.tempSetAutoMin);
  prefs.putFloat("setTempAutoMax", OperatingParameters.tempSetAutoMax);
  prefs.putFloat("currTemp", OperatingParameters.tempCurrent);
  prefs.putFloat("currHumid", OperatingParameters.humidCurrent);
  prefs.putChar("setUnits", OperatingParameters.tempUnits);
  prefs.putFloat("setSwing", OperatingParameters.tempSwing);
  prefs.putFloat("setTempCorr", OperatingParameters.tempCorrection);
  prefs.putFloat("setHumidityCorr", OperatingParameters.humidityCorrection);
  prefs.putInt("sleepTime", OperatingParameters.thermostatSleepTime);
  prefs.putInt("timezoneSel", OperatingParameters.timezone_sel);
  prefs.putBool("Beep", OperatingParameters.thermostatBeepEnable);
  prefs.end();
}

void getThermostatParams()
{
  prefs.begin(NVS_TAG, false);
  OperatingParameters.hvacOpMode = (HVAC_MODE)prefs.getInt("currMode", (int)IDLE);
  OperatingParameters.hvacSetMode = (HVAC_MODE)prefs.getInt("setMode", (int)OFF);
  OperatingParameters.tempSet = prefs.getFloat("setTemp", 70.0);
  OperatingParameters.tempSetAutoMin = prefs.getFloat("setTempAutoMin", 68.0);
  OperatingParameters.tempSetAutoMax = prefs.getFloat("setTempAutoMax", 74.0);
  OperatingParameters.tempCurrent = prefs.getFloat("currTemp", 0.0);
  OperatingParameters.humidCurrent = prefs.getFloat("currHumid", 50.0);
  OperatingParameters.tempUnits = prefs.getChar("setUnits", 'F');
  OperatingParameters.tempSwing = prefs.getFloat("setSwing", 3.0);
  OperatingParameters.tempCorrection = prefs.getFloat("setTempCorr", -4.8);
  OperatingParameters.humidityCorrection = prefs.getFloat("setHumidityCorr", 10);
  OperatingParameters.thermostatSleepTime = prefs.getInt("sleepTime", 30);
  OperatingParameters.timezone_sel = prefs.getInt("timezoneSel", 15);
  OperatingParameters.timezone = (char *)gmt_timezones[OperatingParameters.timezone_sel];
  OperatingParameters.thermostatBeepEnable = prefs.getBool("Beep", true);
  OperatingParameters.lightDetected = 1024;
  OperatingParameters.motionDetected = false;
  prefs.end();
}

//
// If the eeprom area is intact (crc matches), then the value is not changed.
// (Change it via the touch screen)
// If the crc is not correct, you can set the wifi info via the UART serial
// console or the touch screen UI.
//
void setDefaultWifiCreds()
{
  prefs.begin(NVS_WIFI_TAG, false);
  prefs.putString("hostname", "thermostat");
  prefs.putString("ssid", "");
  prefs.putString("pass", "");
  prefs.end();
}

//
// Only called from ui_events.cpp
//
void setWifiCreds()
{
  prefs.begin(NVS_WIFI_TAG, false);
  prefs.putString("hostname", WifiCreds.hostname);
  prefs.putString("ssid", WifiCreds.ssid);
  prefs.putString("pass", WifiCreds.password);
  prefs.end();
}

void getWifiCreds()
{
  prefs.begin(NVS_WIFI_TAG, false);
  strncpy (WifiCreds.hostname, prefs.getString("hostname", "thermostat").c_str(), sizeof(WifiCreds.hostname));
  strncpy (WifiCreds.ssid, prefs.getString("ssid", "").c_str(), sizeof(WifiCreds.ssid));
  strncpy (WifiCreds.password, prefs.getString("pass", "").c_str(), sizeof(WifiCreds.password));
  prefs.end();
}

bool eepromUpdateHvacSetTemp()
{
  prefs.begin(NVS_TAG, false);
  prefs.putFloat("setTemp", OperatingParameters.tempSet);
  prefs.end();
  return true;
}

bool eepromUpdateHvacSetMode()
{
  prefs.begin(NVS_TAG, false);
  prefs.putInt("currMode", OperatingParameters.hvacSetMode);
  prefs.end();
  return true;
}

void eepromInit()
{
  prefs.begin(NVS_TAG, false);
  if ((not prefs.isKey("setUnits")) ||
      (prefs.getChar("setUnits", 'X') == 'X'))
  {
    Serial.printf("Initializing EEPROM...\n");
    prefs.end();
    setDefaultThermostatParams();
  }
  else
    prefs.end();

  prefs.begin(NVS_WIFI_TAG, false);
  if ((not prefs.isKey("ssid")) ||
      (prefs.getString("ssid", "X") == "X"))
  {
    Serial.printf("Initializing stored wifi credentials...\n");
    prefs.end();
    setDefaultWifiCreds();
  }
  else
    prefs.end();

  getThermostatParams();
  getWifiCreds();
}
