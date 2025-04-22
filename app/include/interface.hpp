#pragma once
#include <stdint.h>
#include "thermostat.hpp"
#include "display.hpp"

void updateCalibration(calibration_t *cal);
void updateTempUnits(char units);
void updateHVACSettings(bool newStages, bool newRV, bool newAux, bool newCool, bool newFan);
void updateBeepEnable(bool enabled);
void updateTimezone(int idx, bool hasDST);
void updateFriendlyHost(const char *host);
void updateWifiCreds(const char *ssid, const char *password, bool updateNVS=true);
void updateMqttParameters(bool enabled, const char *host, int port, const char *user, const char *password);
void updateMatterEnabled(bool enable);

//const char *Get_WiFiSSID_DD_List( void );
void StartWifiScan(void (*callback)(void));
void stopWifiScan();
