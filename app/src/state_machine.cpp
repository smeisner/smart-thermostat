// SPDX-License-Identifier: GPL-3.0-only
/*
 * state_machine.cpp
 *
 * Implements the state machine to control the HVAC actions and modes.
 * As part of this responsibility, statuses of items such as wifi connection
 * are also monitored and action taken if necessary.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * Notes:
 *   Need to add support for reversing valve (when aux heat enabled)
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 *  11-Oct-2023: Steve Meisner (steve@meisners.net) - Add suport for home automation (MQTT & Matter)
 * 
 */
#include <stdbool.h>
#include "thermostat.hpp"
#include "driver/gpio.h"
#define HIGH 1
#define LOW 0

OPERATING_PARAMETERS OperatingParameters;
extern int64_t lastTimeUpdate;
int64_t lastWifiReconnect = millis();
#ifdef MQTT_ENABLED
int64_t lastMqttReconnect = 0;
#endif
static bool MqttConnectCalled = false;


struct gpio_pin_desc {
  short pin;
  short level;
};

#define NR_GPIO_PINS      (8)
#define INVALID_PIN()     {.pin=-1, .level=-1} /* plays as an arbitrary stop guard for the iterator code */
#define PIN(p, l)         {.pin=(p), .level=(l)}
#define DESC(mode, ...)   [(mode)] = {__VA_ARGS__, INVALID_PIN()}

/*
 * NOTE: g++ doesn't support C's non-trivial designated initializers,
 * so the array must be initilazed in the HVAC_MODE enum order
 */
const struct gpio_pin_desc hvac_mode_gpio[NR_HVAC_MODES][NR_GPIO_PINS] = {
  DESC(OFF, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, LOW),
            PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, LOW),
            PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(HEAT, PIN(HVAC_HEAT_PIN, HIGH), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, LOW),
             PIN(LED_HEAT_PIN, HIGH), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, LOW),
             PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(COOL, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, HIGH), PIN(HVAC_FAN_PIN, LOW),
             PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, HIGH), PIN(LED_FAN_PIN, LOW),
             PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(DRY, INVALID_PIN()),
  DESC(IDLE, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, LOW),
             PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, LOW),
             PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(FAN_ONLY, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, HIGH),
                 PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, HIGH),
                 PIN(HVAC_STAGE2_PIN, LOW)),
};

static inline bool is_invalid_desc(const struct gpio_pin_desc desc)
{
  struct gpio_pin_desc inv = INVALID_PIN();

  if (desc.pin == inv.pin && desc.level == inv.level)
    return true;

  return false;
}

static void set_hvac_mode(HVAC_MODE mode)
{
  const struct gpio_pin_desc *desc = hvac_mode_gpio[mode];

  for (int i = 0; i < NR_GPIO_PINS; i++) {
    if (is_invalid_desc(desc[i]))
      break;

    gpio_set_level((gpio_num_t)desc[i].pin, (uint32_t)desc[i].level);
  }

  OperatingParameters.hvacOpMode = mode;
}

#define COND_LOG(cond, log_msg, ...)              \
do {                                              \
  if ((cond)) {                                   \
    ESP_LOGI(__FUNCTION__, log_msg, __VA_ARGS__); \
  }                                               \
} while(0)

