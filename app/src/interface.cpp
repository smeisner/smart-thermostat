#include <ctype.h>

#include "thermostat.hpp"
#include "interface.hpp"

static const char *TAG = "INTERFACE";

TaskHandle_t ntScanTaskHandler = NULL;

void uiWake(bool beep)
{
  ESP_LOGI(TAG, "UI Wakeup");
  if (beep)
    audioBeep();
}

void uiDim()
{
  ESP_LOGI(TAG, "UI Dim");
}

void updateHvacMode(HVAC_MODE mode)
{
  OperatingParameters.hvacSetMode = mode;
  eepromUpdateHvacSetMode();
#ifdef MQTT_ENABLED
  MqttUpdateStatusTopic();
#endif
}

void updateHvacSetTemp(float setTemp)
{
  OperatingParameters.tempSet = setTemp;
  eepromUpdateHvacSetTemp();
  ESP_LOGI(TAG, "Set temp: %.1f", setTemp);
#ifdef MQTT_ENABLED
  MqttUpdateStatusTopic();
#endif
}

void updateCalibration(calibration_t *cal)
{
  OperatingParameters.tftCalibration = *cal;
  updateThermostatParams();
}

void updateTempUnits(char units)
{
  if (units == OperatingParameters.tempUnits)
    return;
  if (units == 'C')
  {
    // Switch to Celcius
    OperatingParameters.tempSet = (OperatingParameters.tempSet - 32.0) / (9.0/5.0);
    OperatingParameters.tempCurrent = (OperatingParameters.tempCurrent - 32.0) / (9.0/5.0);
    OperatingParameters.tempCorrection = OperatingParameters.tempCorrection * 5.0 / 9.0;
    OperatingParameters.tempSwing = OperatingParameters.tempSwing * 5.0 / 9.0;
  } else {
    // Switch to Fahrenheit
    OperatingParameters.tempSet = (OperatingParameters.tempSet * 9.0/5.0) + 32.0;
    OperatingParameters.tempCurrent = (OperatingParameters.tempCurrent * 9.0/5.0) + 32.0;
    OperatingParameters.tempCorrection = OperatingParameters.tempCorrection * 1.8;
    OperatingParameters.tempSwing = OperatingParameters.tempSwing * 1.8;
  }
  OperatingParameters.tempUnits = units;
  resetTempSmooth();
  updateThermostatParams();
}

void updateHVACSettings(bool newStages, bool newRV, bool newAux, bool newCool, bool newFan)
{
  OperatingParameters.hvac2StageHeatEnable = newStages;
  OperatingParameters.hvacReverseValveEnable = newRV;
  OperatingParameters.hvacCoolEnable = newCool;
  OperatingParameters.hvacFanEnable = newFan;
  updateThermostatParams();
}

void updateBeepEnable(bool enabled)
{
  OperatingParameters.thermostatBeepEnable = enabled;
  updateThermostatParams();
}

void updateTimezone(int idx, bool hasDST)
{
  OperatingParameters.timezone_sel = idx;
  OperatingParameters.timezone = (char *)(gmt_timezones[OperatingParameters.timezone_sel]);
  updateTimezoneFromConfig();
  updateThermostatParams();
}

void updateFriendlyHost(const char *host)
{
  if (strlcpy(OperatingParameters.FriendlyName, host, sizeof(OperatingParameters.FriendlyName)) >= sizeof(OperatingParameters.FriendlyName))
    ESP_LOGE(TAG, "FriendlyName was trancated");

  char *p = (char *)OperatingParameters.FriendlyName;
  char *t = (char *)OperatingParameters.DeviceName;
  // Make DeviceName the same as FriendlyName just with all lower
  // case letters and no spaces.
  while (*p)
  {
    if (*p != ' ')
    {
      *t = tolower(*p);
      t++;
    }         
    p++;
  }
  setWifiCreds(); // FIXME: Why does hostname get set by WifiCreds?
  updateThermostatParams();
}

void updateWifiCreds(const char *ssid, const char *password, bool writeNVS)
{
  if (WifiCreds.ssid != ssid)
    if (strlcpy(WifiCreds.ssid, ssid, sizeof(WifiCreds.ssid)) >= sizeof(WifiCreds.ssid))
      ESP_LOGE(TAG, "Wifi SSID was trancated");
  if (WifiCreds.password != password)
    if (strlcpy(WifiCreds.password, password, sizeof(WifiCreds.password)) >= sizeof(WifiCreds.password))
      ESP_LOGE(TAG, "Wifi password was trancated");
  if (writeNVS) {
    setWifiCreds();
  }
}

void updateMqttParameters(bool enabled, const char *host, int port, const char *user, const char *password)
{
  OperatingParameters.MqttEnabled = enabled;
  if (strlcpy(OperatingParameters.MqttBrokerHost, host, sizeof(OperatingParameters.MqttBrokerHost)) >= sizeof(OperatingParameters.MqttBrokerHost))
    ESP_LOGE(TAG, "MQTT broker hostname was truncated");
  OperatingParameters.MqttBrokerPort = port;
  if (strlcpy(OperatingParameters.MqttBrokerUsername, user, sizeof(OperatingParameters.MqttBrokerUsername)) >= sizeof(OperatingParameters.MqttBrokerUsername))
    ESP_LOGE(TAG, "MQTT broker username was trancated");
  if (strlcpy(OperatingParameters.MqttBrokerPassword, password, sizeof(OperatingParameters.MqttBrokerPassword)) >= sizeof(OperatingParameters.MqttBrokerPassword))
    ESP_LOGE(TAG, "MQTT broker password was trancated");
  updateThermostatParams();
}

#ifdef MATTER_ENABLED
void updateMatterEnabled(bool enable)
{
  peratingParameters.MatterEnabled = enable;
  updateThermostatParams();
}
#endif

void wifiScanner(void *pvParameters)
{
  void (*callback)() = (void (*)())pvParameters;
  WiFi_ScanSSID();
  //audioBeep();
  callback();
  ntScanTaskHandler = NULL;
  vTaskDelete(NULL);
} 
    
void StartWifiScan(void (*callback)(void))
{   
  //audioBeep();
  
  // Scan for SSIDs. This is done synchronously, so complete it via a RTOS task.
  xTaskCreate(wifiScanner,
    "ScanWifiTask",
    4096,
    (void *)callback,
    tskIDLE_PRIORITY+2,
    &ntScanTaskHandler);
}

extern bool wifiScanActive;

void stopWifiScan()
{
  if (ntScanTaskHandler != NULL)
  {
    vTaskDelete(ntScanTaskHandler);
    ntScanTaskHandler = NULL;
  }
  wifiScanActive = false;
}