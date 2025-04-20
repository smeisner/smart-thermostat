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
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 *  11-Oct-2023: Steve Meisner (steve@meisners.net) - Add suport for home automation (MQTT & Matter)
 *  16-Oct-2023: Steve Meisner (steve@meisners.net) - Add support for friendly names & saving MQTT broker info
 * 
 */

#include "thermostat.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TAG "NVS"
#define NVS_TAG "thermostat"
#define NVS_WIFI_TAG "wificreds"

#define DEF_CURR_MODE IDLE
#define DEF_SET_MODE OFF
#define DEF_SET_TEMP 70.0
#define DEF_SET_TEMP_AUTO_MIN 68.0
#define DEF_SET_TEMP_AUTO_MAX 74.0
#define DEF_CURR_TEMP 0.0
#define DEF_CURR_HUMIDITY 50.0
#define DEF_SET_UNITS 'F'
#define DEF_SET_SWING 3.0
#define DEF_TEMP_CORR -4.8
#define DEF_HUMIDITY_CORR 10
#define DEF_SLEEP_TIME 30
#define DEF_TZ_SEL 15
#define DEF_BEEP_ENABLE true
#define DEF_HVAC_COOL_ENABLE false
#define DEF_HVAC_FAN_ENABLE false
#define DEF_HVAC_2STAGE_ENABLE false
#define DEF_HVAC_REVERSE_ENABLE false
#define DEF_MQTT_ENABLE false
#define DEF_MQTT_BROKER "mqtt"
#define DEF_MQTT_USER "mqtt"
#define DEF_MQTT_PASS "mqtt"
#define DEF_MQTT_PORT 1883
#define DEF_MATTER_ENABLE false
static const calibration_t DEF_TFT_CALIBRATION = {240 / 4096.0, 0, 0, 320 / 4096.0, 0, 0};

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


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//                                                                        //
//          Utility functions to read/write NVS storage                   //
//                                                                        //
//          Note: All data is stored in the "nvs" partition               //
//                                                                        //
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////
//                                                                        //
//          NVS write functions                                           //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

bool writeNVS(nvs_handle_t handle, nvs_type_t type, const char *key, void *_value, int _len = 0)
{
  switch (type)
  {
    case NVS_TYPE_I32:
      if ((nvs_set_i32(handle, key, *(int32_t *)_value)) != ESP_OK)
        return false;
      break;
    case NVS_TYPE_U8:
      if (nvs_set_u8(handle, key, *((uint8_t *)_value)) != ESP_OK)
        return false;
      break;
    case NVS_TYPE_U16:
      if (nvs_set_u16(handle, key, *((u_int16_t *)_value)) != ESP_OK)
        return false;
      break;
    case NVS_TYPE_STR:
      if (nvs_set_str(handle, key, (const char *)_value) != ESP_OK)
        return false;
      break;
    case NVS_TYPE_BLOB:
      if ((nvs_set_blob(handle, key, _value, (size_t)_len)) != ESP_OK)
        return false;
      break;
    default:
      return false;
  }
  return true;
}

void nvs_writeMode(nvs_handle_t handle, const char *key, HVAC_MODE _value)
{
  HVAC_MODE mode = _value;
  writeNVS(handle, NVS_TYPE_I32, key, &mode);
}

void nvs_writeFloat(nvs_handle_t handle, const char *key, float _value)
{
  float value = _value;
  writeNVS(handle, NVS_TYPE_BLOB, key, &value, sizeof(float));
}

void nvs_writeRaw(nvs_handle_t handle, const char *key, void *value, int len)
{
  writeNVS(handle, NVS_TYPE_BLOB, key, value, len);
}

void nvs_writeChar(nvs_handle_t handle, const char *key, char _value)
{
  char value = _value;
  writeNVS(handle, NVS_TYPE_U8, key, &value);
}

void nvs_writeInt16(nvs_handle_t handle, const char *key, u_int16_t _value)
{
  uint16_t value = _value;
  writeNVS(handle, NVS_TYPE_U16, key, &value);
}

void nvs_writeBool(nvs_handle_t handle, const char *key, bool _value)
{
  bool value = _value;
  writeNVS(handle, NVS_TYPE_U8, key, &value);
}

void nvs_writeString(nvs_handle_t handle, const char *key, const char *value)
{
  writeNVS(handle, NVS_TYPE_STR, key, (void *)value);
}


