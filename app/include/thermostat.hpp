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

enum logic_levels {LOW, HIGH};

/////////////////////////////////////////////////////////////////////
//     Shared data structures
/////////////////////////////////////////////////////////////////////

typedef struct
{
    uint16_t systemErrors;
    uint16_t hardwareErrors;
    uint16_t wifiErrors;
#ifdef MQTT_ENABLED
    uint16_t mqttConnectErrors;
    uint16_t mqttProtocolErrors;
#endif
#ifdef MATTER_ENABLED
    uint16_t matterConnectErrors;
#endif
#ifdef TELNET_ENABLED
    uint16_t telnetNetworkErrors;
#endif
} ERRORS;

/*
 * HVAC_MODE enum modes declared as listed at https://www.home-assistant.io/integrations/climate.mqtt/
 * to facilitate integration with Home Assitant
 */
typedef enum
{
    OFF = 0,
    HEAT,
    COOL,
    DRY,
    IDLE,
    FAN_ONLY,
    AUTO,
    AUX_HEAT,
    ERROR,
    NR_HVAC_MODES
} HVAC_MODE;

typedef struct
{
    ERRORS Errors;
    // FriendlyName and DeviceName must be
    // the same size due to copy operation
    // in ui_events.cpp, saveDeviceName()
    char FriendlyName[32];
    char DeviceName[32];
    uint8_t mac[6];
    char ld2410FirmWare[24];
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

#ifdef MQTT_ENABLED
    bool MqttEnabled;
    // bool MqttStarted;
    bool MqttConnected;
    char MqttBrokerHost[32];
    uint16_t MqttBrokerPort;
    char MqttBrokerUsername[32];
    char MqttBrokerPassword[72];
    void *MqttClient;   // Used during MQTT publish
#endif

#ifdef MATTER_ENABLED
    bool MatterEnabled;
    bool MatterStarted;
#endif
    //@@@ Zipcode for outside temp??
    //@@@ Calibration data for touchscreen?

} OPERATING_PARAMETERS;

typedef struct
{
    // char hostname[24];   ...replaced by OperatingParameters.DeviceName
    char ssid[33];
    char password[64];

} WIFI_CREDS;

typedef struct
{
    /* Same as if_init?? */
    bool driver_started;
    /* Indicates esp_wifi_init() has been called */
    bool if_init;
    /* Indicates esp_wifi_start() has been called */
    bool wifi_started;
    /* A wifi reconnect has been requested */
    bool reconnect_requested;
    /* Indicates active wifi connection to AP established */
    bool Connected;
    /* IP address fetched when AP connect happened */
    esp_ip4_addr_t ip;  // uint32_t addr;  /*!< IPv4 address */

} WIFI_STATUS;


extern OPERATING_PARAMETERS OperatingParameters;
extern WIFI_CREDS WifiCreds;
extern int64_t ui_WifiStatusLabel_timestamp;

#ifdef MQTT_ENABLED
#define MQTT_RECONNECT_DELAY 75000
#endif
#define MOTION_TIMEOUT 10000
#define WIFI_CONNECT_INTERVAL 30000
#define UPDATE_TIME_INTERVAL 300000  //60000
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

int64_t millis();   // Defined in main.cpp

void showConfigurationData();
void scanI2cBus();

#ifdef MATTER_ENABLED
bool MatterInit();
#endif

#ifdef MQTT_ENABLED
void MqttInit();
bool MqttConnect();
bool MqttReconnect();
void MqttUpdateStatusTopic();
void MqttMotionUpdate(bool);
void MqttHomeAssistantDiscovery();
#endif

#ifdef TELNET_ENABLED
esp_err_t telnetStart();
void terminateTelnetSession();
bool telnetServiceRunning();
#else
static inline esp_err_t telnetStart(void) {return ESP_FAIL;}
static inline void terminateTelnetSession(void) {}
static inline bool telnetServiceRunning(void) {return false;}
#endif

// State Machine
void stateCreateTask();
extern int64_t lastWifiReconnect;

// EEPROM
void eepromInit();
void clearNVS();
bool eepromUpdateHvacSetTemp();
bool eepromUpdateHvacSetMode();
void setWifiCreds();
void updateThermostatParams();

bool eepromUpdateArbFloat(const char *key, float value);
bool eepromUpdateArbMode(const char *key, HVAC_MODE mode);

// HTTP Server
void webStart();

// Routine to control wifi
void WifiSetHostname(const char *hostname);
void WifiSetCredentials(const char *ssid, const char *pass);
bool WifiStart(const char *hostname, const char *ssid, const char *pass);
bool WifiStarted();
bool WifiRestartPending();
bool WifiConnected();
void WifiDisconnect();
void startReconnectTask();
uint16_t WifiSignal();
char *WifiAddress();
char *Get_WiFiSSID_DD_List( void );
void WiFi_ScanSSID( void );

// TFT
void tftInit();
void tftCalibrateTouch();
void tftCreateTask();
HVAC_MODE strToHvacMode(char *mode);
HVAC_MODE convertSelectedHvacMode();
void setHvacModesDropdown();
#ifdef MQTT_ENABLED
const char *hvacModeToMqttOpMode(HVAC_MODE mode);
const char *hvacModeToMqttCurrMode(HVAC_MODE mode);
#endif
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
void updateHvacMode(HVAC_MODE mode);
void updateEnabledHvacModes();
void updateHvacSetTemp(float setTemp);
// float roundValue(float value, int places = 0);
float roundValue(float value, int places);
float getRoundedFrac(float value);
void initTimeSntp();
void resetTempSmooth();
void sensorsInit();
void initRelays();
int getTemp();
int getHumidity();
void ld2410_loop();
int readLightSensor();

// Indicators
void audioStartupBeep();
void indicatorsInit();
void audioBeep();

// SNTP Time Sync
void updateTimezoneFromConfig();
bool getLocalTime(struct tm *, uint64_t);
void updateTimeSntp();

// ui_events.cpp
bool isCurrentScreenMain();
