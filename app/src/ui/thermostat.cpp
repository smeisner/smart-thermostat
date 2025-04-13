#include "lvgl.h"

extern lv_font_t roboto_semicond_medium_num_148;
extern const lv_image_dsc_t heat_cool;

# ifdef USE_AWESOME
extern lv_font_t fa_672_icons_28;
#define ICON_FONT_28 fa_672_icons_28
#define MENU_SYMBOL "\xEF\x83\x89"
#define HEAT_SYMBOL "\xEF\x81\xAD"
#define COOL_SYMBOL "\xEF\x8B\x9C"
#define FAN_SYMBOL "\xEF\xA1\xA3"

#else
extern lv_font_t materialdesign_32;
extern lv_font_t materialdesign_24;
#define ICON_FONT_28 materialdesign_32
#define ICON_FONT_22 materialdesign_24
#define HOME_SYMBOL     "\xF3\xB0\x8B\x9C" //F02DC
#define MENU_SYMBOL     "\xF3\xB0\x8D\x9C" //F035C
#define FAN_SYMBOL      "\xF3\xB0\x88\x90" //F0210
#define FAN_AUTO_SYMBOL "\xF3\xB1\x9C\x9D" //F171D
#define HEAT_SYMBOL     "\xF3\xB0\x88\xB8" //F0238
#define COOL_SYMBOL     "\xF3\xB0\x9C\x97" //F0717
#define AUXHEAT_SYMBOL  "\xF3\xB1\x97\x97" //F15D7
#define WATER_SYMBOL    "\xF3\xB0\xB8\x8A" //F0E0A

#define WEATHER_CLEAR_NIGHT     "\xF3\xB0\x96\x94" //F0594 # clear-night
#define WEATHER_CLOUDY          "\xF3\xB0\x96\x90" //F0590 # cloudy
#define WEATHER_EXCEPTIONAL     "\U000F0F2F" # exceptional
#define WEATHER_FOG             "\U000F0591", # fog
#define WEATHER_HAIL            "\U000F0592", # hail
#define WEATHER_LIGHTNING       "\U000F0593", # lightning
#define WEATHER_LIGHTNING_RAINY "\U000F067E", # lightning-rainy
#define WEATHER_PARTLYCLOUDY    "\U000F0595", # partlycloudy
#define WEATHER_POURING         "\U000F0596", # pouring
#define WEATHER_RAINY           "\U000F0597", # rainy
#define WEATHER_SNOWY           "\U000F0598", # snowy
#define WEATHER_SNOWY_RAINY     "\U000F067F", # snowy-rainy
#define WEATHER_SUNNY           "\xF3\xB0\x96\x99" // F0599 # sunny
#define WEATHER_WINDY           "\U000F059D", # windy
#define WEATHER_WINDY_VAIRANT   "\U000F059E", # windy-variant
#define WEATHER_SUNNY_OFF       "\U000F14E4", # sunny-off

#endif

int target_temp = 77;
int hvac_mode = 0;
lv_obj_t * l_set_temp;
lv_obj_t * l_hvac;
lv_obj_t * l_hvac_auto;
lv_obj_t * scr_MainPagePresent;
lv_obj_t * scr_MainPageAbsent;
lv_obj_t * scr_QuickSettings;

static void setHVACMode() {
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
}
static void set_temp_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        int dir = (long)lv_event_get_user_data(e);
        target_temp += dir;
        lv_label_set_text_fmt(l_set_temp, "%d", target_temp);
    }
}
static void change_mode_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        hvac_mode = (long)lv_event_get_user_data(e);
        setHVACMode();
    }
}

static void page_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
      lv_obj_t ** page = (lv_obj_t **)lv_event_get_user_data(e);
      lv_scr_load_anim(*page, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, 0);
    }
}

lv_obj_t *createModeIcon(lv_obj_t *scr, const char *icon, uint32_t color)
{
    lv_obj_t * l_icon = lv_label_create(scr);
    lv_obj_set_style_text_align(l_icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l_icon, icon);
    lv_obj_set_style_text_font(l_icon, &ICON_FONT_28, 0);
    lv_obj_set_style_text_color(l_icon, lv_color_hex(color), 0);
    return l_icon;
}

