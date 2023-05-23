#pragma once

#include <Arduino.h>
#include <driver/rtc_io.h>
#include "gpio_defs.hpp"

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

} OPERATING_PARAMETERS;

typedef struct
{
    char hostname[24];
    char ssid[24];
    char password[16];

} WIFI_CREDS;

extern OPERATING_PARAMETERS OperatingParameters;
extern WIFI_CREDS WifiCreds;
extern int32_t ui_WifiStatusLabel_timestamp;

#define MOTION_TIMEOUT 5000
#define WIFI_CONNECT_INTERVAL 3000
#define UPDATE_TIME_INTERVAL 60000
#define UI_TEXT_DELAY 3000

static const char *gmt_timezones[] = 
  {"GMT-12", "GMT-11", "GMT-10", "GMT-9", "GMT-8", "GMT-7", "GMT-6", "GMT-5", "GMT-4", "GMT-3", "GMT-2",  "GMT-1"
   "GMT",    "GMT+1",  "GMT+2",  "GMT+3", "GMT+4", "GMT+5", "GMT+6", "GMT+7", "GMT+8", "GMT+9", "GMT+10", "GMT+11"};


/////////////////////////////////////////////////////////////////////
//          Forward Declarations
/////////////////////////////////////////////////////////////////////

// State Machine
void stateCreateTask();
void serialStart();

// EEPROM
void eepromInit();
void clearNVS();
bool eepromUpdateHvacSetTemp();
bool eepromUpdateHvacSetMode();

// HTTP Server
void webInit();
void webPump();
const char *hvacModeToString(HVAC_MODE mode);

// Routine to control wifi
bool wifiStart(const char *hostname, const char *ssid, const char *pass);
bool wifiConnected();
void WifiDisconnect();
bool wifiReconnect(const char *hostname, const char *ssid, const char *pass);
uint16_t wifiSignal();
char *wifiAddress();
char *Get_WiFiSSID_DD_List( void );
void WiFi_ScanSSID( void );

// TFT
void tftInit();
void tftCreateTask();
void setHvacModesDropdown();

#ifdef __cplusplus
extern "C" {
#endif

void tftDisableTouchTimer();
void tftEnableTouchTimer();
void tftUpdateTouchTimestamp();
void tftWakeDisplay(bool beep);
void tftDimDisplay();

#ifdef __cplusplus
} /*extern "C"*/
#endif

// Sensors
int degCfrac(float tempF);
int tempOut(float tempF);
float tempIn(float tempC);
float degFtoC(float degF);
bool sensorsInit();
void testToggleRelays();
int getTemp();
int getHumidity();
void updateTimezone();

// Indicators
void audioStartupBeep();
void indicatorsInit();
void audioBeep();

// SNTP Time Sync
void initTimeSntp();
void updateTimeSntp();
