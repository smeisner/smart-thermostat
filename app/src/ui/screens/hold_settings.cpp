#include "../ui.h"

lv_obj_t * scr_HoldSettings;

void uiHoldSettings()
{
  scr_HoldSettings = lv_obj_create(NULL);
  lv_obj_t * scr = scr_HoldSettings;

  lv_obj_set_style_bg_color(scr,lv_color_black(),LV_PART_MAIN);
  lv_obj_set_style_text_color(scr, lv_color_white(), 0);

  create_menu_icon(scr, HOME_SYMBOL, &scr_MainPagePresent);

  lv_obj_t * l_hold = lv_label_create(scr);
    lv_obj_set_size(l_hold, 240, 150);
    lv_obj_set_style_text_align(l_hold, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_hold, LV_ALIGN_TOP_MID, 0, 105);
    lv_label_set_text(l_hold, "Hold");

    lv_obj_t * b_nohold = label_button(l_hold, 240-32, 40, 0, 16, "No Hold");
    lv_obj_t * b_hold_next = label_button(l_hold, 240-32, 40, 0, 60, "Until Next Event");
    lv_obj_t * b_hold_time = label_button(l_hold, 240-32, 40, 0, 104, "30 minutes");
}
