#include <stdio.h>
#include "../ui.h"
#include "menu_internal.h"

bool auto_brightness;
int brightness = 30;
bool auto_dim;
int dim = 50;
int dim_delay = 20;
int volume = 90;
int reversingValve = 0;
int stages = 0;
int auxHeat = 0;
int timezone = 0;
bool dst = true;

static void bool_clicked(lv_event_t *e)
{
    lv_obj_t * obj = lv_event_get_target_obj(e);
    bool *ptr = (bool *)lv_event_get_user_data(e);
    if (! ptr) return;
    *ptr = lv_obj_get_state(obj) & LV_STATE_CHECKED;
}

static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target_obj(e);
    int *ptr = (int *)lv_event_get_user_data(e);
    *ptr = lv_slider_get_value(slider);
}

static void menu_adjust_brightness(const char *title, bool *enable_auto, int min_value, int *value, int *delay)
{
    lv_obj_t * scr = initLeafScr(title);

    lv_obj_t * cb;
    cb = create_switch(scr, "Auto adjust", enable_auto);

    lv_obj_t *bright_l = lv_label_create(scr);
    lv_label_set_text(bright_l, "Brightness");
    lv_obj_align_to(bright_l, cb, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);

    /*Create a slider in the center of the display*/
    lv_obj_t * slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 240-40);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, value);
    lv_slider_set_range(slider, min_value, 100);
    lv_slider_set_value(slider, *value, LV_ANIM_OFF);
    lv_obj_set_style_anim_duration(slider, 2000, 0);
    lv_obj_align_to(slider, bright_l, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 15);

    if (delay) {
        lv_obj_t *delay_l = lv_label_create(scr);
        lv_label_set_text(delay_l, "Delay");
        lv_obj_align_to(delay_l, slider, LV_ALIGN_OUT_BOTTOM_LEFT, -10, 15);

        /*Create a slider in the center of the display*/
        lv_obj_t * delay_slider = lv_slider_create(scr);
        lv_obj_set_width(delay_slider, 240-40);
        lv_obj_add_event_cb(delay_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, delay);
        lv_slider_set_range(delay_slider, 0, 100);
        lv_slider_set_value(delay_slider, *delay, LV_ANIM_OFF);
        lv_obj_set_style_anim_duration(delay_slider, 2000, 0);
        lv_obj_align_to(delay_slider, delay_l, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 15);

    }
}
void menuBrightness(const char *title)
{
    menu_adjust_brightness(title, &auto_brightness, 10, &brightness, NULL);
}

void menuDim(const char *title)
{
    menu_adjust_brightness(title, &auto_dim, 0, &dim, &dim_delay);
}

void menuBeep(const char *title) {
    lv_obj_t * scr = initLeafScr(title);

    lv_obj_t *volume_l = lv_label_create(scr);
    lv_label_set_text(volume_l, "Volume");

    /*Create a slider in the center of the display*/
    lv_obj_t * slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 240-40);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, &volume);
    lv_slider_set_value(slider, volume, LV_ANIM_OFF);
    lv_obj_set_style_anim_duration(slider, 2000, 0);
    lv_obj_align_to(slider, volume_l, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 15);
}

void menuHVACStages(const char *title) {
    lv_obj_t * scr = initLeafScr(title);
    lv_obj_t * radio = create_radio(scr, &stages, 2, (const char *[]){"1 Stage", "2 Stage"});
}
void menuHVACRV(const char *title) {
    lv_obj_t * scr = initLeafScr(title);
    lv_obj_t * radio = create_radio(scr, &reversingValve, 3,
        (const char *[]){"None", "Active on cool", "Active on heat"});

}
void menuHVACAuxHeat(const char *title)
{
    lv_obj_t * scr = initLeafScr(title);
    lv_obj_t * radio = create_radio(scr, &auxHeat, 2,
        (const char *[]){"Disabled", "Available"});
}

