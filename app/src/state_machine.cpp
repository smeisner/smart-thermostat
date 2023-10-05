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
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 * 
 */

#include "thermostat.hpp"
#include "driver/gpio.h"
#define HIGH 1
#define LOW 0

OPERATING_PARAMETERS OperatingParameters;
extern int32_t lastTimeUpdate;
int32_t lastWifiReconnect = 0;

void stateMachine(void *parameter)
{
  float currentTemp;
  float minTemp, maxTemp;
  float autoMinTemp, autoMaxTemp;

  for (;;) // infinite loop
  {
    OperatingParameters.lightDetected = readLightSensor();

    currentTemp = OperatingParameters.tempCurrent + OperatingParameters.tempCorrection;
    minTemp = OperatingParameters.tempSet - (OperatingParameters.tempSwing / 2.0);
    maxTemp = OperatingParameters.tempSet + (OperatingParameters.tempSwing / 2.0);

#if 0
    autoMinTemp = OperatingParameters.tempSetAutoMin - (OperatingParameters.tempSwing / 2.0);
    autoMaxTemp = OperatingParameters.tempSetAutoMax + (OperatingParameters.tempSwing / 2.0);
#else
    autoMinTemp = minTemp;
    autoMaxTemp = maxTemp;
#endif

    if (OperatingParameters.hvacSetMode == OFF)
    {
      OperatingParameters.hvacOpMode = OFF;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
    }
    else if (OperatingParameters.hvacSetMode == FAN)
    {
      OperatingParameters.hvacOpMode = FAN;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_FAN_PIN, HIGH);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, HIGH);
    }
    else if (OperatingParameters.hvacSetMode == HEAT)
    {
      if (OperatingParameters.hvacOpMode == COOL)
      {
        gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
      if (currentTemp < minTemp)
      {
        OperatingParameters.hvacOpMode = HEAT;
        gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, HIGH);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, HIGH);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
    }
    else if (OperatingParameters.hvacSetMode == AUX_HEAT)
    {
      //
      // Set up for 2-stage, emergency or aux heat mode
      // Set relays correct
      // For now, just emulate HEAT mode
      //
      if (OperatingParameters.hvacOpMode == COOL)
      {
        // gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
      if (currentTemp < minTemp)
      {
        OperatingParameters.hvacOpMode = HEAT;
        gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, HIGH);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, HIGH);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
    }
    else if (OperatingParameters.hvacSetMode == COOL)
    {
      if (OperatingParameters.hvacOpMode == HEAT)
      {
        gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
      if (currentTemp > maxTemp)
      {
        OperatingParameters.hvacOpMode = COOL;
        gpio_set_level((gpio_num_t)HVAC_COOL_PIN, HIGH);
        gpio_set_level((gpio_num_t)LED_COOL_PIN, HIGH);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
      }
    }
    else if (OperatingParameters.hvacSetMode == AUTO)
    {
      if (currentTemp < autoMinTemp)
      {
        OperatingParameters.hvacOpMode = HEAT;
        gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, HIGH);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, HIGH);
        gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
      else if (currentTemp > autoMaxTemp)
      {
        OperatingParameters.hvacOpMode = COOL;
        gpio_set_level((gpio_num_t)HVAC_COOL_PIN, HIGH);
        gpio_set_level((gpio_num_t)LED_COOL_PIN, HIGH);
        gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
        gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
      }
    }

    if ((currentTemp >= minTemp) && (currentTemp <= maxTemp) && (OperatingParameters.hvacSetMode != FAN) && (OperatingParameters.hvacSetMode != OFF))
    {
      OperatingParameters.hvacOpMode = IDLE;
      gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
      gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
    }

    ld2410_loop();

    if (millis() - lastTimeUpdate > UPDATE_TIME_INTERVAL)
    {
      lastTimeUpdate = millis();
      updateTimeSntp();
    }

    OperatingParameters.wifiConnected = wifiConnected();

#ifdef MQTT_ENABLED
    if ((!OperatingParameters.MqttConnected) && (OperatingParameters.MqttEnabled) && (OperatingParameters.wifiConnected))
      MqttInit();
#endif

    // Check wifi
#ifdef MATTER_ENABLED
    if ((!OperatingParameters.wifiConnected) && (strlen(WifiCreds.ssid)) && (!OperatingParameters.MatterStarted))
#else
    if ((!OperatingParameters.wifiConnected) && (strlen(WifiCreds.ssid)))
#endif
    {
      if (millis() > lastWifiReconnect + 60000)
      {
        lastWifiReconnect = millis();
        startReconnectTask();
      }
    }

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

    /*
     * Test code
     */
    //   static int loop = 0;
    //   if (loop++ >= 40)
    //   {
    // //    Serial.printf ("Value of Motion GPIO %d: %d\n", MOTION_PIN, analogRead(MOTION_PIN));
    // //    Serial.printf ("Value of Motion GPIO %d: %d\n", MOTION_PIN, digitalRead(MOTION_PIN));
    //     digitalWrite (LED_FAN_PIN, digitalRead(MOTION_PIN));

    //     if (digitalRead(MOTION_PIN))
    //       tftMotionTrigger = true;

    //     // digitalWrite (GPIO_NUM_35, HIGH);
    //     // digitalWrite (GPIO_NUM_36, HIGH);
    //     // delay(50);
    //     // digitalWrite (GPIO_NUM_35, LOW);
    //     // digitalWrite (GPIO_NUM_36, LOW);

    //     loop = 0;
    //   }
    /*
     * Test code
     */

    // Pause the task again for 40ms
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

void stateCreateTask()
{
  xTaskCreate(
      stateMachine,
      "State Machine",
      4096,
      NULL,
      tskIDLE_PRIORITY - 1,
      NULL);
}

void testToggleRelays()
{
  vTaskDelay(pdMS_TO_TICKS(500));
  gpio_reset_pin((gpio_num_t)HVAC_HEAT_PIN);
  gpio_set_direction((gpio_num_t)HVAC_HEAT_PIN, GPIO_MODE_OUTPUT);
}

void serialStart()
{
  //
  // Serial port mapping:
  //
  // Serial relates to the onboard UART port
  // Serial1 is mapped (I believe) to the SWD port
  // Serial2 is the USB port directly connected to the ESP32
  //  NB: Serial2 is remapped to provide serial comms with the LD2410
  //
  // Do not use Serial2 as this will disable firmware uploading
  // and debugging via the USB port. It will also cause the USB
  // interface to not show up as a USB device to the host.
  //

  // int32_t tmo = millis() + 60000;
  //Serial.begin(115200);
  // Serial.setDebugOutput(true);
  printf("UART Serial port (via UART-USB adapter -> usually /dev/ttyUSB0)\n");

  // Serial1.begin(115200);
  // Serial1.setDebugOutput(true);
  // Serial1.printf("SERIAL1\n");
}