void uiMainPagePresent()
{
    scr_MainPagePresent = lv_obj_create(NULL);
    lv_obj_t * scr = scr_MainPagePresent;

	lv_obj_set_style_bg_color(scr,lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);

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
    lv_obj_set_style_text_font(l_set_temp, &lv_font_montserrat_28, 0);

    // Increase Temperature
    lv_obj_t * l_temp_inc = lv_label_create(scr);
    lv_obj_set_style_text_align(l_temp_inc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_temp_inc, LV_ALIGN_TOP_MID, 50, 220);
    lv_label_set_text(l_temp_inc, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(l_temp_inc, &lv_font_montserrat_28, 0);

    // Decrease Temperature
    lv_obj_t * l_temp_dec = lv_label_create(scr);
    lv_obj_set_style_text_align(l_temp_dec, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_temp_dec, LV_ALIGN_TOP_MID, -50, 220);
    lv_label_set_text(l_temp_dec, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(l_temp_dec, &lv_font_montserrat_28, 0);

    // Outside Weather
    lv_obj_t * l_weather = lv_label_create(scr);
    lv_obj_set_style_text_align(l_weather, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_weather, LV_ALIGN_TOP_MID, -25, 0);
    lv_label_set_text(l_weather, WEATHER_SUNNY);
    lv_obj_set_style_text_font(l_weather, &ICON_FONT_22, 0);
    lv_obj_set_style_text_color(l_weather, lv_color_hex(0xff0000), 0);
    lv_obj_update_layout(l_weather);

    // Outside temperature
    lv_obj_t * l_outside_temp = lv_label_create(scr);
    lv_obj_set_style_text_align(l_outside_temp, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_outside_temp, LV_ALIGN_TOP_MID, 0, 0);
    lv_label_set_text(l_outside_temp, "66");
    lv_obj_set_style_text_font(l_outside_temp, &lv_font_montserrat_22, 0);

    // Time
    lv_obj_t * l_time = lv_label_create(scr);
    lv_obj_set_style_text_align(l_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_time, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(l_time, "23:59");
    lv_obj_set_style_text_font(l_time, &lv_font_montserrat_22, 0);

    // Menu
    lv_obj_t * l_menu = lv_label_create(scr);
    lv_obj_set_style_text_align(l_menu, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_menu, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_label_set_text(l_menu, MENU_SYMBOL);
    lv_obj_set_style_text_font(l_menu, &ICON_FONT_22, 0);


    // Humidity
    lv_obj_t * l_humid = lv_label_create(scr);
    lv_obj_set_style_text_align(l_humid, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_humid, LV_ALIGN_TOP_MID, 0, 45);
    lv_label_set_text(l_humid, WATER_SYMBOL " 60%");
    lv_obj_set_style_text_font(l_humid, &ICON_FONT_22, 0);

    // Status

    l_hvac = createModeIcon(scr, HEAT_SYMBOL, 0xff0000);
    lv_obj_align(l_hvac, LV_ALIGN_BOTTOM_MID, 0, -10);
    l_hvac_auto = lv_image_create(scr);
    lv_image_set_src(l_hvac_auto, &heat_cool);
    lv_obj_align(l_hvac_auto, LV_ALIGN_BOTTOM_MID, 0, -10);
    setHVACMode();

    lv_obj_t * l_fan = createModeIcon(scr, FAN_SYMBOL, 0xffffff);
    lv_obj_align(l_fan, LV_ALIGN_BOTTOM_MID, 30, -10);

    lv_obj_t * inc_box = lv_obj_create(scr);
    lv_obj_set_size(inc_box, 120, 180);
    lv_obj_set_pos(inc_box, 120, 75);
    lv_obj_add_flag(inc_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(inc_box, set_temp_event_cb, LV_EVENT_ALL, (void *)1);
    lv_obj_set_style_bg_opa(inc_box, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(inc_box, LV_STATE_DEFAULT, 0);

    lv_obj_t * dec_box = lv_obj_create(scr);
    lv_obj_set_size(dec_box, 120, 180);
    lv_obj_set_pos(dec_box, 0, 75);
    lv_obj_add_flag(dec_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dec_box, set_temp_event_cb, LV_EVENT_ALL, (void *)-1);
    lv_obj_set_style_bg_opa(dec_box, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(dec_box, LV_STATE_DEFAULT, 0);

    lv_obj_t * quick_box = lv_obj_create(scr);
    lv_obj_set_size(quick_box, 240, 50);
    lv_obj_set_pos(quick_box, 0, 270);
    lv_obj_add_flag(quick_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(quick_box, page_event_cb, LV_EVENT_ALL, (void *)&scr_QuickSettings);
    lv_obj_set_style_bg_opa(quick_box, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(quick_box, LV_STATE_DEFAULT, 0);

}
lv_obj_t * button(lv_obj_t *scr, int width, int height, int x, int y,
     lv_align_t align = LV_ALIGN_TOP_MID)
{
    lv_obj_t * b_off = lv_button_create(scr);
    lv_obj_set_size(b_off, width, height);
    lv_obj_align(b_off, align, x, y);
    lv_obj_set_style_bg_opa(b_off, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_border_color(b_off, lv_color_white(), 0);
    lv_obj_set_style_border_width(b_off, 2, 0);
    lv_obj_set_style_shadow_width(b_off, 0, 0);
    return b_off;
}

lv_obj_t * label_button(lv_obj_t *scr, int width, int height, int x, int y, const char *label,
     lv_align_t align = LV_ALIGN_TOP_MID)
{
    lv_obj_t *b_but = button(scr, width, height, x, y, align);
    lv_obj_t * l_label = lv_label_create(b_but);
    lv_label_set_text(l_label, label);
    lv_obj_center(l_label);
    return b_but;
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
}

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

void uiQuickSettings()
{
  scr_QuickSettings = lv_obj_create(NULL);
  lv_obj_t * scr = scr_QuickSettings;

  lv_obj_set_style_bg_color(scr,lv_color_black(),LV_PART_MAIN);
  lv_obj_set_style_text_color(scr, lv_color_white(), 0);

  lv_obj_t * l_cool = createModeIcon(scr, COOL_SYMBOL, 0x6666e6);
  createModeBox(scr, l_cool, "Cool", 16, 5, 1);

  lv_obj_t * l_heat = createModeIcon(scr, HEAT_SYMBOL, 0xff0000);
  createModeBox(scr, l_heat, "Heat", 16+56, 5, 2);

  lv_obj_t * l_auto = lv_image_create(scr);
  lv_image_set_src(l_auto, &heat_cool);
  createModeBox(scr, l_auto, "Auto", 16+2*56, 5, 4);

  lv_obj_t * l_aux = createModeIcon(scr, AUXHEAT_SYMBOL, 0xff0000);
  createModeBox(scr, l_aux, "Aux", 16+3*56, 5, 3);

  lv_obj_t * b_off = label_button(scr, 240-32, 30, 0, 65, "Off");
  lv_obj_add_event_cb(b_off, change_mode_event_cb, LV_EVENT_ALL, (void *)0);

  lv_obj_t * l_hold = lv_label_create(scr);
  lv_obj_set_size(l_hold, 240, 115);
  lv_obj_set_style_text_align(l_hold, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l_hold, LV_ALIGN_TOP_MID, 0, 105);
  lv_label_set_text(l_hold, "Hold");

  lv_obj_t * b_nohold = label_button(l_hold, 240-32, 30, 0, 16, "No Hold");
  lv_obj_t * b_hold_next = label_button(l_hold, 240-32, 30, 0, 50, "Until Next Event");
  lv_obj_t * b_hold_time = label_button(l_hold, 240-32, 30, 0, 84, "30 minutes");

  lv_obj_t * l_fan = lv_label_create(scr);
  lv_obj_set_size(l_fan, 240-32, 46);
  lv_obj_set_style_text_align(l_fan, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l_fan, LV_ALIGN_TOP_MID, 0, 230);
  lv_label_set_text(l_fan, "Fan");

  lv_obj_t * b_fan_hold = label_button(l_fan, 64, 30, 0, 16, "Hold", LV_ALIGN_TOP_LEFT);
  lv_obj_t * b_fan_pct = label_button(l_fan, 120, 30, 0, 16, "15%", LV_ALIGN_TOP_RIGHT);

  lv_obj_t * b_back = label_button(scr, 240-32, 30, 0, -5, "Back", LV_ALIGN_BOTTOM_MID);
  lv_obj_add_event_cb(b_back, page_event_cb, LV_EVENT_ALL, (void *)&scr_MainPagePresent);

}

void my_timer(lv_timer_t * timer)
{
    lv_scr_load_anim(scr_MainPagePresent, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, 0);
}

extern "C" void thermostat()
{
  uiMainPagePresent();
  uiQuickSettings();
  uiMainPageAbsent();
  lv_scr_load(scr_MainPageAbsent);
  lv_timer_t * timer = lv_timer_create(my_timer, 4000, NULL);
  lv_timer_set_repeat_count(timer, 1);
}
