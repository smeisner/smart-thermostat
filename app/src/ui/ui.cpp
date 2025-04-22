#include "ui.h"
#include "display.hpp"


void my_timer(lv_timer_t * timer)
{
    load_page(PAGE_PRESENT, TRANSITION_FADE);
  }

// extern "C"
void uiInit()
{
  uiMainPagePresent();
  //uiHVACSettings();
  //uiHoldSettings();
  //uiMainPageAbsent();
  //uiMenu();
  load_page(PAGE_ABSENT, TRANSITION_NONE);
  lv_timer_t * timer = lv_timer_create(my_timer, 1000, NULL);
  lv_timer_set_repeat_count(timer, 1);
}

void uiUpdate()
{
  display_lock();
  uiUpdateMainPage();
  display_unlock();
}

void displayUpdate()
{
  display_lock();
  uiUpdate();
  display_unlock();
}
