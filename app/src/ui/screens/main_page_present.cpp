// Main Screen when user presence detected
#include "../ui.h"

int target_temp = 77;
int hvac_mode = 1;
int fan_mode = 1;
int hold = 1;

lv_obj_t * l_set_temp;
lv_obj_t * l_hvac;
lv_obj_t * l_hvac_auto;
lv_obj_t * l_fan;
lv_obj_t * l_hold;
lv_obj_t * scr_MainPagePresent;

static void set_temp_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        int dir = (long)lv_event_get_user_data(e);
        target_temp += dir;
        lv_label_set_text_fmt(l_set_temp, "%d", target_temp);
    }
}

void setHVACMode() {
    // hvac mode
    if (hvac_mode == 0) {
        lv_obj_add_flag(l_hvac_auto, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(l_hvac, LV_OBJ_FLAG_HIDDEN);

    }
    else if (hvac_mode < 4) {
        lv_obj_add_flag(l_hvac_auto, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(l_hvac, LV_OBJ_FLAG_HIDDEN);
        if (hvac_mode == 1) {
            lv_label_set_text(l_hvac, COOL_SYMBOL);
            lv_obj_set_style_text_color(l_hvac, lv_color_hex(0x6666e6), 0);
        } else if (hvac_mode == 2) {
            lv_label_set_text(l_hvac, HEAT_SYMBOL);
            lv_obj_set_style_text_color(l_hvac, lv_color_hex(0xff0000), 0);
        } else {
            lv_label_set_text(l_hvac, AUXHEAT_SYMBOL);
            lv_obj_set_style_text_color(l_hvac, lv_color_hex(0xff0000), 0);
        }
    } else {
        lv_obj_clear_flag(l_hvac_auto, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(l_hvac, LV_OBJ_FLAG_HIDDEN);
    }
    // Fan
    if (fan_mode == 0) {
        lv_obj_add_flag(l_fan, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(l_fan, LV_OBJ_FLAG_HIDDEN);
        if (fan_mode == 1) {
            lv_label_set_text(l_fan, FAN_SYMBOL);
        } else {
            lv_label_set_text(l_fan, FAN_AUTO_SYMBOL);
        }
    }
    // Hold
    if (hold) {
        lv_obj_clear_flag(l_hold, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(l_hold, LV_OBJ_FLAG_HIDDEN);
    }
}

static void hold_page_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
      lv_obj_t ** page = (lv_obj_t **)lv_event_get_user_data(e);
      if (page == &scr_HoldSettings && ! hold) {
            // special case to not show hold menu if not in hold mode
            return;
      }
      page_event_cb(e);
    }
}

void uiMainPagePresent()
{
    scr_MainPagePresent = lv_obj_create(NULL);
    lv_obj_t * scr = scr_MainPagePresent;

	lv_obj_set_style_bg_color(scr,lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);

    // Menu
    create_menu_icon(scr, MENU_SYMBOL, &scr_Menu);

    // Current temperature
    lv_obj_t * l_cur_temp = lv_label_create(scr);
    lv_obj_set_style_text_align(l_cur_temp, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_cur_temp, LV_ALIGN_TOP_MID, 0, 78);
    lv_label_set_text(l_cur_temp, "55");
    lv_obj_set_style_text_font(l_cur_temp, &roboto_semicond_medium_num_148, 0);

    // Target temperature
    l_set_temp = lv_label_create(scr);
    lv_obj_set_style_text_align(l_set_temp, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_set_temp, LV_ALIGN_TOP_MID, 0, 220);
    lv_label_set_text_fmt(l_set_temp, "%d", target_temp);
    lv_obj_set_style_text_font(l_set_temp, &FONT_28, 0);

    // Increase Temperature
    lv_obj_t * l_temp_inc = lv_label_create(scr);
    lv_obj_set_style_text_align(l_temp_inc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_temp_inc, LV_ALIGN_TOP_MID, 50, 220);
    lv_label_set_text(l_temp_inc, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(l_temp_inc, &FONT_28, 0);

    // Decrease Temperature
    lv_obj_t * l_temp_dec = lv_label_create(scr);
    lv_obj_set_style_text_align(l_temp_dec, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_temp_dec, LV_ALIGN_TOP_MID, -50, 220);
    lv_label_set_text(l_temp_dec, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(l_temp_dec, &FONT_28, 0);

    // Outside Weather
    lv_obj_t * l_weather = lv_label_create(scr);
    lv_obj_set_style_text_align(l_weather, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_weather, LV_ALIGN_TOP_MID, -25, 0);
    lv_label_set_text(l_weather, WEATHER_SUNNY);
    lv_obj_set_style_text_font(l_weather, &FONT_22, 0);
    lv_obj_set_style_text_color(l_weather, lv_color_hex(0xff0000), 0);
    lv_obj_update_layout(l_weather);

    // Outside temperature
    lv_obj_t * l_outside_temp = lv_label_create(scr);
    lv_obj_set_style_text_align(l_outside_temp, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_outside_temp, LV_ALIGN_TOP_MID, 0, 0);
    lv_label_set_text(l_outside_temp, "66");
    lv_obj_set_style_text_font(l_outside_temp, &FONT_22, 0);

    // Time
    lv_obj_t * l_time = lv_label_create(scr);
    lv_obj_set_style_text_align(l_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_time, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(l_time, "23:59");
    lv_obj_set_style_text_font(l_time, &FONT_22, 0);

    // Humidity
    lv_obj_t * l_humid = lv_label_create(scr);
    lv_obj_set_style_text_align(l_humid, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_humid, LV_ALIGN_TOP_MID, 0, 45);
    lv_label_set_text(l_humid, WATER_SYMBOL " 60%");
    lv_obj_set_style_text_font(l_humid, &FONT_22, 0);

    // Status

    l_hvac = create_hvac_icon(scr, HEAT_SYMBOL, 0xff0000);
    lv_obj_align(l_hvac, LV_ALIGN_BOTTOM_MID, 0, -10);
    l_hvac_auto = lv_image_create(scr);
    lv_image_set_src(l_hvac_auto, &heat_cool);
    lv_obj_align(l_hvac_auto, LV_ALIGN_BOTTOM_MID, 0, -10);

    l_fan = create_hvac_icon(scr, FAN_SYMBOL, 0xffffff);
    lv_obj_align(l_fan, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    l_hold = create_hvac_icon(scr, HAND_SYMBOL, 0xff0000);
    lv_obj_align(l_hold, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    setHVACMode();

    // Hit Boxes
    lv_obj_t * inc_box = create_invisible_box(scr, 120, 180);
    lv_obj_set_pos(inc_box, 120, 75);
    lv_obj_add_flag(inc_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(inc_box, set_temp_event_cb, LV_EVENT_ALL, (void *)1);

    lv_obj_t * dec_box = create_invisible_box(scr, 120, 180);
    lv_obj_set_pos(dec_box, 0, 75);
    lv_obj_add_flag(dec_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dec_box, set_temp_event_cb, LV_EVENT_ALL, (void *)-1);

    lv_obj_t * hold_box = create_invisible_box(scr, 80, 50);
    lv_obj_set_pos(hold_box, 0, 270);
    lv_obj_add_flag(hold_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hold_box, hold_page_event_cb, LV_EVENT_ALL, (void *)&scr_HoldSettings);

    lv_obj_t * hvac_box = create_invisible_box(scr, 160, 50);
    lv_obj_set_pos(hvac_box, 80, 270);
    lv_obj_add_flag(hvac_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hvac_box, page_event_cb, LV_EVENT_ALL, (void *)&scr_HVACSettings);
}
