#include "thermostat.hpp"
#include "tft.hpp"

#define START_BRIGHTNESS 255
#define END_BRIGHTNESS 0

//static LGFX lcd;
int32_t lastTouchDetected = 0;
TaskHandle_t xTouchHandle;
bool tftTouchTimerEnabled = true;

// RTC_DATA_ATTR int bootCount = 0;  // Retains the number of starts (the value does not disappear even if deepsleep is performed)
int bootCount = 0;  // Retains the number of starts (the value does not disappear even if deepsleep is performed)

void tftTouchScreenDriver(void * parameter)
{
  int32_t x, y;
  for(;;) // infinite loop
  {
    vTaskSuspend( NULL );

    if (millis() - lastTouchDetected > 300)  // debounce
    {
      if (tft.getTouchRaw(&x, &y))
      {
        audioBeep();
        Serial.print(millis() - lastTouchDetected);
        Serial.print(" - Touch - X: "); Serial.print(x); Serial.print(" Y: "); Serial.println(y);
        lastTouchDetected = millis();

//        displayDimDemo(0, true);

      }
    }
  }
}

void IRAM_ATTR TouchDetect_ISR()
{
  BaseType_t xYieldRequired;
  xYieldRequired = xTaskResumeFromISR( xTouchHandle );
  portYIELD_FROM_ISR( xYieldRequired );
}

void tftCreateTask()
{
    // attachInterrupt(TOUCH_IRQ_PIN, TouchDetect_ISR, FALLING);

    // xTaskCreate (
    //     tftTouchScreenDriver,
    //     "Touch Screen",
    //     4096,
    //     NULL,
    //     tskIDLE_PRIORITY+1,
    //     &xTouchHandle
    // );
}

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
void tftWakeDisplay()
{
  lv_obj_clear_flag(ui_ModeDropdown, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_Panel1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_Panel2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_SetupBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_Arc2, LV_OBJ_FLAG_HIDDEN);
  tft.setBrightness(START_BRIGHTNESS);
  tftEnableTouchTimer();
  tftUpdateTouchTimestamp();
  audioBeep();
}
void tftDimDisplay()
{
  if (tftTouchTimerEnabled)
    tft.setBrightness(END_BRIGHTNESS);

  lv_obj_add_flag(ui_ModeDropdown, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Panel1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Panel2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SetupBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Arc2, LV_OBJ_FLAG_HIDDEN);
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
  lv_label_set_text(ui_Time, buffer);

  lv_label_set_text_fmt(ui_Temp, "%d°", getTemp());
  lv_label_set_text_fmt(ui_Humidity, "%d%%", getHumidity());
  lv_label_set_text_fmt(ui_SetTemp, "%d°", (int)(OperatingParameters.tempSet + 0.5));

  lv_dropdown_set_selected(ui_ModeDropdown, OperatingParameters.hvacSetMode);
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

  // Build up HVAC mode dropdown from enum list
//  for (int n=0; n != ERROR; n++)
//    lv_dropdown_set_options(ui_ModeDropdown, hvacModeToString((HVAC_MODE)(n)));

  lv_arc_set_value(ui_Arc2, (int)(OperatingParameters.tempSet + 0.5));

  tftWakeDisplay();
  tftUpdateDisplay();
}

void displaySplash()
{
  // lcd.clear(TFT_BLACK);

  // lcd.startWrite();      // draw the background
  // lcd.setColorDepth(24);
  // {
  //   LGFX_Sprite sp(&lcd);
  //   sp.createSprite(128, 128);
  //   sp.createPalette();
  //   for (int y = 0; y < 128; y++)
  //     for (int x = 0; x < 128; x++)
  //       sp.writePixel(x, y, sp.color888(x << 1, x + y, y << 1));
  //   for (int y = 0; y < lcd.height(); y += 128)
  //     for (int x = 0; x < lcd.width(); x += 128)
  //       sp.pushSprite(x, y);
  // }
  // lcd.endWrite();
}

void displayStartDemo()
{
//   displaySplash();

//   lcd.setCursor(bootCount*6, bootCount*8);
//   lcd.setBrightness(START_BRIGHTNESS);
//   lcd.setTextColor(TFT_WHITE, TFT_BLACK);  // Draw the black and white reversed state once
//   lcd.print("Display test : " + String(bootCount));

//   ++bootCount;
//   if ((bootCount == 0) || (bootCount > 37)) bootCount = 1; // bootCount wrapped?

//   lcd.setCursor(bootCount*6, bootCount*8);
//   lcd.setTextColor(TFT_BLACK, TFT_WHITE);
//   lcd.print("Display test : " + String(bootCount));
// //  lcd.powerSaveOn(); // Power saving specification M5Stack CoreInk prevents colors from fading when power is turned off
//   lcd.waitDisplay(); // stand-by
}

void displayDimDemo(int32_t timeDelta, bool abort)
{
  static int brightness;
  static bool dimming = false;
  static bool demo_started = false;

  if (abort)
  {
    tft.setBrightness(START_BRIGHTNESS);
    dimming = false;
    brightness = END_BRIGHTNESS;
    demo_started = false;
    return;
  }

  if ((demo_started == false) && (timeDelta < 2000))
  {
    brightness = START_BRIGHTNESS;
    demo_started = true;
    dimming = false;
  }

  if ((timeDelta > 2000) && (!dimming))
  {
    Serial.println("Dimming started...");
    dimming = true;
  }

  if ((dimming) && (brightness > END_BRIGHTNESS))
  {
    --brightness;
    tft.setBrightness(brightness);
    //delay(40);
  }

  if (brightness == END_BRIGHTNESS)
    demo_started = false;

}


void tftPump()
{
  lv_timer_handler();

  tftUpdateDisplay();

  if (millis() - lastTouchDetected > 10000)
    tftDimDisplay();

  delay(5);
}