void hvacStateUpdate()
{
  float currentTemp;
  float minTemp, maxTemp;
  float autoMinTemp, autoMaxTemp;
  HVAC_MODE prev_mode = OperatingParameters.hvacOpMode;

  currentTemp = OperatingParameters.tempCurrent + OperatingParameters.tempCorrection;
  minTemp = OperatingParameters.tempSet - (OperatingParameters.tempSwing / 2.0);
  maxTemp = OperatingParameters.tempSet + (OperatingParameters.tempSwing / 2.0);

// The following code is for later when auto mode is supported fully
#if 0
    autoMinTemp = OperatingParameters.tempSetAutoMin - (OperatingParameters.tempSwing / 2.0);
    autoMaxTemp = OperatingParameters.tempSetAutoMax + (OperatingParameters.tempSwing / 2.0);
#else
    autoMinTemp = minTemp;
    autoMaxTemp = maxTemp;
#endif

  switch (OperatingParameters.hvacSetMode) {
  case OFF:
    set_hvac_mode(OFF);
    COND_LOG(prev_mode != OFF, "Entering off mode: Current: %.2f", currentTemp);
    break;
  case FAN_ONLY:
    set_hvac_mode(FAN_ONLY);
    COND_LOG(prev_mode != FAN_ONLY, "Entering fan only mode: Current: %.2f", currentTemp);
    break;
  case HEAT:
    if (currentTemp < minTemp) {
      set_hvac_mode(HEAT);
      COND_LOG(prev_mode != HEAT, "Entering heat mode: Current: %.2f  Lo Limit: %.2f", currentTemp, minTemp);
    } else {
      // Expected overshoot (where furnace continues to run to dissipate residual heat) is ~0.5F
      // Almost the same in Celcius.
      if (currentTemp > maxTemp - 0.5) {
        set_hvac_mode(IDLE);
        COND_LOG(prev_mode != IDLE, "Stopping heat mode: Current: %.2f  Hi Limit: %.2f", currentTemp, maxTemp);
      }
    }
    break;
  case AUX_HEAT:
    //
    // Set up for 2-stage, emergency or aux heat mode
    // Set relays correct
    //
    //@@@ still todo
    //
    if (currentTemp < minTemp) {
      set_hvac_mode(HEAT);
      COND_LOG(prev_mode != HEAT, "Entering aux heat mode: Current: %.2f  Lo Limit: %.2f", currentTemp, minTemp);
    } else {
      set_hvac_mode(IDLE);
      COND_LOG(prev_mode != IDLE, "Stopping aux heat mode: Current: %.2f  Hi Limit: %.2f", currentTemp, maxTemp);
    }
    break;
  case COOL:
    if (currentTemp > maxTemp) {
      set_hvac_mode(COOL);
      COND_LOG(prev_mode != COOL, "Entering cool mode: Current: %.2f  Hi Limit: %.2f", currentTemp, maxTemp);
    } else {
      set_hvac_mode(IDLE);
      COND_LOG(prev_mode != IDLE, "Stopping cool mode: Current: %.2f  Lo Limit: %.2f", currentTemp, minTemp);
    }
    break;
  case AUTO:
    if (currentTemp < autoMinTemp) {
      set_hvac_mode(HEAT);
      COND_LOG(prev_mode != HEAT, "Entering auto heat mode: Current: %.2f  auto min Limit: %.2f", currentTemp, autoMinTemp);
    } else if (currentTemp > autoMaxTemp) {
      set_hvac_mode(COOL);
      COND_LOG(prev_mode != COOL, "Entering auto cool mode: Current: %.2f  auto max Limit: %.2f", currentTemp, autoMaxTemp);
    } else {
      set_hvac_mode(IDLE);
      COND_LOG(prev_mode != COOL, "Exiting auto heat/cool mode: Current: %.2f  auto min Limit: %.2f  auto max Limit: %.2f", currentTemp, autoMinTemp, autoMaxTemp);
    }
    break;
  }
  // if ((currentTemp >= minTemp) && (currentTemp <= maxTemp) && (OperatingParameters.hvacSetMode != FAN_ONLY) && (OperatingParameters.hvacSetMode != OFF))
  // {
  //   OperatingParameters.hvacOpMode = IDLE;
  //   gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
  //   gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
  //   gpio_set_level((gpio_num_t)HVAC_STAGE2_PIN, LOW); //@@ Not used yet
  //   gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
  //   gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
  //   gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
  // }
}


void stateMachine(void *parameter)
{
#ifdef MQTT_ENABLED
  // This will cause the first pass to make a connect attempt
  lastMqttReconnect = MQTT_RECONNECT_DELAY * -1;
#endif

  for (;;) // infinite loop
  {
    // Update sensor readings
    OperatingParameters.lightDetected = readLightSensor();
    // Update state of motion sensor
    ld2410_loop();

    // Update HVAC State machine
    hvacStateUpdate();

    // Check wifi
    // Update wifi connection status
    OperatingParameters.wifiConnected = wifiConnected();

#ifdef MATTER_ENABLED
    if ((!OperatingParameters.wifiConnected) && (strlen(WifiCreds.ssid)) && (!OperatingParameters.MatterStarted))
#else
    if ((!OperatingParameters.wifiConnected) && (strlen(WifiCreds.ssid)))
#endif
    {
      if (millis() > lastWifiReconnect + WIFI_CONNECT_INTERVAL)
      {
        lastWifiReconnect = millis();
        startReconnectTask();
      }
    }

    // Determine if it's time to update the SNTP sourced clock
    if (millis() - lastTimeUpdate > UPDATE_TIME_INTERVAL)
    {
      lastTimeUpdate = millis();
      updateTimeSntp();
    }

#ifdef MQTT_ENABLED
    //@@ Must be cleaned up. This (theoretically) should only execute
    // once to establish the MQTT connection.

    if ((OperatingParameters.wifiConnected) && (OperatingParameters.MqttEnabled) && (!OperatingParameters.MqttConnected))
    {
      if (millis() > (lastMqttReconnect + MQTT_RECONNECT_DELAY))
      {
        lastMqttReconnect = millis();
        //@@@ Should be a "reconnect"
        if (!MqttConnectCalled)
        {
          MqttConnectCalled = true;
          MqttConnect();
        }
        else
        {
          MqttReconnect();
        }
      }
    }
#endif

//@@@ Old code, but may be useful for configuring via the USB serial port...

    // if (Serial.available())
    // {
    //   String temp = Serial.readString();
    //   printf ("[USB] %s\n", temp);
    //   if (temp.indexOf("reset") > -1)
    //     ESP.restart();
    //   // if (temp.indexOf("scan") > -1)
    //   //   scanI2cBus();
    //   if (temp.indexOf("temp") > -1)
    //   {
    //     printf ("Current Set Temp: %.1f\n", OperatingParameters.tempSet);
    //   }
    // }

    // Pause the task again for 40ms
    vTaskDelay(pdMS_TO_TICKS(40));
  } // end for loop
}

void stateCreateTask()
{
  xTaskCreate(
      stateMachine,
      "State Machine",
      8192,
      NULL,
      tskIDLE_PRIORITY, // - 1,
      NULL);
}
