// SPDX-License-Identifier: GPL-3.0-only
/*
 * display_thread.cpp
 *
 * Implement the RTOS thread to handle motion and touch events
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * History
 *  05-Apr-2025: Geoffrey Hausheer (rc2012@pblue.org) - Reworked to separate configuration, UI and backlight code
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 *  11-Oct-2023: Steve Meisner (steve@meisners.net) - Add suport for home automation (MQTT & Matter)
 *  04-Dec-2023: Michael Burke (michaelburke2000@gmail.com) - Add screen brightness auto-adjustment
 */

#include "thermostat.hpp"
#include "display.hpp"
#include "ui.hpp"
#include "ui/ui.h"

#define OFF_BRIGHTNESS 0
#define MIN_BRIGHTNESS 80
#define FULL_BRIGHTNESS 4096

static char thermostatModes[48] = {0};
TaskHandle_t xTouchUIHandle;
int64_t lastTouchDetected = 0;
bool tftTouchTimerEnabled = true;
int64_t ui_WifiStatusLabel_timestamp = 0;
int64_t lastMotionDetected = 0;
bool tftAwake = true;

volatile bool tftMotionTrigger = false;

#define TAG "Display"

void tftDisableTouchTimer()
{
  ESP_LOGI(TAG, "Disabling touch/motion timer");
  tftTouchTimerEnabled = false;
}

void tftEnableTouchTimer()
{
  ESP_LOGI(TAG, "Enabling touch/motion timer");
  tftTouchTimerEnabled = true;
}

void tftUpdateTouchTimestamp()
{
  lastTouchDetected = millis();
}


int sensorToScreenBrightness(int sensor)
{
  // THIS 4095 IS THE MAX BRIGHTNESS READING OF THE LIGHT SENSOR.
  return sensor * FULL_BRIGHTNESS / 4095;
}

#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : x))
#define BRIGHTNESS_STEP_SIZE 2
void tftAutoBrightness()
{
  int curBrightness = getBrightness();
  int convertedLightLevel = sensorToScreenBrightness(OperatingParameters.lightDetected);
  int adjustment;
  // Screen will only adjust brightness if difference between intended and current brightnesses is large enough.
  // (this difference threshold is arbitrarily chosen to be FULL_BRIGHTNESS >> 5, or 1/32nd of the range of
  // possible brightnesses)
  // Also adjust brightness if screen is currently brightness 0 (just now waking up from sleep). Without this
  // `OR`, the screen won't turn back on in very low ambient light.
  if ((abs(convertedLightLevel - curBrightness) > FULL_BRIGHTNESS >> 5) || curBrightness == 0)
    adjustment = BRIGHTNESS_STEP_SIZE;
  else
    return;
  adjustment = curBrightness > convertedLightLevel ? adjustment * -1 : adjustment;
  ESP_LOGI(TAG, "Seeting brightness to %d", CLAMP(curBrightness + adjustment, MIN_BRIGHTNESS, FULL_BRIGHTNESS));
  setBrightness(CLAMP(curBrightness + adjustment, MIN_BRIGHTNESS, FULL_BRIGHTNESS));
}
#undef CLAMP

void tftWakeDisplay(bool beep)
{
  ESP_LOGI(TAG, "TFT Wakeup");
  tftEnableTouchTimer();
  tftUpdateTouchTimestamp();
  setBrightness(FULL_BRIGHTNESS);
  uiWake(beep);
  tftAwake = true;
}

void tftWakeDisplayMotion()
{
  if (!tftTouchTimerEnabled)
    tftWakeDisplay(false);
}

void tftDimDisplay()
{
  if (tftTouchTimerEnabled)
  {
    ESP_LOGD(TAG, "Dimming display");
    setBrightness(OFF_BRIGHTNESS);
    uiDim();
    tftDisableTouchTimer();
    tftAwake = false;
  }
}

void tftPump(void * parameter)
{
  ESP_LOGI(TAG, "Starting display update loop");
  uiInit();
  for(;;) // infinite loop
  {
    uiUpdate();

    if (millis() - lastTouchDetected > OperatingParameters.thermostatSleepTime * 1000)
    {
      //
      // Only enter display sleep if the motionTrigger is false and
      // there is no motion currently detected. The second test is to
      // avoid a race condition where the timeout has passed but motion
      // was detected again. This causes the display to shut off and turn
      // back on quickly. Annoying...
      //
      if ((!tftMotionTrigger) && (gpio_get_level((gpio_num_t)MOTION_PIN) == LOW))
      {
        lastTouchDetected = 0;
        tftDimDisplay();
      }
    }

  // Was the motion detection triggered?
  if (tftMotionTrigger)
  {
    tftMotionTrigger = false;
    //
    // This only matters if the touchTimer is already disabled (so the display is sleeping)
    // and the current screen is the cefault (main screen).
    //
    if (! tftAwake)
    {
      ESP_LOGI (TAG, "Motion wake triggered");
      lastMotionDetected = millis();
      OperatingParameters.motionDetected = true;
      tftWakeDisplayMotion();
#ifdef MQTT_ENABLED
      if (OperatingParameters.MqttConnected)
        MqttMotionUpdate(OperatingParameters.motionDetected);
#endif
    }
  }

  //
  // If we did detect motion and now the MOTION pin is low, 
  // reset the status ...if the MOTION_TIMEOUT period has passed.
  //
  if ((OperatingParameters.motionDetected) && (gpio_get_level((gpio_num_t)MOTION_PIN) == LOW))
  {
    if (millis() - lastMotionDetected > MOTION_TIMEOUT)
    {
      ESP_LOGI (TAG, "Motion detection timeout");
      OperatingParameters.motionDetected = false;
#ifdef MQTT_ENABLED
      // Stay in synch with OperatingParameter
      //@@@if (OperatingParameters.MqttConnected)
        //@@@MqttMotionUpdate(OperatingParameters.motionDetected);
#endif
    }
  }

    // Pause the task again for 10ms
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void tftCreateTask()
{
  xTaskCreate (
      tftPump,
      "Touch Screen UI",
      8192,
      NULL,
      tskIDLE_PRIORITY+1,
      &xTouchUIHandle
  );
}