////////////////////////////////////////////////////////////////////////////
//                                                                        //
//          NVS read functions                                            //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

bool readNVS(nvs_handle_t handle, nvs_type_t type, const char *key, void *out_value, int _len = 0)
{
  int len = _len;

  switch (type)
  {
    case NVS_TYPE_I32:
      len = sizeof(int32_t);
      if ((nvs_get_i32(handle, key, (int32_t *)out_value)) != ESP_OK)
        return false;
      break;
    case NVS_TYPE_U8:
      len = sizeof(uint8_t);
      if ((nvs_get_u8(handle, key, (uint8_t *)out_value)) != ESP_OK)
        return false;
      break;
    case NVS_TYPE_U16:
      len = sizeof(u_int16_t);
      if ((nvs_get_u16(handle, key, (u_int16_t *)out_value)) != ESP_OK)
        return false;
      break;
    case NVS_TYPE_STR:
      if (nvs_get_str(handle, key, (char *)out_value, (size_t *)&len) != ESP_OK)
        return false;
      break;
    case NVS_TYPE_BLOB:
      len = _len;
      if ((nvs_get_blob(handle, key, out_value, (size_t *)&len)) != ESP_OK)
        return false;
      break;
    default:
      return false;
  }
  return true;
}

void nvs_readMode(nvs_handle_t handle, const char *key, HVAC_MODE *out_value, HVAC_MODE def)
{
  if (!readNVS(handle, NVS_TYPE_I32, key, out_value))
    *out_value = def;
}

void nvs_readFloat(nvs_handle_t handle, const char *key, float *out_value, float def)
{
  if (!readNVS(handle, NVS_TYPE_BLOB, key, out_value, sizeof(float)))
    *out_value = def;
}

void nvs_readRaw(nvs_handle_t handle, const char *key, void *out_value, void *def, int len)
{
  if (!readNVS(handle, NVS_TYPE_BLOB, key, out_value, len))
    memcpy(out_value, def, len);
}

void nvs_readChar(nvs_handle_t handle, const char *key, char *out_value, char def)
{
  if (!readNVS(handle, NVS_TYPE_U8, key, out_value, sizeof(char)))
    *out_value = def;
}

void nvs_readInt16(nvs_handle_t handle, const char *key, u_int16_t *out_value, u_int16_t def)
{
  if (!readNVS(handle, NVS_TYPE_U16, key, out_value, sizeof(u_int16_t)))
    *out_value = def;
}

void nvs_readBool(nvs_handle_t handle, const char *key, bool *out_value, bool def)
{
  if (!readNVS(handle, NVS_TYPE_U8, key, out_value, sizeof(bool)))
    *out_value = def;
}

void nvs_readStr(nvs_handle_t handle, const char *key, const char *def, char *out_value, int len)
{
  if (!readNVS(handle, NVS_TYPE_STR, key, out_value, len))
    strncpy (out_value, def, len);
}



void clearNVS()
{
  nvs_flash_erase(); // erase the NVS partition and...
  nvs_flash_init(); // initialize the NVS partition.
}

