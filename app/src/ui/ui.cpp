#include "thermostat.hpp"
#include "ui.hpp"

#include "lvgl.h"
#include "esp_lvgl_port.h"

static int64_t ui_WifiStatusLabel_timestamp = 0;
static lv_obj_t *ui_WifiStatusLabel;
static bool displayed = false;
extern const lv_font_t roboto_semicond_medium_148_num;

extern "C" void thermostat();

void uiWake(bool beep)
{
  if (beep)
    audioBeep();
}

void uiDim()
{

}
static void updateDisplay()
{
  lvgl_port_lock(0);
  if(! displayed) {
    thermostat();
    displayed = true;
    //lv_obj_t * label1 = lv_label_create(lv_screen_active());
    //lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_CENTER, 0);
    //lv_obj_align(label1, LV_ALIGN_CENTER, 0, 0);
    //lv_label_set_text(label1, "55");
    //lv_obj_set_style_text_font(label1, &roboto_semicond_medium_148_num, 0);
    
  }
  lvgl_port_unlock();
}

void uiPeriodic()
{
  //lv_timer_handler();

  updateDisplay();
#if 0
      // Do we need to update the display and remove the "Scanning..." message?
      if ((ui_WifiStatusLabel_timestamp > 0) && (millis() - ui_WifiStatusLabel_timestamp > UI_TEXT_DELAY))
      {
        lv_label_set_text(ui_WifiStatusLabel, "");
        ui_WifiStatusLabel_timestamp = 0;
      }
#endif
  
}

void uiUpdateTemperatureUnits()
{
  if (OperatingParameters.tempUnits == 'C')
  {
  }
}