static void tz_handler(lv_event_t *e)
{
    lv_obj_t * obj = lv_event_get_current_target_obj(e);
    timezone = lv_roller_get_selected(obj);
}
void menuTimezone(const char *title) {
    lv_obj_t * scr = initLeafScr(title);
    lv_obj_set_size(scr, 240-32, 320-42);
    lv_obj_t * roller = lv_roller_create(scr);
    lv_roller_set_options(roller,
        "+11\n"
        "+10\n"
        "+9\n"
        "+8\n"
        "+7\n"
        "+6\n"
        "+5\n"
        "+4\n"
        "+3 FET\n"
        "+2 EET\n"
        "+1 CET\n"
        "GMT\n"
        "-1\n"
        "-2\n"
        "-3\n"
        "-4\n"
        "GMT-5 (EST)\n"
        "GMT-6 (CST)\n"
        "GMT-7 (MST)\n"
        "GMT-8 (PST)\n"
        "GMT-9 (AKST)\n"
        "GMT-10 (HST)\n"
        "GMT-11\n"
        "GMT-12",
        LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(roller, timezone, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller, tz_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_align(roller, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *sw = create_switch(scr, "Enable DST", &dst);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, 0, 200);
}
#define ALIGN_OFFSET_X (5)
#define ALIGN_OFFSET_Y (7)

static int align_state = 0;
static int align[6][2];
static void align_cb(lv_event_t *e)
{
    lv_point_t p;
    lv_obj_t *img = (lv_obj_t *)lv_event_get_user_data(e);
    lv_indev_t * indev = lv_indev_get_act();
    lv_indev_get_point(indev, &p);
    printf("Got point %d: %d x %d\n", align_state, p.x, p.y);
    align[align_state][0] = p.x;
    align[align_state][1] = p.y;
    align_state++;
    if (align_state == 1) {
        lv_obj_set_pos(img, 220-ALIGN_OFFSET_X, 20-ALIGN_OFFSET_Y);
    } else if (align_state == 2) {
        lv_obj_set_pos(img, 60-ALIGN_OFFSET_X, 167-ALIGN_OFFSET_Y);
    } else if (align_state == 3) {
        lv_obj_set_pos(img, 180-ALIGN_OFFSET_X, 155-ALIGN_OFFSET_Y);
    } else if (align_state == 4) {
        lv_obj_set_pos(img, 13-ALIGN_OFFSET_X, 300-ALIGN_OFFSET_Y);
    } else if (align_state == 5) {
        lv_obj_set_pos(img, 225-ALIGN_OFFSET_X, 295-ALIGN_OFFSET_Y);
    } else if (align_state == 6) {
        lv_obj_remove_event_cb(scr_leaf, align_cb);
        //FIXME: Do calibration here
        lv_screen_load_anim(scr_Menu, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, 0);
    }
}
void menuCalibrateScreen(const char *title) {
    lv_obj_clean(scr_leaf);
    lv_scr_load_anim(scr_leaf, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, 0);

    align_state = 0;
    lv_obj_t *scr = scr_leaf;
    lv_obj_t *img = lv_image_create(scr);
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text_static(label, "press each " LV_SYMBOL_CLOSE);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    lv_image_set_src(img, LV_SYMBOL_CLOSE);
    lv_obj_set_pos(img, 11-ALIGN_OFFSET_X, 13-ALIGN_OFFSET_Y);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, align_cb, LV_EVENT_CLICKED, img);
}

void cancel_back_cb(lv_event_t *e)
{
    lv_screen_load_anim(scr_Menu, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, 0);
}

void reboot_cb(lv_event_t *e)
{
    // Do reboot
    cancel_back_cb(e);
}

void menuReboot(const char *title) {
    lv_obj_t * scr = initLeafScr(title);
    create_popup(scr, "Reboot", "This will reboot the thermostat", reboot_cb, cancel_back_cb, NULL);
}

void reset_cb(lv_event_t *e)
{
    // Reset factory settings
    cancel_back_cb(e);
}

void menuFactoryReset(const char *title) {
    lv_obj_t * scr = initLeafScr(title);
    create_popup(scr, "WARNING", "This will reset your thermostat to factory settings, are you sure you want to proceed?", reset_cb, cancel_back_cb, NULL);
}