bool openNVS(nvs_handle_t *handle, const char *nvsNameSpace)
{
  esp_err_t err;

  // Initialize NVS
  err = nvs_flash_init();
  // if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  if (err != ESP_OK)
  {
      // NVS partition was truncated and needs to be erased
      // Retry nvs_flash_init
      ESP_LOGW(TAG, "Erasing & initing 'nvs' partition");
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Open the pre-filled NVS partition called "nvs"
  ESP_LOGD(TAG, "Opening Non-Volatile Storage (NVS) handle");
  err = nvs_open(nvsNameSpace, NVS_READWRITE, handle);
  if (err != ESP_OK)
  {
      ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
      return false;
  }
  return true;
}

void closeNVS(nvs_handle_t handle)
{
  esp_err_t err;
  err = nvs_commit(handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error (%s) commiting NVS!", esp_err_to_name(err));
  }
  nvs_close(handle);
}


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//                                                                        //
//          NVS storage for thermostat operating parameters               //
//                                                                        //
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


void setDefaultThermostatParams()
{
  nvs_handle_t my_handle;
  ESP_LOGW(TAG, "Writing default Thermostat perameters to NVS");
  if (!openNVS(&my_handle, NVS_TAG))
  {
    ESP_LOGE(TAG, "Failed to open handle to NVS partition");
    return;
  }
  strncpy (OperatingParameters.FriendlyName, "Thermostat", sizeof(OperatingParameters.FriendlyName));
  nvs_writeMode(my_handle, "currMode", DEF_CURR_MODE);
  nvs_writeMode(my_handle, "setMode", DEF_SET_MODE);
  nvs_writeFloat(my_handle, "setTemp", DEF_SET_TEMP);
  nvs_writeFloat(my_handle, "setTempAutoMin", DEF_SET_TEMP_AUTO_MIN);
  nvs_writeFloat(my_handle, "setTempAutoMax", DEF_SET_TEMP_AUTO_MAX);
  nvs_writeFloat(my_handle, "currTemp", DEF_CURR_TEMP);
  nvs_writeFloat(my_handle, "currHumid", DEF_CURR_HUMIDITY);
  nvs_writeChar(my_handle, "setUnits", DEF_SET_UNITS);
  nvs_writeFloat(my_handle, "setSwing", DEF_SET_SWING);
  nvs_writeFloat(my_handle, "setTempCorr", DEF_TEMP_CORR);
  nvs_writeFloat(my_handle, "setHumidityCorr", DEF_HUMIDITY_CORR);
  nvs_writeInt16(my_handle, "sleepTime", DEF_SLEEP_TIME);
  nvs_writeInt16(my_handle, "timezoneSel", DEF_TZ_SEL);
  nvs_writeBool(my_handle, "Beep", DEF_BEEP_ENABLE);
  nvs_writeBool(my_handle, "hvacCoolEnable", DEF_HVAC_COOL_ENABLE);
  nvs_writeBool(my_handle, "hvacFanEnable", DEF_HVAC_FAN_ENABLE);
  nvs_writeBool(my_handle, "twoStageEnable", DEF_HVAC_2STAGE_ENABLE);
  nvs_writeBool(my_handle, "reverseEnable", DEF_HVAC_REVERSE_ENABLE);
  nvs_writeRaw(my_handle, "tftCalibration", (void *)&DEF_TFT_CALIBRATION, sizeof(calibration_t));
#ifdef MQTT_ENABLED
  nvs_writeBool(my_handle, "MqttEn", DEF_MQTT_ENABLE);
  nvs_writeString(my_handle, "MqttBroker", DEF_MQTT_BROKER);
  nvs_writeString(my_handle, "MqttUsername", DEF_MQTT_USER);
  nvs_writeString(my_handle, "MqttPassword", DEF_MQTT_PASS);
  nvs_writeInt16(my_handle, "MqttBrokerPort", DEF_MQTT_PORT);
#endif
#ifdef MATTER_ENABLED
  nvs_writeBool(my_handle, "MatterEn", DEF_MATTER_ENABLE);
#endif
  closeNVS(my_handle);

  // Set default parameters in local struct
  OperatingParameters.hvacOpMode = DEF_CURR_MODE;
  OperatingParameters.hvacSetMode = DEF_SET_MODE;
  OperatingParameters.tempSet = DEF_SET_TEMP;
  OperatingParameters.tempSetAutoMin = DEF_SET_TEMP_AUTO_MIN;
  OperatingParameters.tempSetAutoMax = DEF_SET_TEMP_AUTO_MAX;
  OperatingParameters.tempCurrent = DEF_CURR_TEMP;
  OperatingParameters.humidCurrent = DEF_CURR_HUMIDITY;
  OperatingParameters.tempUnits = DEF_SET_UNITS;
  OperatingParameters.tempSwing = DEF_SET_SWING;
  OperatingParameters.tempCorrection = DEF_TEMP_CORR;
  OperatingParameters.humidityCorrection = DEF_HUMIDITY_CORR;
  OperatingParameters.thermostatSleepTime = DEF_SLEEP_TIME;
  OperatingParameters.timezone_sel = DEF_TZ_SEL;
  OperatingParameters.thermostatBeepEnable = DEF_BEEP_ENABLE;
  OperatingParameters.hvacCoolEnable = DEF_HVAC_COOL_ENABLE;
  OperatingParameters.hvacFanEnable = DEF_HVAC_FAN_ENABLE;
  OperatingParameters.hvac2StageHeatEnable = DEF_HVAC_2STAGE_ENABLE;
  OperatingParameters.hvacReverseValveEnable = DEF_HVAC_REVERSE_ENABLE;
  OperatingParameters.tftCalibration = DEF_TFT_CALIBRATION;
#ifdef MQTT_ENABLED
  OperatingParameters.MqttEnabled = DEF_MQTT_ENABLE;
  strncpy (OperatingParameters.MqttBrokerHost, DEF_MQTT_BROKER, sizeof(OperatingParameters.MqttBrokerHost));
  strncpy (OperatingParameters.MqttBrokerUsername, DEF_MQTT_USER, sizeof(OperatingParameters.MqttBrokerUsername));
  strncpy (OperatingParameters.MqttBrokerPassword, DEF_MQTT_PASS, sizeof(OperatingParameters.MqttBrokerPassword));
  OperatingParameters.MqttBrokerPort = DEF_MQTT_PORT;
#endif
#ifdef MATTER_ENABLED
  OperatingParameters.MatterEnabled = DEF_MATTER_ENABLE;
#endif
}

void updateThermostatParams()
{
  nvs_handle_t my_handle;
  ESP_LOGI(TAG, "Updating Thermostat parameters in NVS");
  openNVS(&my_handle, NVS_TAG);
  nvs_writeString(my_handle, "friendlyname", OperatingParameters.FriendlyName);
  nvs_writeMode(my_handle, "currMode", OperatingParameters.hvacOpMode);
  nvs_writeMode(my_handle, "setMode", OperatingParameters.hvacSetMode);
  nvs_writeFloat(my_handle, "setTemp", OperatingParameters.tempSet);
  nvs_writeFloat(my_handle, "setTempAutoMin", OperatingParameters.tempSetAutoMin);
  nvs_writeFloat(my_handle, "setTempAutoMax", OperatingParameters.tempSetAutoMax);
  nvs_writeFloat(my_handle, "currTemp", OperatingParameters.tempCurrent);
  nvs_writeFloat(my_handle, "currHumid", OperatingParameters.humidCurrent);
  nvs_writeChar(my_handle, "setUnits", OperatingParameters.tempUnits);
  nvs_writeFloat(my_handle, "setSwing", OperatingParameters.tempSwing);
  nvs_writeFloat(my_handle, "setTempCorr", OperatingParameters.tempCorrection);
  nvs_writeFloat(my_handle, "setHumidityCorr", OperatingParameters.humidityCorrection);
  nvs_writeInt16(my_handle, "sleepTime", OperatingParameters.thermostatSleepTime);
  nvs_writeInt16(my_handle, "timezoneSel", OperatingParameters.timezone_sel);
  nvs_writeBool(my_handle, "Beep", OperatingParameters.thermostatBeepEnable);
  nvs_writeBool(my_handle, "hvacCoolEnable", OperatingParameters.hvacCoolEnable);
  nvs_writeBool(my_handle, "hvacFanEnable", OperatingParameters.hvacFanEnable);
  nvs_writeBool(my_handle, "twoStageEnable", OperatingParameters.hvac2StageHeatEnable);
  nvs_writeBool(my_handle, "reverseEnable", OperatingParameters.hvacReverseValveEnable);
  nvs_writeRaw(my_handle, "tftCalibration", (void *)&OperatingParameters.tftCalibration, sizeof(calibration_t));
  #ifdef MQTT_ENABLED
  nvs_writeBool(my_handle, "MqttEn", OperatingParameters.MqttEnabled);
  nvs_writeString(my_handle, "MqttBroker", OperatingParameters.MqttBrokerHost);
  nvs_writeString(my_handle, "MqttUsername", OperatingParameters.MqttBrokerUsername);
  nvs_writeString(my_handle, "MqttPassword", OperatingParameters.MqttBrokerPassword);
  nvs_writeInt16(my_handle, "MqttBrokerPort", OperatingParameters.MqttBrokerPort);
#endif
#ifdef MATTER_ENABLED
  nvs_writeBool(my_handle, "MatterEn", &OperatingParameters.MatterEnabled);
#endif
  closeNVS(my_handle);
}

void getThermostatParams()
{
  nvs_handle_t my_handle;

  if (!openNVS(&my_handle, NVS_TAG))
  {
    setDefaultThermostatParams();
    if (!openNVS(&my_handle, NVS_TAG))
      return;
  }

  nvs_readStr(my_handle, "friendlyname", "Thermostat", OperatingParameters.FriendlyName, sizeof(OperatingParameters.FriendlyName));
  nvs_readMode(my_handle, "currMode", &OperatingParameters.hvacOpMode, DEF_CURR_MODE);
  nvs_readMode(my_handle, "setMode", &OperatingParameters.hvacSetMode, DEF_SET_MODE);
  nvs_readFloat(my_handle, "setTemp", &OperatingParameters.tempSet, DEF_SET_TEMP);
  nvs_readFloat(my_handle, "setTempAutoMin", &OperatingParameters.tempSetAutoMin, DEF_SET_TEMP_AUTO_MIN);
  nvs_readFloat(my_handle, "setTempAutoMax", &OperatingParameters.tempSetAutoMax, DEF_SET_TEMP_AUTO_MAX);
  nvs_readFloat(my_handle, "currTemp", &OperatingParameters.tempCurrent, DEF_CURR_TEMP);
  nvs_readFloat(my_handle, "currHumid", &OperatingParameters.humidCurrent, DEF_CURR_HUMIDITY);
  nvs_readFloat(my_handle, "setSwing", &OperatingParameters.tempSwing, DEF_SET_SWING);
  nvs_readFloat(my_handle, "setHumidityCorr", &OperatingParameters.humidityCorrection, DEF_HUMIDITY_CORR);
  nvs_readFloat(my_handle, "setTempCorr", &OperatingParameters.tempCorrection, DEF_TEMP_CORR);
  nvs_readChar(my_handle, "setUnits", &OperatingParameters.tempUnits, DEF_SET_UNITS);
  nvs_readInt16(my_handle, "sleepTime", &OperatingParameters.thermostatSleepTime, DEF_SLEEP_TIME);
  nvs_readInt16(my_handle, "timezoneSel", &OperatingParameters.timezone_sel, DEF_TZ_SEL);
  OperatingParameters.timezone = (char *)gmt_timezones[OperatingParameters.timezone_sel];
  nvs_readBool(my_handle, "Beep", &OperatingParameters.thermostatBeepEnable, DEF_BEEP_ENABLE);
  nvs_readBool(my_handle, "hvacCoolEnable", &OperatingParameters.hvacCoolEnable, DEF_HVAC_COOL_ENABLE);
  nvs_readBool(my_handle, "hvacFanEnable", &OperatingParameters.hvacFanEnable, DEF_HVAC_FAN_ENABLE);
  nvs_readBool(my_handle, "twoStageEnable", &OperatingParameters.hvac2StageHeatEnable, DEF_HVAC_2STAGE_ENABLE);
  nvs_readBool(my_handle, "reverseEnable", &OperatingParameters.hvacReverseValveEnable, DEF_HVAC_REVERSE_ENABLE);
  nvs_readRaw(my_handle, "tftCalibration", (void *)&OperatingParameters.tftCalibration, (void *)&DEF_TFT_CALIBRATION, sizeof(calibration_t));
  // OperatingParameters.tftCalibration = DEF_TFT_CALIBRATION;
#ifdef MQTT_ENABLED
  nvs_readBool(my_handle, "MqttEn", &OperatingParameters.MqttEnabled, DEF_MQTT_ENABLE);
  nvs_readStr(my_handle, "MqttBroker", DEF_MQTT_BROKER, OperatingParameters.MqttBrokerHost, sizeof(OperatingParameters.MqttBrokerHost));
  nvs_readStr(my_handle, "MqttUsername", DEF_MQTT_USER, OperatingParameters.MqttBrokerUsername, sizeof(OperatingParameters.MqttBrokerUsername));
  nvs_readStr(my_handle, "MqttPassword", DEF_MQTT_PASS, OperatingParameters.MqttBrokerPassword, sizeof(OperatingParameters.MqttBrokerPassword));
  nvs_readInt16(my_handle, "MqttBrokerPort", &OperatingParameters.MqttBrokerPort, DEF_MQTT_PORT);
#endif
#ifdef MATTER_ENABLED
  nvs_readBool(my_handle, "MatterEn", &OperatingParameters.MatterEnabled, DEF_MATTER_ENABLE);
#endif

  OperatingParameters.lightDetected = 1024;
  OperatingParameters.motionDetected = false;

  closeNVS(my_handle);
}


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//                                                                        //
//          NVS storage for wifi credentials                              //
//                                                                        //
//          Note: These credentials are separate from the ones            //
//          managed by the ESP32 wifi subsystem.                          //
//                                                                        //
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


//
// If the eeprom area is intact (crc matches), then the value is not changed.
// (Change it via the touch screen)
// If the crc is not correct, you can set the wifi info via the UART serial
// console or the touch screen UI.
//
void setDefaultWifiCreds()
{
  ESP_LOGW(TAG, "Writing default WIFI credentials to NVS");
  nvs_handle_t my_handle;
  openNVS(&my_handle, NVS_WIFI_TAG);
  nvs_writeString(my_handle, "hostname", "thermostat");
  nvs_writeString(my_handle, "ssid", "");
  nvs_writeString(my_handle, "pass", "");
  closeNVS(my_handle);

  strncpy (OperatingParameters.DeviceName, "thermostat", sizeof(OperatingParameters.DeviceName));
  strncpy (WifiCreds.ssid, "", sizeof(WifiCreds.ssid));
  strncpy (WifiCreds.password, "", sizeof(WifiCreds.password));
}

//
// Only called from ui_events.cpp
//
void setWifiCreds()
{
  ESP_LOGW(TAG, "Saving latest WIFI credentials to NVS");
  // printf ("*** [setWifiCreds] WifiCreds.password = %s\n", WifiCreds.password);
  nvs_handle_t my_handle;
  openNVS(&my_handle, NVS_WIFI_TAG);
  nvs_writeString(my_handle, "hostname", OperatingParameters.DeviceName);
  nvs_writeString(my_handle, "ssid", WifiCreds.ssid);
  nvs_writeString(my_handle, "pass", WifiCreds.password);
  closeNVS(my_handle);
}

void getWifiCreds()
{
  nvs_handle_t my_handle;
  if (!openNVS(&my_handle, NVS_WIFI_TAG))
  {
    ESP_LOGE(TAG, "Failed to open NVS wifi partition");
    setDefaultWifiCreds();
    if (!openNVS(&my_handle, NVS_WIFI_TAG))
      return;
  }
  nvs_readStr(my_handle, "hostname", "thermostat", OperatingParameters.DeviceName, sizeof(OperatingParameters.DeviceName));
  nvs_readStr(my_handle, "ssid", "", WifiCreds.ssid, sizeof(WifiCreds.ssid));
  nvs_readStr(my_handle, "pass", "", WifiCreds.password, sizeof(WifiCreds.password));
  // printf ("*** [getWifiCreds] WifiCreds.password = %s\n", WifiCreds.password);
  closeNVS(my_handle);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//                                                                        //
//          NVS functions to write specific values                        //
//                                                                        //
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool eepromUpdateArbFloat(const char *key, float value)
{
  nvs_handle_t my_handle;
  openNVS(&my_handle, NVS_TAG);
  nvs_writeFloat(my_handle, key, value);
  closeNVS(my_handle);
  return true;
}

bool eepromUpdateArbMode(const char *key, HVAC_MODE mode)
{
  nvs_handle_t my_handle;
  openNVS(&my_handle, NVS_TAG);
  nvs_writeMode(my_handle, key, mode);
  closeNVS(my_handle);
  return true;
}

bool eepromUpdateHvacSetTemp()
{
  return eepromUpdateArbFloat("setTemp", OperatingParameters.tempSet);
}

bool eepromUpdateHvacSetMode()
{
  return eepromUpdateArbMode("currMode", OperatingParameters.hvacOpMode);
}
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//                                                                        //
//          NVS functions init entry point                                //
//                                                                        //
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void eepromInit()
{
  ESP_LOGI(TAG, "Loading Thermostat operating parameters from NVS");
  getThermostatParams();
  ESP_LOGI(TAG, "Loading wifi credentials from NVS");
  getWifiCreds();
  ESP_LOGI(TAG, "Calibration: %.3f %.3f %.3f    %.3f %.3f %.3f",
      OperatingParameters.tftCalibration.alpha_x, OperatingParameters.tftCalibration.beta_x, OperatingParameters.tftCalibration.delta_x,
      OperatingParameters.tftCalibration.alpha_y, OperatingParameters.tftCalibration.beta_y, OperatingParameters.tftCalibration.delta_y
    );
}
