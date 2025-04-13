#include "../ui.h"

lv_obj_t * scr_HVACSettings;

static void change_mode_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        lv_obj_t * obj = lv_event_get_current_target_obj(e);
        lv_obj_t * parent = lv_obj_get_parent(obj);
        hvac_mode = (long)lv_event_get_user_data(e);
        select_object(parent, obj);
        setHVACMode();
    }
}

void createModeBox(lv_obj_t * scr, lv_obj_t *icon, const char *label, int x, long mode)
{
    lv_obj_t * box = lv_button_create(scr);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(box, 44, 54);
    lv_obj_set_pos(box, x, 0);
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

    if (hvac_mode == mode) {
        select_object(scr, box);
    }
}

void uiHVACSettings()
{
  scr_HVACSettings = lv_obj_create(NULL);
  lv_obj_t * scr = scr_HVACSettings;

  lv_obj_set_style_bg_color(scr,lv_color_black(),LV_PART_MAIN);
  lv_obj_set_style_text_color(scr, lv_color_white(), 0);

  create_menu_icon(scr, HOME_SYMBOL, &scr_MainPagePresent);

  lv_obj_t * mode_box = create_invisible_box(scr, 240, 104);
  lv_obj_set_pos(mode_box, 0, 55);

  lv_obj_t * l_cool = create_hvac_icon(scr, COOL_SYMBOL, 0x6666e6);
  createModeBox(mode_box, l_cool, "Cool", 16, 1);

  lv_obj_t * l_heat = create_hvac_icon(scr, HEAT_SYMBOL, 0xff0000);
  createModeBox(mode_box, l_heat, "Heat", 16+56, 2);

  lv_obj_t * l_auto = lv_image_create(scr);
  lv_image_set_src(l_auto, &heat_cool);
  createModeBox(mode_box, l_auto, "Auto", 16+2*56, 4);

  lv_obj_t * l_aux = create_hvac_icon(scr, AUXHEAT_SYMBOL, 0xff0000);
  createModeBox(mode_box, l_aux, "Aux", 16+3*56, 3);

  lv_obj_t * b_off = label_button(mode_box, 240-32, 40, 0, 60, "Off");
  lv_obj_add_event_cb(b_off, change_mode_event_cb, LV_EVENT_ALL, (void *)0);
  if (hvac_mode == 0) {
    select_object(mode_box, b_off);
  }

  lv_obj_t * l_fan = lv_label_create(scr);
  lv_obj_set_size(l_fan, 240-32, 110);
  lv_obj_set_style_text_align(l_fan, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l_fan, LV_ALIGN_TOP_MID, 0, 180);
  lv_label_set_text(l_fan, "Fan");

  lv_obj_t * b_fan_hold = label_button(l_fan, 64, 40, 0, 20, "Hold", LV_ALIGN_TOP_LEFT);
  lv_obj_t * b_fan_pct = label_button(l_fan, 120, 40, 0, 20, "15%", LV_ALIGN_TOP_RIGHT);
  lv_obj_t * b_fan_off = label_button(l_fan, 240-32, 40, 0, 65, "Off");
  //lv_obj_add_event_cb(b_fan_off, change_mode_event_cb, LV_EVENT_ALL, (void *)0);
  select_object(l_fan, b_fan_off);

}
