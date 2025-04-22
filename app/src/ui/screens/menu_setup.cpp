#include <stdio.h>
#include "thermostat.hpp"
#include "../ui.h"
#include "menu_internal.h"

static int itemSelect;
//FIXME : Need to be put in OperatingParameters
bool auto_brightness;
int brightness = 30;
bool auto_dim;
int dim = 50;
int dim_delay = 20;
int volume = 90;
bool dst = true;

static const char *diable_available_txt[] = {"Disabled", "Available"};

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
void change_temp_units(lv_event_t *e)
{
    if (itemSelect == 0) {
        updateTempUnits('F');
    } else {
        updateTempUnits('C');
    }
}

void menuTempUnits(const char *title) {
    static const char *degrees[] = {"00 °F", "00.0 °C"};
    lv_obj_t * scr = initLeafScr(title);
    if (TEMP_UNITS_C)
        itemSelect = 1;
    else
        itemSelect = 0;
    lv_obj_t * radio = create_radio(scr, &itemSelect, 2, degrees);
    lv_obj_add_event_cb(radio, change_temp_units, LV_EVENT_CLICKED, NULL);
}
void menuClockFormat(const char *title) {}

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

static void beep_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_current_target_obj(e);
    volume = lv_slider_get_value(slider);
    updateBeepEnable(!!volume);
}

void menuBeep(const char *title) {
    lv_obj_t * scr = initLeafScr(title);

    lv_obj_t *volume_l = lv_label_create(scr);
    lv_label_set_text(volume_l, "Volume");

    /*Create a slider in the center of the display*/
    lv_obj_t * slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 240-40);
    lv_obj_add_event_cb(slider, beep_changed_cb, LV_EVENT_VALUE_CHANGED, &volume);
    lv_slider_set_value(slider, volume, LV_ANIM_OFF);
    lv_obj_set_style_anim_duration(slider, 2000, 0);
    lv_obj_align_to(slider, volume_l, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 15);
}

enum RADIO_TYPE {
    RADIO_TWOSTAGE,
    RADIO_REVERSEVALVE,
    RADIO_COOL,
    RADIO_FAN,
    RADIO_AUXHEAT,
};

static void hvac_change_cb(lv_event_t *e)
{
    enum RADIO_TYPE menu = (enum RADIO_TYPE)(long)lv_event_get_user_data(e);
    bool newStage = OperatingParameters.hvac2StageHeatEnable;
    bool newRV = OperatingParameters.hvacReverseValveEnable;
    bool newAux = OperatingParameters.hvacAuxHeatEnable;
    bool newCool = OperatingParameters.hvacCoolEnable;
    bool newFan = OperatingParameters.hvacFanEnable;

    if (menu == RADIO_TWOSTAGE) {
        newStage = !! itemSelect;
    } else if (menu == RADIO_REVERSEVALVE) {
        newRV = !! itemSelect;
    } else if (menu == RADIO_AUXHEAT) {
        newAux = !! itemSelect;
    } else if (menu == RADIO_COOL) {
        newCool = !! itemSelect;
    } else if (menu == RADIO_FAN) {
        newFan = !! itemSelect;
    }
    updateHVACSettings(newStage, newRV, newAux, newCool, newFan);
}

void menuHVACStages(const char *title) {
    static const char *stages[] = {"1 Stage", "2 Stage"};
    lv_obj_t * scr = initLeafScr(title);
    itemSelect = OperatingParameters.hvac2StageHeatEnable;
    lv_obj_t * radio = create_radio(scr, &itemSelect, 2, stages);
    lv_obj_add_event_cb(radio, hvac_change_cb, LV_EVENT_CLICKED, (void *)(long)RADIO_TWOSTAGE);
}

void menuHVACRV(const char *title) {
    static const char *valve[] ={"None", "Active on cool", "Active on heat"};

    lv_obj_t * scr = initLeafScr(title);
    itemSelect = OperatingParameters.hvacReverseValveEnable;
    lv_obj_t * radio = create_radio(scr, &itemSelect, 3, valve);
    lv_obj_add_event_cb(radio, hvac_change_cb, LV_EVENT_CLICKED, (void *)(long)RADIO_REVERSEVALVE);
    }

void menuHVACAuxHeat(const char *title)
{
    lv_obj_t * scr = initLeafScr(title);
    itemSelect = OperatingParameters.hvacAuxHeatEnable;
    lv_obj_t * radio = create_radio(scr, &itemSelect, 2, diable_available_txt);
    lv_obj_add_event_cb(radio, hvac_change_cb, LV_EVENT_CLICKED, (void *)(long)RADIO_AUXHEAT);
}
void menuHVACCool(const char *title)
{
    lv_obj_t * scr = initLeafScr(title);
    itemSelect = OperatingParameters.hvacCoolEnable;
    lv_obj_t * radio = create_radio(scr, &itemSelect, 2, diable_available_txt);
    lv_obj_add_event_cb(radio, hvac_change_cb, LV_EVENT_CLICKED, (void *)(long)RADIO_COOL);
}
void menuHVACFan(const char *title)
{
    lv_obj_t * scr = initLeafScr(title);
    itemSelect = OperatingParameters.hvacFanEnable;
    lv_obj_t * radio = create_radio(scr, &itemSelect, 2, diable_available_txt);
    lv_obj_add_event_cb(radio, hvac_change_cb, LV_EVENT_CLICKED, (void *)(long)RADIO_FAN);
}

static void tz_handler(lv_event_t *e)
{
    lv_obj_t * obj = lv_event_get_current_target_obj(e);
    updateTimezone(lv_roller_get_selected(obj), false);
}
void menuTimezone(const char *title) {
    lv_obj_t * scr = initLeafScr(title);
    lv_obj_set_size(scr, 240-32, 320-42);
    lv_obj_t * roller = lv_roller_create(scr);
    lv_roller_set_options(roller,
        "GMT+11\n"
        "GMT+10\n"
        "GMT+9\n"
        "GMT+8\n"
        "GMT+7\n"
        "GMT+6\n"
        "GMT+5\n"
        "GMT+4\n"
        "GMT+3 (FET)\n"
        "GMT+2 (EET)\n"
        "GMT+1 (CET)\n"
        "GMT\n"
        "GMT-1\n"
        "GMT-2\n"
        "GMT-3\n"
        "GMT-4\n"
        "GMT-5 (EST)\n"
        "GMT-6 (CST)\n"
        "GMT-7 (MST)\n"
        "GMT-8 (PST)\n"
        "GMT-9 (AKST)\n"
        "GMT-10 (HST)\n"
        "GMT-11\n"
        "GMT-12",
        LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(roller, OperatingParameters.timezone_sel, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller, tz_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_align(roller, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *sw = create_switch(scr, "Enable DST", &dst);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, 0, 200);
}


void cancel_back_cb(lv_event_t *e)
{
    transition_leaf_to_menu();
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
