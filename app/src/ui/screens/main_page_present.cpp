// Main Screen when user presence detected
#include "thermostat.hpp"
#include "../ui.h"

int fan_mode = 1;
int hold = 1;

lv_obj_t * l_cur_temp;
lv_obj_t * l_cur_temp_dec;
lv_obj_t * l_set_temp;
lv_obj_t * l_hvac;
lv_obj_t * l_hvac_auto;
lv_obj_t * l_fan;
lv_obj_t * l_hold;

static lv_obj_t * transparent_overlay;

static lv_obj_t * scr_MainPagePresent = NULL;

static void update_temp_display()
{
  if (TEMP_UNITS_C) {
    lv_label_set_text_fmt(l_set_temp, "%.1f", OperatingParameters.tempSet);
  } else {
    lv_label_set_text_fmt(l_set_temp, "%d", int(OperatingParameters.tempSet));
  }
  lv_label_set_text_fmt(l_cur_temp, "%d", int(OperatingParameters.tempCurrent));
  if (TEMP_UNITS_C) {
    int cur_temp = int(OperatingParameters.tempCurrent);
    int cur_temp_dec = int((OperatingParameters.tempCurrent - cur_temp) * 10 + 0.5);
    lv_label_set_text_fmt(l_cur_temp_dec, ".%d", cur_temp_dec);
    lv_obj_align_to(l_cur_temp_dec, l_cur_temp, LV_ALIGN_BOTTOM_RIGHT, 45, -10); // -10, -60);
  }
}
void uiUpdateTemperatureUnits()
{
  if (TEMP_UNITS_C) {
    lv_obj_align(l_cur_temp, LV_ALIGN_TOP_MID, -25, 78);
    lv_obj_clear_flag(l_cur_temp_dec, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_align(l_cur_temp, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_add_flag(l_cur_temp_dec, LV_OBJ_FLAG_HIDDEN);
  }
}

static void set_temp_event_cb(lv_event_t * e)
{
    int dir = (long)lv_event_get_user_data(e);
    updateHvacSetTemp(OperatingParameters.tempSet += dir / (TEMP_UNITS_C ? 2.0 : 1));
    update_temp_display();
}

void setHVACMode() {
    // hvac mode
    if (OperatingParameters.hvacSetMode == AUTO) {
        lv_obj_remove_flag(l_hvac_auto, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(l_hvac, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(l_hvac_auto, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(l_hvac, LV_OBJ_FLAG_HIDDEN);
        if (OperatingParameters.hvacSetMode == COOL) {
            lv_label_set_text(l_hvac, COOL_SYMBOL);
            lv_obj_set_style_text_color(l_hvac, lv_color_hex(0x6666e6), 0);
        } else if (OperatingParameters.hvacSetMode == HEAT) {
            lv_label_set_text(l_hvac, HEAT_SYMBOL);
            lv_obj_set_style_text_color(l_hvac, lv_color_hex(0xff0000), 0);
        } else if (OperatingParameters.hvacSetMode == AUX_HEAT) {
            lv_label_set_text(l_hvac, AUXHEAT_SYMBOL);
            lv_obj_set_style_text_color(l_hvac, lv_color_hex(0xff0000), 0);
        } else if (OperatingParameters.hvacSetMode == DRY) {
            lv_label_set_text(l_hvac, DRY_SYMBOL);
            lv_obj_set_style_text_color(l_hvac, lv_color_hex(0x444488), 0);
        } else {
            lv_label_set_text(l_hvac, FAN_OFF_SYMBOL);
            lv_obj_set_style_text_color(l_hvac, lv_color_hex(0x888888), 0);
        }
    }
    // Fan
    // FIXME
    fan_mode = (OperatingParameters.hvacSetMode == FAN_ONLY);
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
      PAGE page = LV_USERDATA_TO_PAGE(lv_event_get_user_data(e));
      if (page == PAGE_QUICK_HOLD && ! hold) {
            // special case to not show hold menu if not in hold mode
            return;
      }
      page_event_cb(e);
    }
}

static void buildMainPagePresent()
{
    scr_MainPagePresent = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(scr_MainPagePresent,lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_text_color(scr_MainPagePresent, lv_color_white(), 0);

    // Current temperature
    l_cur_temp = lv_label_create(scr_MainPagePresent);
    lv_obj_set_style_text_align(l_cur_temp, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_cur_temp, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_style_text_font(l_cur_temp, &roboto_semicond_medium_num_148, 0);

    // Current temperature decimal
    l_cur_temp_dec = lv_label_create(scr_MainPagePresent);
    lv_obj_set_style_text_align(l_cur_temp_dec, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(l_cur_temp_dec, &roboto_semicond_medium_num_72, 0);


    lv_obj_t * scr = create_invisible_box(scr_MainPagePresent, 240, 320);
    transparent_overlay = scr;

    // Menu
    create_menu_icon(scr, MENU_SYMBOL, PAGE_MENU);

    // Target temperature
    l_set_temp = lv_label_create(scr);
    lv_obj_set_style_text_align(l_set_temp, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_set_temp, LV_ALIGN_TOP_MID, 0, 220);
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


    // Hit Boxes
    lv_obj_t * inc_box = create_invisible_box(scr, 120, 180);
    lv_obj_set_pos(inc_box, 120, 75);
    lv_obj_add_flag(inc_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(inc_box, set_temp_event_cb, LV_EVENT_CLICKED, (void *)1);

    lv_obj_t * dec_box = create_invisible_box(scr, 120, 180);
    lv_obj_set_pos(dec_box, 0, 75);
    lv_obj_add_flag(dec_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dec_box, set_temp_event_cb, LV_EVENT_CLICKED, (void *)-1);

    lv_obj_t * hold_box = create_invisible_box(scr, 80, 50);
    lv_obj_set_pos(hold_box, 0, 270);
    lv_obj_add_flag(hold_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hold_box, hold_page_event_cb, LV_EVENT_ALL, (void *)PAGE_QUICK_HOLD);

    lv_obj_t * hvac_box = create_invisible_box(scr, 160, 50);
    lv_obj_set_pos(hvac_box, 80, 270);
    lv_obj_add_flag(hvac_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hvac_box, page_event_cb, LV_EVENT_ALL, (void *)PAGE_QUICK_HVAC);

}

void main_page_fade_out(int speed)
{
    lv_obj_fade_out(transparent_overlay, speed, 0);
}
void main_page_fade_in(int speed)
{
    lv_obj_fade_in(transparent_overlay, speed, 0);
}

lv_obj_t * uiMainPagePresent()
{
    if (! scr_MainPagePresent)
       buildMainPagePresent();
    setHVACMode();
    uiUpdateTemperatureUnits();
    update_temp_display();
    return scr_MainPagePresent;
}

void uiUpdateMainPage()
{
  setHVACMode();
  update_temp_display();
}