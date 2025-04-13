#include "ui.h"

void page_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
      lv_obj_t ** page = (lv_obj_t **)lv_event_get_user_data(e);
      lv_scr_load_anim(*page, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, 0);
    }
}
