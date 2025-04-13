#include "ui.h"


void my_timer(lv_timer_t * timer)
{
    lv_scr_load_anim(scr_MainPagePresent, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, 0);
}

extern "C" void thermostat()
{
  uiMainPagePresent();
  uiHVACSettings();
  uiHoldSettings();
  uiMainPageAbsent();
  uiMenu();
  lv_scr_load(scr_MainPageAbsent);
  lv_timer_t * timer = lv_timer_create(my_timer, 1000, NULL);
  lv_timer_set_repeat_count(timer, 1);
}
