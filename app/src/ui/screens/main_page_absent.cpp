#include "../ui.h"

lv_obj_t * scr_MainPageAbsent;

void uiMainPageAbsent()
{
    scr_MainPageAbsent = lv_obj_create(NULL);
    lv_obj_t * scr = scr_MainPageAbsent;

	lv_obj_set_style_bg_color(scr,lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);

    // Current temperature
    lv_obj_t * l_cur_temp_absent = lv_label_create(scr);
    lv_obj_set_style_text_align(l_cur_temp_absent, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_cur_temp_absent, LV_ALIGN_TOP_MID, 0, 78);
    lv_label_set_text(l_cur_temp_absent, "55");
    lv_obj_set_style_text_font(l_cur_temp_absent, &roboto_semicond_medium_num_148, 0);
}
