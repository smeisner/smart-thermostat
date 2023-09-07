#pragma once

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <driver/rtc_io.h>

#include "gpio_defs.hpp"

#define HIGH 1
#define LOW 0



/////////////////////////////////////////////////////////////////////
//     Shared data structures
/////////////////////////////////////////////////////////////////////

typedef enum
{
    OFF = 0,
    AUTO,
    HEAT,
    COOL,
    FAN,
    AUX_HEAT,
    ERROR,
    IDLE

} HVAC_MODE;

typedef struct
{
    HVAC_MODE hvacOpMode;
    HVAC_MODE hvacSetMode;
    float tempSet;
    float tempSetAutoMin;
    float tempSetAutoMax;
    float tempCurrent;
    float humidCurrent;
    char tempUnits;
    float tempSwing;
    float tempCorrection;
    float humidityCorrection;
    int lightDetected;
    bool motionDetected;
    bool wifiConnected;
    bool hvacCoolEnable;
    bool hvacFanEnable;
    bool hvac2StageHeatEnable;
    bool hvacReverseValveEnable;

    bool thermostatBeepEnable;
    uint16_t thermostatSleepTime;

    char *timezone;
    uint16_t timezone_sel;

    //@@@ Zipcode for outside temp??
    //@@@ Calibration data for touchscreen?

} OPERATING_PARAMETERS;

typedef struct
{
    char hostname[24];
    char ssid[24];
    char password[16];

} WIFI_CREDS;

typedef struct
{
    /* Same as if_init?? */
    bool driver_started;
    /* Indicates esp_wifi_init() has been called */
    bool if_init;
    /* Indicates esp_wifi_start() has been called */
    bool wifi_started;
    /* Indicates active wifi connection to AP established */
    bool Connected;
    /* IP address fetched when AP connect happened */
    esp_ip4_addr_t ip;  // uint32_t addr;  /*!< IPv4 address */

} WIFI_STATUS;

extern OPERATING_PARAMETERS OperatingParameters;
extern WIFI_CREDS WifiCreds;
extern int32_t ui_WifiStatusLabel_timestamp;

#define MOTION_TIMEOUT 10000
#define WIFI_CONNECT_INTERVAL 30000
#define UPDATE_TIME_INTERVAL 60000
#define UI_TEXT_DELAY 3000

extern const char *gmt_timezones[];


/////////////////////////////////////////////////////////////////////
//          Forward Declarations
/////////////////////////////////////////////////////////////////////

#ifdef __cplusplus  //@@@ Needed?
extern "C" {
#endif

extern void app_main();

#ifdef __cplusplus
} /*extern "C"*/
#endif

int32_t millis();   // Defined in main.cpp

void showConfigurationData();
void scanI2cBus();

// State Machine
void stateCreateTask();
void serialStart();
extern int32_t lastWifiReconnect;

// EEPROM
void eepromInit();
void clearNVS();
bool eepromUpdateHvacSetTemp();
bool eepromUpdateHvacSetMode();
void setWifiCreds();
void updateThermostatParams();

// HTTP Server
void webStart();

// Routine to control wifi
void wifiSetHostname(const char *hostname);
void wifiSetCredentials(const char *ssid, const char *pass);
bool wifiStart(const char *hostname, const char *ssid, const char *pass);
bool wifiConnected();
void WifiDisconnect();
//bool wifiReconnect(const char *hostname, const char *ssid, const char *pass);
void startReconnectTask();
uint16_t wifiSignal();
char *wifiAddress();
char *Get_WiFiSSID_DD_List( void );
void WiFi_ScanSSID( void );

// TFT
void tftInit();
void tftCalibrateTouch();
void tftCreateTask();
HVAC_MODE strToHvacMode(char *mode);
HVAC_MODE convertSelectedHvacMode();
void setHvacModesDropdown();
const char *hvacModeToString(HVAC_MODE mode);
extern volatile bool tftMotionTrigger;

#ifdef __cplusplus
extern "C" {
#endif

void tftDisableTouchTimer();
void tftEnableTouchTimer();
void tftUpdateTouchTimestamp();
void tftWakeDisplay(bool beep);
void tftDimDisplay();
// void tftWakeDisplayMotion();

#ifdef __cplusplus
} /*extern "C"*/
#endif

// Sensors
// float roundValue(float value, int places = 0);
float roundValue(float value, int places);
float getRoundedFrac(float value);
// int degCfrac(float tempF);
// int tempOut(float tempF);
// float tempIn(float tempC);
// float degFtoC(float degF);
void resetTempSmooth();
bool sensorsInit();
void testToggleRelays();
int getTemp();
int getHumidity();
void ld2410_loop();
int readLightSensor();

// Indicators
void audioStartupBeep();
void indicatorsInit();
void audioBeep();

// SNTP Time Sync
void updateTimezone();
bool getLocalTime(struct tm *, uint32_t);
void updateTimeSntp();

// ui_events.cpp
bool isCurrentScreenMain();
