#include "ui.h"

// This page is used for all screens except for the 'MainPagePresent' screen, since all transitions
// are between MainPagePresent and some other screen
static lv_obj_t * scr_Page = NULL;
static PAGE cur_page = PAGE_NONE;

lv_obj_t * get_page()
{
  if (! scr_Page) {
    scr_Page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_Page,lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_text_color(scr_Page, lv_color_white(), 0);
  }
  lv_obj_clean(scr_Page);
  return scr_Page;
}

void page_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
      PAGE page = LV_USERDATA_TO_PAGE(lv_event_get_user_data(e));
      load_page(page, TRANSITION_SLIDE);
    }
}

void load_page(PAGE page, TRANSITION transition)
{
  lv_obj_t *scr = NULL;
  switch(page) {
    case PAGE_ABSENT:       scr = uiMainPageAbsent();  break;
    case PAGE_PRESENT:      scr = uiMainPagePresent(); break;
    case PAGE_QUICK_HVAC:   scr = uiHVACSettings();    break;
    case PAGE_QUICK_HOLD:   scr = uiHoldSettings();    break;
    case PAGE_MENU:         scr = uiMenu();            break;
    case PAGE_NONE:         return;
  }
  if (! scr)
      return;
  if (page == PAGE_PRESENT && cur_page == PAGE_ABSENT) {
    main_page_fade_in(200);
  } else if (page == PAGE_ABSENT && cur_page == PAGE_PRESENT) {
    main_page_fade_out(200);
  } else {
    switch (transition) {
      case TRANSITION_NONE:
        lv_screen_load(scr);
        break;
      case TRANSITION_FADE:
        lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, 0);
        break;
      case TRANSITION_SLIDE:
        lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, 0);
        break;
    }
  }
  cur_page = page;
}
