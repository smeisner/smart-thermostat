#include "thermostat.hpp"
#include <Preferences.h>
#include "wifi-credentials.h"
#include <nvs_flash.h>

Preferences wifiCreds;
Preferences thermostat;

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

void setDefaultThermostatParams()
{
  thermostat.putInt("currMode", IDLE);
  thermostat.putInt("setMode", OFF);
  thermostat.putFloat("setTemp", 70.0);
  thermostat.putFloat("setTempAutoMin",68.0);
  thermostat.putFloat("setTempAutoMax",74.0);
  thermostat.putFloat("currTemp", 0.0);
  thermostat.putFloat("currHumid", 50.0);
  thermostat.putChar("setUnits", 'F');
  thermostat.putFloat("setSwing", 3.0);
  thermostat.putFloat("setTempCorr", -4.8);
  thermostat.putFloat("setHumidityCorr", 10);
  thermostat.putInt("sleepTime", 30);
  thermostat.putInt("timezoneSel", 17);
  thermostat.putBool("Beep", true);
  thermostat.end();
}

void getThermostatParams()
{
  OperatingParameters.hvacOpMode = (HVAC_MODE)thermostat.getInt("currMode", (int)IDLE);
  OperatingParameters.hvacSetMode = (HVAC_MODE)thermostat.getInt("setMode", (int)OFF);
  OperatingParameters.tempSet = thermostat.getFloat("setTemp", 70.0);
  OperatingParameters.tempSetAutoMin = thermostat.getFloat("setTempAutoMin", 68.0);
  OperatingParameters.tempSetAutoMax = thermostat.getFloat("setTempAutoMax", 74.0);
  OperatingParameters.tempCurrent = thermostat.getFloat("currTemp", 0.0);
  OperatingParameters.humidCurrent = thermostat.getFloat("currHumid", 50.0);
  OperatingParameters.tempUnits = thermostat.getChar("setUnits", 'F');
  OperatingParameters.tempSwing = thermostat.getFloat("setSwing", 3.0);
  OperatingParameters.tempCorrection = thermostat.getFloat("setTempCorr", -4.8);
  OperatingParameters.humidityCorrection = thermostat.getFloat("setHumidityCorr", 10);
  OperatingParameters.thermostatSleepTime = thermostat.getInt("sleepTime", 30);
  OperatingParameters.timezone_sel = thermostat.getInt("timezoneSel", 17);
  OperatingParameters.timezone = (char *)gmt_timezones[OperatingParameters.timezone_sel];
  OperatingParameters.thermostatBeepEnable = thermostat.getBool("Beep", true);
  OperatingParameters.lightDetected = 1024;
  OperatingParameters.motionDetected = false;
}

void setDefaultWifiCreds()
{
  wifiCreds.putString("hostname", hostname);
  wifiCreds.putString("ssid", wifiSsid);
  wifiCreds.putString("pass", wifiPass);
}

void getWifiCreds()
{
  strncpy (WifiCreds.hostname, wifiCreds.getString("hostname", hostname).c_str(), sizeof(WifiCreds.hostname));
  strncpy (WifiCreds.ssid, wifiCreds.getString("ssid", wifiSsid).c_str(), sizeof(WifiCreds.ssid));
  strncpy (WifiCreds.password, wifiCreds.getString("pass", wifiPass).c_str(), sizeof(WifiCreds.password));
}

bool eepromUpdateHvacSetTemp()
{
  thermostat.begin("thermostat", false);
  thermostat.putFloat("setTemp", OperatingParameters.tempSet);
  thermostat.end();
  return true;
}

bool eepromUpdateHvacSetMode()
{
  thermostat.begin("thermostat", false);
  thermostat.putInt("currMode", OperatingParameters.hvacSetMode);
  thermostat.end();
  return true;
}

void eepromInit()
{
  wifiCreds.begin("wifiCreds", false);
  thermostat.begin("thermostat", false);

  if (thermostat.getChar("setUnits", 'X') == 'X')
  {
    Serial.printf("Initializing EEPROM...\n");
    setDefaultThermostatParams();
  }

  if (wifiCreds.getString("ssid", "") == "")
  {
    Serial.printf("Initializing stored wifi credentials...\n");
    setDefaultWifiCreds();
  }

  getThermostatParams();
  getWifiCreds();
}
