#pragma once

#include "lvgl.h"
#include "fonts.h"

void uiMainPagePresent();
void uiHVACSettings();
void uiHoldSettings();
void uiMainPageAbsent();
void uiMenu();

extern lv_obj_t * scr_HoldSettings;
extern lv_obj_t * scr_HVACSettings;
extern lv_obj_t * scr_MainPageAbsent;
extern lv_obj_t * scr_MainPagePresent;
extern lv_obj_t * scr_Menu;

void page_event_cb(lv_event_t * e);

//widgets
void select_object(lv_obj_t * parent, lv_obj_t *obj);
lv_obj_t * create_invisible_box(lv_obj_t *scr, int width, int height);
lv_obj_t *create_hvac_icon(lv_obj_t *scr, const char *icon, uint32_t color);
lv_obj_t * create_menu_icon(lv_obj_t *scr, const char *icon, lv_obj_t ** page);
lv_obj_t * button(lv_obj_t *scr, int width, int height, int x, int y, lv_align_t align = LV_ALIGN_TOP_MID);
lv_obj_t * label_button(lv_obj_t *scr, int width, int height, int x, int y, const char *label, lv_align_t align = LV_ALIGN_TOP_MID);
lv_obj_t * create_switch(lv_obj_t *scr, const char *label, bool *var);
lv_obj_t * create_popup(lv_obj_t *scr, const char *title, const char *message,
    void (*ok_cb)(lv_event_t *), void (*cancel_cb)(lv_event_t *), void *userData);
lv_obj_t * create_radio(lv_obj_t *scr, int *value, int count, const char **items);
lv_obj_t * create_text_entry(lv_obj_t * scr, char *str, int max_len, bool is_pass);

extern int hvac_mode;
void setHVACMode();
