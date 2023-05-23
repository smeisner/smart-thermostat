#include "thermostat.hpp"
#include "tft.hpp"

#define START_BRIGHTNESS 255
#define END_BRIGHTNESS 0

TaskHandle_t xTouchUIHandle;
int32_t lastTouchDetected = 0;
bool tftTouchTimerEnabled = true;
int32_t ui_WifiStatusLabel_timestamp = 0;

#ifdef __cplusplus
extern "C" {
#endif

void tftDisableTouchTimer()
{
  tftTouchTimerEnabled = false;
}
void tftEnableTouchTimer()
{
  tftTouchTimerEnabled = true;
}

void tftUpdateTouchTimestamp()
{
  lastTouchDetected = millis();
}

void tftWakeDisplay(bool beep)
{
  if (beep)
    audioBeep();
  lv_obj_clear_flag(ui_ModeDropdown, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_TempDecreaseBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_TempIncreaseBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_InfoBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_SetupBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_TempArc, LV_OBJ_FLAG_HIDDEN);
  tft.setBrightness(START_BRIGHTNESS);
  tftEnableTouchTimer();
  tftUpdateTouchTimestamp();
}

void tftDimDisplay()
{
  if (tftTouchTimerEnabled)
  {
    tft.setBrightness(END_BRIGHTNESS);

    lv_obj_add_flag(ui_ModeDropdown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_TempDecreaseBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_TempIncreaseBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_InfoBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SetupBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_TempArc, LV_OBJ_FLAG_HIDDEN);
  }
}

#ifdef __cplusplus
} /*extern "C"*/
#endif

void tftUpdateDisplay()
{
  static char buffer[16];
  static struct tm time;

  getLocalTime(&time);
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &time);
  lv_label_set_text(ui_TimeLabel, buffer);

  lv_label_set_text_fmt(ui_TempLabel, "%d°", tempOut(getTemp()));
  lv_label_set_text_fmt(ui_HumidityLabel, "%d%%", getHumidity());

  if (OperatingParameters.tempUnits == 'C')
  {

    lv_arc_set_value(ui_TempArc, tempOut(OperatingParameters.tempSet)*10);
    lv_label_set_text_fmt(ui_SetTemp, "%d°", tempOut(OperatingParameters.tempSet));
    lv_label_set_text_fmt(ui_SetTempFrac, "%d", degCfrac(OperatingParameters.tempSet));

  } else {

    int temp;
    temp = tempOut(OperatingParameters.tempSet);
    lv_arc_set_value(ui_TempArc, temp*10.0);
    lv_label_set_text_fmt(ui_SetTemp, "%d°", temp);

  }

  lv_dropdown_set_selected(ui_ModeDropdown, OperatingParameters.hvacSetMode);

  if (wifiConnected())
    lv_label_set_text(ui_WifiIndicatorLabel, "#0000A0 " LV_SYMBOL_WIFI);
  else
    lv_label_set_text(ui_WifiIndicatorLabel, "#4f4f4f " LV_SYMBOL_WIFI);
//    lv_label_set_text(ui_WifiIndicatorLabel, "#7d7d7d " LV_SYMBOL_WIFI);
//    lv_label_set_text(ui_WifiIndicatorLabel, "#808080 " LV_SYMBOL_WIFI);

  switch (OperatingParameters.hvacOpMode)
  {
    // Set color of inner circle representing the operating mode
    case HEAT: lv_obj_set_style_bg_color(ui_SetTempBg, lv_color_hex(0xa00b0b), LV_PART_MAIN); break;
    case COOL: lv_obj_set_style_bg_color(ui_SetTempBg, lv_color_hex(0x435deb), LV_PART_MAIN); break;
    case FAN:  lv_obj_set_style_bg_color(ui_SetTempBg, lv_color_hex(0x3c8945), LV_PART_MAIN); break;  //@@@
    default:   lv_obj_set_style_bg_color(ui_SetTempBg, lv_color_hex(0x7a92b2), LV_PART_MAIN); break;
  }

}

void setHvacModesDropdown()
{
  // Build up HVAC mode dropdown from enum list
  static char thermostatModes[48] = {0};
  char tempModes[48] = {0};
  for (int n=OFF; n != ERROR; n++)
  {
    if ((n == AUTO) && !OperatingParameters.hvacCoolEnable)
      continue;
    if ((n == FAN) && !OperatingParameters.hvacFanEnable)
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

void tftInit()
{
  lv_init();

  tft.begin();

  /*Set the touchscreen calibration data,
    the actual data for your display can be acquired using
    the Generic -> Touch_calibrate example from the TFT_eSPI library*/
//  uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
/*
Cal data:
0 : 3778
1 : 359
2 : 3786
3 : 3803
4 : 266
5 : 347
6 : 258
7 : 3769
*/
//  uint16_t calData[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  uint16_t calData[8] = { 3778, 359, 3786, 3803, 266, 347, 258, 3769 };
//  tft.calibrateTouch(calData, TFT_BLACK, TFT_WHITE);
  tft.setTouchCalibrate(calData);
//  Serial.printf ("Cal data:\n");
//  for (int n=0; n < 8; n++) Serial.printf("%d : %d\n", n, calData[n]);

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

  lv_arc_set_value(ui_TempArc, tempOut(OperatingParameters.tempSet)*10);
  lv_label_set_text_fmt(ui_SetTemp, "%d°", tempOut(OperatingParameters.tempSet));

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
      tftDimDisplay();

    if ((ui_WifiStatusLabel_timestamp > 0) && (millis() - ui_WifiStatusLabel_timestamp > UI_TEXT_DELAY))
    {
      lv_label_set_text(ui_WifiStatusLabel, "");
      ui_WifiStatusLabel_timestamp = 0;
    }

    delay(10);
  }
}

void tftCreateTask()
{
  xTaskCreate (
      tftPump,
      "Touch Screen UI",
      4096,
      NULL,
      tskIDLE_PRIORITY+1,
      &xTouchUIHandle
  );
}
