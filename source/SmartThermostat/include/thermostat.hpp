#pragma once

#include <driver/rtc_io.h>

// State Machine
void stateCreateTask();
void serialStart();

// HTTP Server
void webInit();
void webPump();

// Routine to control wifi
bool wifiStart();
bool wifiConnected();

// TFT
void tftInit();
void tftCreateTask();
void displaySplash();
void displayStartDemo();
void displayDimDemo(int32_t timeDelta, bool abort);

// Sensors
bool sensorsInit();
void testToggleRelays();

// Indicators
void audioStartupBeep();
void audioBeep();
void indicatorsInit();



//
// Define all the GPIO pins used
//

#define LED_BUILTIN 2

#if 1

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

#endif
