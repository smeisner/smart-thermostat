// SPDX-License-Identifier: GPL-3.0-only
/*
 * tft.cpp
 *
 * This module implements the driver for the TFT touch screen. Functions
 * to calibrate the tocuhscreen, provide an interface between the thermostat
 * and TFT specific functionality and control the TFT backlight are included.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * Notes:
 * 
 * The graphics tool used to generate the menus and screen is SquareLine Studio.
 * This product is subscrition based. More info can be found here:
 * https://squareline.io/
 * 
 * All data files exported from SquareLine Studio are included in the folder
 * ./ui. The project files are located in this folder with the extensions .sli and .spj.
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 *  11-Oct-2023: Steve Meisner (steve@meisners.net) - Add suport for home automation (MQTT & Matter)
 * 
 */

#include "thermostat.hpp"
#include "tft.hpp"

#define OFF_BRIGHTNESS 0
#define FULL_BRIGHTNESS 255

static char thermostatModes[48] = {0};
TaskHandle_t xTouchUIHandle;
int32_t lastTouchDetected = 0;
bool tftTouchTimerEnabled = true;
int32_t ui_WifiStatusLabel_timestamp = 0;
uint16_t calData_2_8[8] = { 3839, 336, 3819, 3549, 893, 390, 718, 3387 };
uint16_t calData_3_2[8] = { 3778, 359, 3786, 3803, 266, 347, 258, 3769 };
uint16_t calData[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int32_t lastMotionDetected = 0;

volatile bool tftMotionTrigger = false;

#ifdef __cplusplus
extern "C" {
#endif

void tftDisableTouchTimer()
{
  printf ("Disabling touch timer\n");
  tftTouchTimerEnabled = false;
}
void tftEnableTouchTimer()
{
  printf ("Enabling touch timer\n");
  tftTouchTimerEnabled = true;
}

void tftUpdateTouchTimestamp()
{
  lastTouchDetected = millis();
}

void tftShowDisplayItems()
{
  lv_obj_clear_flag(ui_ModeDropdown, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_TempDecreaseBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_TempIncreaseBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_InfoBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_SetupBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_TempArc, LV_OBJ_FLAG_HIDDEN);
}

void tftHideDisplayItems()
{
  lv_obj_add_flag(ui_ModeDropdown, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TempDecreaseBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TempIncreaseBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_InfoBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SetupBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TempArc, LV_OBJ_FLAG_HIDDEN);
}

void tftWakeDisplay(bool beep)
{
  if (beep)
    audioBeep();
  tftShowDisplayItems();
  tft.setBrightness(FULL_BRIGHTNESS);
  tftEnableTouchTimer();
  tftUpdateTouchTimestamp();
}

void tftWakeDisplayMotion()
{
  if (!tftTouchTimerEnabled)
  {
    float ratio = (float)(OperatingParameters.lightDetected) / 4095.0;
    // float ratio = (float)(OperatingParameters.lightDetected) / 3150.0;  // Measured in mV (max = 3.15 V)
    int brightness = (int)(ratio * (float)(FULL_BRIGHTNESS));
    tftShowDisplayItems();
    printf ("Setting display to partial brightness: %d (%d%%)\n",
      brightness, (int)(ratio*100.0));
    tft.setBrightness(brightness);
    tftEnableTouchTimer();
    tftUpdateTouchTimestamp();
  }
}

void tftDimDisplay()
{
  if (tftTouchTimerEnabled)
  {
    tft.setBrightness(OFF_BRIGHTNESS);
    tftHideDisplayItems();
    tftDisableTouchTimer();
  }
}

#ifdef __cplusplus
} /*extern "C"*/
#endif

void tftUpdateDisplay()
{
  static char buffer[16];
  static struct tm local_time;
  // static ulong last = 0;

  // updateTime() will return false if wifi is not connected. 
  // Otherwise pending on SNTP call will cause a stall.
  // if (!updateTime(&local_time))
  // if (millis() - last >  5000)
  // {
    // last = millis();
    if (getLocalTime(&local_time, 80))
    {
      strftime(buffer, sizeof(buffer), "%H:%M:%S", &local_time);
    }
    else
    {
      strcpy (buffer, "--:--:--");
    }
    lv_label_set_text(ui_TimeLabel, buffer);
  // }

  lv_label_set_text_fmt(ui_TempLabel, "%d°", getTemp());
  lv_label_set_text_fmt(ui_HumidityLabel, "%d%%", getHumidity());

  lv_arc_set_value(ui_TempArc, OperatingParameters.tempSet*10.0);
  lv_label_set_text_fmt(ui_SetTemp, "%d°", (int)OperatingParameters.tempSet);
  if (OperatingParameters.tempUnits == 'C')
  {
    lv_label_set_text_fmt(ui_SetTempFrac, "%d", (int)getRoundedFrac(OperatingParameters.tempSet));
  }

  lv_dropdown_set_selected(ui_ModeDropdown, convertSelectedHvacMode());

  if (wifiConnected())
    lv_label_set_text(ui_WifiIndicatorLabel, "#0000A0 " LV_SYMBOL_WIFI);
  else
    lv_label_set_text(ui_WifiIndicatorLabel, "#d0e719 " LV_SYMBOL_WIFI);

  switch (OperatingParameters.hvacOpMode)
  {
    // Set color of inner circle representing the operating mode
    case HEAT:     lv_obj_set_style_bg_color(ui_SetTempBg, lv_color_hex(0xa00b0b), LV_PART_MAIN); break;
    case COOL:     lv_obj_set_style_bg_color(ui_SetTempBg, lv_color_hex(0x435deb), LV_PART_MAIN); break;
    case FAN_ONLY: lv_obj_set_style_bg_color(ui_SetTempBg, lv_color_hex(0x3c8945), LV_PART_MAIN); break;  //@@@
    default:       lv_obj_set_style_bg_color(ui_SetTempBg, lv_color_hex(0x7a92b2), LV_PART_MAIN); break;
  }
  switch (OperatingParameters.hvacSetMode)
  {
    // Set color of outer circle representing the enabled or set mode
    case AUX_HEAT:
    case HEAT:     lv_obj_set_style_bg_color(ui_SetTempBg1, lv_color_hex(0xc71b1b), LV_PART_MAIN); break;
    case COOL:     lv_obj_set_style_bg_color(ui_SetTempBg1, lv_color_hex(0x1b7dc7), LV_PART_MAIN); break;
    case FAN_ONLY: lv_obj_set_style_bg_color(ui_SetTempBg1, lv_color_hex(0x23562b), LV_PART_MAIN); break;  //@@@
    case AUTO:     lv_obj_set_style_bg_color(ui_SetTempBg1, lv_color_hex(0xaeac40), LV_PART_MAIN); break;
    default:       lv_obj_set_style_bg_color(ui_SetTempBg1, lv_color_hex(0x7d7d7d), LV_PART_MAIN); break;
  }
}

#ifdef MQTT_ENABLED
const char *hvacModeToMqttCurrMode(HVAC_MODE mode)
{
  switch (mode)
  {
    case OFF: return "off";
    case IDLE: return "idle";
    case AUTO: return "auto";
    case HEAT: return "heating";
    case COOL: return "cooling";
    case FAN_ONLY: return "Fan_only";
    case AUX_HEAT: return "Aux Heat";
    default: return "Error";
  }
}
#endif

const char *hvacModeToString(HVAC_MODE mode)
{
  switch (mode)
  {
    case OFF: return "Off";
    case IDLE: return "Idle";
    case AUTO: return "Auto";
    case HEAT: return "Heat";
    case COOL: return "Cool";
    case FAN_ONLY: return "Fan_only";
    case AUX_HEAT: return "Aux Heat";
    default:   return "Error";
  }
}

HVAC_MODE strToHvacMode(char *mode)
{
  int n;
  for (n=OFF; n != ERROR; n++)
  {
    if (strcmp(mode, hvacModeToString((HVAC_MODE)n)) == 0)
      break;
  }
  return (HVAC_MODE)n;
}

// Pass in selected HVAC mode from list of available modes
// based on HVAC configuration. Then map this value to the
// list of all HVAC modes.
HVAC_MODE convertSelectedHvacMode()
{
  char selMode[12];

  // Check to see if the selected HVAC mode is now disabled (after config menu)
  if ((OperatingParameters.hvacSetMode == AUTO) && !OperatingParameters.hvacCoolEnable)
    updateHvacMode(OFF);
  if ((OperatingParameters.hvacSetMode == FAN_ONLY) && !OperatingParameters.hvacFanEnable)
    updateHvacMode(OFF);
  if ((OperatingParameters.hvacSetMode == COOL) && !OperatingParameters.hvacCoolEnable)
    updateHvacMode(OFF);
  if ((OperatingParameters.hvacSetMode == AUX_HEAT) && !OperatingParameters.hvac2StageHeatEnable)
    updateHvacMode(OFF);

  // Retrieve currently selected HVAC mode
  strncpy (selMode, hvacModeToString(OperatingParameters.hvacSetMode), sizeof(selMode));

  char tempModes[48] = {0};
  memcpy (tempModes, thermostatModes, sizeof(thermostatModes));

  char *pch = strtok(tempModes, "\n");
  int idx = 0;
  while (pch)
  {
    if (strcmp(pch, selMode) == 0)
      break;
    pch = strtok(NULL, "\n");
    idx++;
  }

  return (HVAC_MODE)idx;
}

void setHvacModesDropdown()
{
  // Build up HVAC mode dropdown from enum list
  char tempModes[48] = {0};
  for (int n=OFF; n != ERROR; n++)
  {
    if ((n == AUTO) && !OperatingParameters.hvacCoolEnable)
      continue;
    if ((n == FAN_ONLY) && !OperatingParameters.hvacFanEnable)
      continue;
    if ((n == COOL) && !OperatingParameters.hvacCoolEnable)
      continue;
    if ((n == AUX_HEAT) && !OperatingParameters.hvac2StageHeatEnable)
      continue;
    if (n != OFF)
      strcat (tempModes, "\n");
    strcat (tempModes, hvacModeToString((HVAC_MODE)(n)));
  }
  strncpy (thermostatModes, tempModes, sizeof(thermostatModes));
  lv_dropdown_set_options(ui_ModeDropdown, thermostatModes);
}

void tftCalibrateTouch()
{
  /*Set the touchscreen calibration data,
    the actual data for your display can be acquired using
    the Generic -> Touch_calibrate example from the TFT_eSPI library*/
//  uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
/*
For 3.2" TouchScreen:
Cal data:
0 : 3778
1 : 359
2 : 3786
3 : 3803
4 : 266
5 : 347
6 : 258
7 : 3769

For 2.8" TouchScreen:
Cal data:
0 : 3839
1 : 336
2 : 3819
3 : 3549
4 : 893
5 : 390
6 : 718
7 : 3387

*/
  // Clear screen & display instructions
  tft.fillScreen(TFT_NAVY);
  tft.setCursor(50, 100);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.print("TOUCH EACH CORNER WITH STYLUS");
  // Run the calibration routine. Data points returned in calData.
  tft.calibrateTouch(calData, TFT_BLACK, TFT_WHITE);
  // Use calData to set up touch dimensions
  tft.setTouchCalibrate(calData);
  // Dump data to debug logger
  printf ("Touch Screen calibration data:\n");
  for (int n=0; n < 8; n++) printf("%d : %d\n", n, calData[n]);
}

void tftInit()
{
  lv_init();

  tft.begin();

  memcpy (calData, calData_3_2, sizeof(calData));
  tft.setTouchCalibrate(calData);

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touch_read;
  lv_indev_drv_register(&indev_drv);

  ui_init();

  setHvacModesDropdown();

  printf("Current temp set to %.1f°\n", OperatingParameters.tempSet);

  lv_arc_set_value(ui_TempArc, OperatingParameters.tempSet*10);
  lv_label_set_text_fmt(ui_SetTemp, "%d°", (int)OperatingParameters.tempSet);
  if (OperatingParameters.tempUnits == 'C')
  {
    lv_label_set_text_fmt(ui_SetTempFrac, "%d", (int)getRoundedFrac(OperatingParameters.tempSet));
  }

  tftWakeDisplay(true);
  tftUpdateDisplay();
}

void tftPump(void * parameter)
{
  for(;;) // infinite loop
  {
    lv_timer_handler();

    tftUpdateDisplay();

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

    // Do we need to update the display and remove the "Scanning..." message?
    if ((ui_WifiStatusLabel_timestamp > 0) && (millis() - ui_WifiStatusLabel_timestamp > UI_TEXT_DELAY))
    {
      lv_label_set_text(ui_WifiStatusLabel, "");
      ui_WifiStatusLabel_timestamp = 0;
    }

  // Was the motion detection triggered?
  if (tftMotionTrigger)
  {
    tftMotionTrigger = false;
    //
    // This only matters if the touchTimer is already disabled (so the display is sleeping)
    // and the current screen is the cefault (main screen).
    //
    if (/*(lastMotionDetected == 0) &&*/ (!tftTouchTimerEnabled) && isCurrentScreenMain())
    {
      printf("Motion wake triggered\n");
      lastMotionDetected = millis();
      OperatingParameters.motionDetected = true;
      tftWakeDisplayMotion();
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
      printf ("Motion detection timeout\n");
      OperatingParameters.motionDetected = false;
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
