#pragma once

#include <Arduino.h>
#include <driver/rtc_io.h>

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
    
} OPERATING_PARAMETERS;

typedef struct
{
    char hostname[24];
    char ssid[24];
    char password[16];
} WIFI_CREDS;

extern OPERATING_PARAMETERS OperatingParameters;
extern WIFI_CREDS WifiCreds;

#define MOTION_TIMEOUT 5000
#define WIFI_CONNECT_INTERVAL 3000
//#define UPDATE_TIME_INTERVAL 14400000   // Every 4 hours: 4 * 60 * 60 * 1000(ms)
#define UPDATE_TIME_INTERVAL 60000   // Every 60 seconds


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
bool wifiReconnect(const char *hostname, const char *ssid, const char *pass);
uint16_t wifiSignal();

// TFT
void tftInit();
void tftCreateTask();
// void displaySplash();
// void displayStartDemo();
// void displayDimDemo(int32_t timeDelta, bool abort);
#ifdef __cplusplus
extern "C" {
#endif

void tftDisableTouchTimer();
void tftEnableTouchTimer();
void tftUpdateTouchTimestamp();
void tftWakeDisplay();
void tftDimDisplay();

#ifdef __cplusplus
} /*extern "C"*/
#endif

// Sensors
bool sensorsInit();
void testToggleRelays();
int getTemp();
int getHumidity();

// Indicators
void audioStartupBeep();
void audioBeep();
void indicatorsInit();

// SNTP Time Sync
void initTimeSntp();
void updateTimeSntp();

//
// Define all the GPIO pins used
//

#define LED_BUILTIN 2

#if 0

// HW Version 0.01

#define MOTION_PIN 0
#define BUZZER_PIN 4
#define TOUCH_CS_PIN 5
#define LED_COOL_PIN 9
#define HVAC_FAN_PIN 10
#define HVAC_COOL_PIN 12
#define HVAC_HEAT_PIN 13
#define TFT_CS_PIN 16
#define TFT_CLK_PIN 18
#define TFT_LED_PIN 32
#define TFT_RESET_PIN 33
#define TFT_MISO_PIN 34
#define TOUCH_IRQ_PIN 35
#define LED_IDLE_PIN 36
#define LED_HEAT_PIN 39
#define SDA_PIN 21
#define SCL_PIN 22
#define TFT_MOSI_PIN 23
#define TFT_DC_PIN 27


#else

// HW Version 0.02

#define LED_HEAT_PIN 0
#define BUZZER_PIN 4
#define TOUCH_CS_PIN 5
#define SWD_TDI_PIN 12
#define SWD_TCK_PIN 13
#define SWD_TMS_PIN 14
#define TFT_CS_PIN 16
#define HVAC_HEAT_PIN 17
#define TFT_CLK_PIN 18
#define HVAC_COOL_PIN 19
#define TFT_LED_PIN 32
#define TFT_RESET_PIN 33
#define TFT_MISO_PIN 34
#define TOUCH_IRQ_PIN 35
#define LIGHT_SENS_PIN 36
#define MOTION_PIN 39
#define SDA_PIN 21
#define SCL_PIN 22
#define TFT_MOSI_PIN 23
#define HVAC_FAN_PIN 25
#define LED_COOL_PIN 26
#define TFT_DC_PIN 27
//#define LED_IDLE_PIN -1
#define LED_IDLE_PIN 2

#endif
