#include "thermostat.hpp"
#include "../ui.h"

static void change_mode_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        lv_obj_t * obj = lv_event_get_current_target_obj(e);
        lv_obj_t * parent = lv_obj_get_parent(obj);
        updateHvacMode((HVAC_MODE)(long)lv_event_get_user_data(e));
        select_object(parent, obj);
        setHVACMode();
    }
}

void createModeBox(lv_obj_t * scr, lv_obj_t *icon, const char *label, int x, int y, long mode)
{
    lv_obj_t * box = lv_button_create(scr);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(box, 44, 54);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_style_bg_opa(box, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_border_color(box, lv_color_white(), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_shadow_width(box, 0, 0);
    lv_obj_add_event_cb(box, change_mode_event_cb, LV_EVENT_ALL, (void *)mode);

    lv_obj_set_parent(icon, box);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, -2);

    lv_obj_t * l_label = lv_label_create(box);
    //lv_obj_set_style_text_align(l_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l_label, label);
    lv_obj_set_style_text_font(l_label, &lv_font_montserrat_12, 0);
    lv_obj_align(l_label, LV_ALIGN_BOTTOM_MID, 0, 6);
    lv_obj_set_style_text_color(l_label, lv_color_white(), 0);

    if (OperatingParameters.hvacSetMode == mode) {
        select_object(scr, box);
    }
}

void fan_set_value(lv_obj_t *fan, int value)
{
  if (value > 0 && OperatingParameters.hvacSetMode == OFF) {
    updateHvacMode(FAN_ONLY);
  } else if (value == 0 && OperatingParameters.hvacSetMode == FAN_ONLY) {
    updateHvacMode(OFF);
  }
}

void fan_cb(lv_event_t *e)
{
  lv_obj_t *but = lv_event_get_current_target_obj(e);
  long value = (long)lv_event_get_user_data(e);
  lv_obj_t *spin = lv_obj_get_child(lv_obj_get_parent(but), 0);
  spinbox_set_value(spin, value);
  //fan_set_value(lv_obj_get_parent(but), value);
}
#include <stdio.h>
void fan_pct_cb(lv_event_t *e)
{
  lv_obj_t *spin = lv_event_get_current_target_obj(e);
  long value = (long)lv_obj_get_user_data(spin);
  fan_set_value(lv_obj_get_parent(spin), value);
}

lv_obj_t * uiHVACSettings()
{
  lv_obj_t * scr = get_page();
  lv_obj_t *img;

  lv_obj_set_style_bg_color(scr,lv_color_black(),LV_PART_MAIN);
  lv_obj_set_style_text_color(scr, lv_color_white(), 0);

  create_menu_icon(scr, HOME_SYMBOL, PAGE_PRESENT);

  lv_obj_t * mode_box = create_invisible_box(scr, 240, 114);
  lv_obj_set_pos(mode_box, 0, 50);

  img = create_hvac_icon(scr, HEAT_SYMBOL, 0xff0000);
  createModeBox(mode_box, img, "Heat", 22, 0, HEAT);

  img = create_hvac_icon(scr, COOL_SYMBOL, 0x6666e6);
  createModeBox(mode_box, img, "Cool", 22 + 1 * (44 + 32), 0, COOL);

  img = lv_image_create(scr);
  lv_image_set_src(img, &heat_cool);
  createModeBox(mode_box, img, "Auto", 22+2*(44 + 32), 0, AUTO);

  img = create_hvac_icon(scr, AUXHEAT_SYMBOL, 0xff0000);
  createModeBox(mode_box, img, "Aux", 22, 60, AUX_HEAT);

  img = create_hvac_icon(scr, DRY_SYMBOL, 0x444488);
  createModeBox(mode_box, img, "Dry", 22+1*(44 + 32), 60, DRY);

  img = create_hvac_icon(scr, FAN_OFF_SYMBOL, 0x888888);
  createModeBox(mode_box, img, "Off", 22+2*(44 + 32), 60, OFF);

  lv_obj_t * l_fan = lv_label_create(scr);
  lv_obj_set_size(l_fan, 240-32, 110);
  lv_obj_set_style_text_align(l_fan, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l_fan, LV_ALIGN_TOP_MID, 0, 180);
  lv_label_set_text(l_fan, "Fan");

  int fan_value = OperatingParameters.hvacSetMode == FAN_ONLY ? 100 : 0;
  lv_obj_t * b_fan_pct = spinbox(l_fan, 240-32-84, 40, 42, 20, fan_value, 0, 100, 5);
  lv_obj_add_event(b_fan_pct, fan_pct_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_t * b_fan_off = label_button(l_fan, 40, 40, 0, 20, "Off", LV_ALIGN_TOP_LEFT);
  lv_obj_add_event(b_fan_off, fan_cb, LV_EVENT_CLICKED, (void *)0);
  lv_obj_t * b_fan_hold = label_button(l_fan, 40, 40, 0, 20, "Hold", LV_ALIGN_TOP_RIGHT);
  lv_obj_add_event(b_fan_hold, fan_cb, LV_EVENT_CLICKED, (void *)100);
  return scr;
}
