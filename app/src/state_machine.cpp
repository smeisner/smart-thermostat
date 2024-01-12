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


void hvacStateUpdate()
{
  float currentTemp;
  float minTemp, maxTemp;
  float autoMinTemp, autoMaxTemp;

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

  if (OperatingParameters.hvacSetMode == OFF)
  {
    if (OperatingParameters.hvacOpMode != OFF)
      ESP_LOGI(__FUNCTION__, "Entering off mode: Current: %.2f", currentTemp);

    OperatingParameters.hvacOpMode = OFF;
    gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
    gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
    gpio_set_level((gpio_num_t)HVAC_FAN_PIN, LOW);
    gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
    gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
    gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
  }
  else if (OperatingParameters.hvacSetMode == FAN_ONLY)
  {
    if (OperatingParameters.hvacOpMode != FAN_ONLY)
      ESP_LOGI(__FUNCTION__, "Entering fan only mode: Current: %.2f", currentTemp);

    OperatingParameters.hvacOpMode = FAN_ONLY;
    gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
    gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
    gpio_set_level((gpio_num_t)HVAC_FAN_PIN, HIGH);
    gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
    gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
    gpio_set_level((gpio_num_t)LED_FAN_PIN, HIGH);
  }
  else if (OperatingParameters.hvacSetMode == HEAT)
  {
    if (currentTemp < minTemp)
    {
      if (OperatingParameters.hvacOpMode != HEAT)
        ESP_LOGI(__FUNCTION__, "Entering heat mode: Current: %.2f  Lo Limit: %.2f", currentTemp, minTemp);

      OperatingParameters.hvacOpMode = HEAT;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, HIGH);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
    }
    else
    {
      // Expected overshoot (where furnace continues to run to dissipate residual heat) is ~0.5F
      // Almost the same in Celcius.
      if (currentTemp > maxTemp - 0.5)
      {
        if (OperatingParameters.hvacOpMode != IDLE)
          ESP_LOGI(__FUNCTION__, "Stopping heat mode: Current: %.2f  Hi Limit: %.2f", currentTemp, maxTemp);

        OperatingParameters.hvacOpMode = IDLE;
        gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)HVAC_FAN_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
    }
  }
  else if (OperatingParameters.hvacSetMode == AUX_HEAT)
  {
    //
    // Set up for 2-stage, emergency or aux heat mode
    // Set relays correct
    //
    //@@@ still todo
    //
    if (currentTemp < minTemp)
    {
      if (OperatingParameters.hvacOpMode != HEAT)
        ESP_LOGI(__FUNCTION__, "Entering aux heat mode: Current: %.2f  Lo Limit: %.2f", currentTemp, minTemp);

      OperatingParameters.hvacOpMode = HEAT;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, HIGH);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
    }
    else
    {
      if (OperatingParameters.hvacOpMode != IDLE)
        ESP_LOGI(__FUNCTION__, "Stopping aux heat mode: Current: %.2f  Hi Limit: %.2f", currentTemp, maxTemp);

      OperatingParameters.hvacOpMode = IDLE;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
    }
  }
  else if (OperatingParameters.hvacSetMode == COOL)
  {
    if (currentTemp > maxTemp)
    {
      if (OperatingParameters.hvacOpMode != COOL)
        ESP_LOGI(__FUNCTION__, "Entering cool mode: Current: %.2f  Hi Limit: %.2f", currentTemp, maxTemp);

      OperatingParameters.hvacOpMode = COOL;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, HIGH);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
    }
    else
    {
      if (OperatingParameters.hvacOpMode != IDLE)
        ESP_LOGI(__FUNCTION__, "Stopping cool mode: Current: %.2f  Lo Limit: %.2f", currentTemp, minTemp);

      OperatingParameters.hvacOpMode = IDLE;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
    }
  }
  else if (OperatingParameters.hvacSetMode == AUTO)
  {
    if (currentTemp < autoMinTemp)
    {
      if (OperatingParameters.hvacOpMode != HEAT)
        ESP_LOGI(__FUNCTION__, "Entering auto heat mode: Current: %.2f  auto min Limit: %.2f", currentTemp, autoMinTemp);

      OperatingParameters.hvacOpMode = HEAT;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, HIGH);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
    }
    else if (currentTemp > autoMaxTemp)
    {
      if (OperatingParameters.hvacOpMode != COOL)
        ESP_LOGI(__FUNCTION__, "Entering auto cool mode: Current: %.2f  auto max Limit: %.2f", currentTemp, autoMaxTemp);

      OperatingParameters.hvacOpMode = COOL;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, HIGH);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
    }
    else
    {
      if (OperatingParameters.hvacOpMode != COOL)
        ESP_LOGI(__FUNCTION__, "Exiting auto heat/cool mode: Current: %.2f  auto min Limit: %.2f  auto max Limit: %.2f", currentTemp, autoMinTemp, autoMaxTemp);

      OperatingParameters.hvacOpMode = IDLE;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
    }
  }

  // if ((currentTemp >= minTemp) && (currentTemp <= maxTemp) && (OperatingParameters.hvacSetMode != FAN_ONLY) && (OperatingParameters.hvacSetMode != OFF))
  // {
  //   OperatingParameters.hvacOpMode = IDLE;
  //   gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
  //   gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
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
      tskIDLE_PRIORITY - 1,
      NULL);
}
