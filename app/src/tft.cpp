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
 *  04-Dec-2023: Michael Burke (michaelburke2000@gmail.com) - Add screen brightness auto-adjustment
 */

#include "thermostat.hpp"
#include "tft.hpp"

#define OFF_BRIGHTNESS 0
#define MIN_BRIGHTNESS 5
#define FULL_BRIGHTNESS 255

static char thermostatModes[48] = {0};
TaskHandle_t xTouchUIHandle;
int64_t lastTouchDetected = 0;
bool tftTouchTimerEnabled = true;
int64_t ui_WifiStatusLabel_timestamp = 0;
uint16_t calData_2_8[8] = { 3839, 336, 3819, 3549, 893, 390, 718, 3387 };
uint16_t calData_3_2[8] = { 3778, 359, 3786, 3803, 266, 347, 258, 3769 };
uint16_t calData[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int64_t lastMotionDetected = 0;
bool tftAwake = true;

volatile bool tftMotionTrigger = false;

#define TAG "TFT"

#ifdef __cplusplus
extern "C" {
#endif

void tftDisableTouchTimer()
{
  ESP_LOGI(TAG, "Disabling touch/motion timer");
  tftTouchTimerEnabled = false;
#ifdef MQTT_ENABLED
  // Stay in synch with display
  if (OperatingParameters.MqttConnected)
    MqttMotionUpdate(OperatingParameters.motionDetected);
#endif
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


int sensorToScreenBrightness(int sensor)
{
  // THIS 4095 IS THE MAX BRIGHTNESS READING OF THE LIGHT SENSOR.
  int conversionFactor =  4095 / FULL_BRIGHTNESS;
  return sensor / conversionFactor;
}

#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : x))
#define BRIGHTNESS_STEP_SIZE 2
void tftAutoBrightness()
{
  int curBrightness = tft.getBrightness();
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
  tft.setBrightness(CLAMP(curBrightness + adjustment, MIN_BRIGHTNESS, FULL_BRIGHTNESS));
}
#undef CLAMP

void tftWakeDisplay(bool beep)
{
  if (beep)
    audioBeep();
  tftShowDisplayItems();
  tftEnableTouchTimer();
  tftUpdateTouchTimestamp();
  tftAwake = true;
}

void tftWakeDisplayMotion()
{
  if (!tftTouchTimerEnabled)
  {
    tftShowDisplayItems();
    tftEnableTouchTimer();
    tftUpdateTouchTimestamp();
    tftAwake = true;
  }
}

void tftDimDisplay()
{
  if (tftTouchTimerEnabled)
  {
    ESP_LOGD(TAG, "Dimming display");
    tft.setBrightness(OFF_BRIGHTNESS);
    tftHideDisplayItems();
    tftDisableTouchTimer();
    tftAwake = false;
  }
}

#ifdef __cplusplus
} /*extern "C"*/
#endif

void tftUpdateDisplay()
{
  static char buffer[16];
  static struct tm local_time;

  if (tftAwake)
    tftAutoBrightness();

  if (getLocalTime(&local_time, 80))
  {
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &local_time);
  }
  else
  {
    strcpy (buffer, "--:--:--");
  }
  lv_label_set_text(ui_TimeLabel, buffer);

  lv_label_set_text_fmt(ui_TempLabel, "%d°", getTemp());
  lv_label_set_text_fmt(ui_HumidityLabel, "%d%%", getHumidity());

  lv_arc_set_value(ui_TempArc, OperatingParameters.tempSet*10.0);
  lv_label_set_text_fmt(ui_SetTemp, "%d°", (int)OperatingParameters.tempSet);
  if (OperatingParameters.tempUnits == 'C')
  {
    lv_label_set_text_fmt(ui_SetTempFrac, "%d", (int)getRoundedFrac(OperatingParameters.tempSet));
  }

  lv_dropdown_set_selected(ui_ModeDropdown, convertSelectedHvacMode());

  if (WifiConnected())
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

static const char *__hvac_mode_to_str(HVAC_MODE mode, const char *mode_str[])
{
  switch (mode) {
  case OFF:
  case HEAT:
  case COOL:
  case DRY:
  case IDLE:
  case AUTO:
  case FAN_ONLY:
  case AUX_HEAT:
    return mode_str[mode];
  default:
    return mode_str[ERROR];
  }
}

#ifdef MQTT_ENABLED
const char *hvac_op_mode_str_mqtt[NR_HVAC_MODES] = {
  "off", "heat", "cool", "dry", "idle", "fan_only", "auto", "aux heat", "error"
};
const char *hvac_curr_mode_str_mqtt[NR_HVAC_MODES] = {
  "off", "heating", "cooling", "drying", "idle", "fan", "error", "error", "error"
};

const char *hvacModeToMqttCurrMode(HVAC_MODE mode)
{
  return __hvac_mode_to_str(mode, hvac_curr_mode_str_mqtt);
}

const char *hvacModeToMqttOpMode(HVAC_MODE mode)
{
  return __hvac_mode_to_str(mode, hvac_op_mode_str_mqtt);
}
#endif

const char *hvac_mode_str[NR_HVAC_MODES] = {
  "Off", "Heat", "Cool", "Dry", "Idle", "Fan Only", "Auto", "Aux Heat", "Error"
};

const char *hvacModeToString(HVAC_MODE mode)
{
  return __hvac_mode_to_str(mode, hvac_mode_str);
}

HVAC_MODE strToHvacMode(char *mode)
{
  for (int m = 0; m < NR_HVAC_MODES; m++) {
    if (strcmp(mode, hvacModeToString((HVAC_MODE)m)) == 0)
      return (HVAC_MODE)m;
  }

  return ERROR;
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
  for (int n = 0; n < NR_HVAC_MODES; n++) {
    if (n == ERROR)
      continue;
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
  ESP_LOGI (TAG, "Touch Screen calibration data:");
  for (int n=0; n < 8; n++) ESP_LOGI (TAG, "%d : %d", n, calData[n]);
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

  ESP_LOGI (TAG, "Current temp set to %.1f°", OperatingParameters.tempSet);

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
