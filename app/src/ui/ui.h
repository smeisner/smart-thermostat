#pragma once

#include "lvgl.h"
#include "fonts.h"
#include "interface.hpp"

enum PAGE {
    PAGE_NONE = 0,
    PAGE_ABSENT,
    PAGE_PRESENT,
    PAGE_QUICK_HVAC,
    PAGE_QUICK_HOLD,
    PAGE_MENU,
};

enum TRANSITION {
    TRANSITION_NONE,
    TRANSITION_FADE,
    TRANSITION_SLIDE,
};

enum CALIB_POINT_TYPE {
    CALIB_POINT_X,
    CALIB_POINT_PLUS,
    CALIB_POINT_CIRCLE,
};

lv_obj_t * uiMainPagePresent();
lv_obj_t * uiHVACSettings();
lv_obj_t * uiHoldSettings();
lv_obj_t * uiMainPageAbsent();
lv_obj_t * uiMenu();

void main_page_fade_out(int speed);
void main_page_fade_in(int speed);

void load_page(PAGE page, TRANSITION transition);
void page_event_cb(lv_event_t * e);
lv_obj_t * get_page();

//widgets
void select_object(lv_obj_t * parent, lv_obj_t *obj);
lv_obj_t * create_invisible_box(lv_obj_t *scr, int width, int height);
lv_obj_t *create_hvac_icon(lv_obj_t *scr, const char *icon, uint32_t color);
lv_obj_t * create_menu_icon(lv_obj_t *scr, const char *icon, PAGE page);
lv_obj_t * button(lv_obj_t *scr, int width, int height, int x, int y, lv_align_t align = LV_ALIGN_TOP_MID);
lv_obj_t * label_button(lv_obj_t *scr, int width, int height, int x, int y, const char *label, lv_align_t align = LV_ALIGN_TOP_MID);
lv_obj_t * spinbox(lv_obj_t *scr, int width, int height, int x, int y, int initial_value, int min_value, int max_value, int step, int sep_pos=0);
void spinbox_set_value(lv_obj_t * box, int value);
lv_obj_t * create_switch(lv_obj_t *scr, const char *label, bool *var);
lv_obj_t * create_popup(lv_obj_t *scr, const char *title, const char *message,
    void (*ok_cb)(lv_event_t *), void (*cancel_cb)(lv_event_t *), void *userData);
lv_obj_t * create_radio(lv_obj_t *scr, int *value, int count, const char **items);
lv_obj_t * create_text_entry(lv_obj_t * scr, char *str, int max_len, bool is_pass=false, bool is_numeric=false);
lv_obj_t * draw_calibration_point(lv_obj_t *scr, int x, int y, enum CALIB_POINT_TYPE type, lv_color_t color);


extern int hvac_mode;
void setHVACMode();

void uiInit();
void uiUpdate();
void uiUpdateMainPage();

#define TEMP_UNITS_C (OperatingParameters.tempUnits == 'C')
#define LV_USERDATA_TO_PAGE(x) ((PAGE)(reinterpret_cast<unsigned long>(x)))
