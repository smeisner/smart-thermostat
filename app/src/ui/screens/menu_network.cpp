#include <stdio.h>
#include "../ui.h"
#include "menu_internal.h"

char password[33];
char hostname[17] = "thermostat";
char ntp_server[33] = "pool.ntp.org";
bool matter_enabled = false;
char pairing_code[15] = "1234-5678-9012";

char mqtt_host[33];
char mqtt_user[33];
char mqtt_password[33];
int cur_ssid = 1;
const char *ssids[] = {"ssid1", "ssid2", "ssid3", NULL};

void menuPassword(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    lv_obj_t * textentry = create_text_entry(scr, password, 32, true);
}

void menuSSID(const char *title)
{
    lv_obj_t *scr = initLeafScr(title);
    // Do scan
    int i = 0;
    while(ssids[i]) {
        i++;
    }
    create_radio(scr, &cur_ssid, i, ssids);
}

void menuMQTTHostname(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    lv_obj_t * textentry = create_text_entry(scr, mqtt_host, 32, false);
}
void menuMQTTUsername(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    lv_obj_t * textentry = create_text_entry(scr, mqtt_user, 32, false);
}
void menuMQTTPassword(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    lv_obj_t * textentry = create_text_entry(scr, mqtt_password, 32, true);
}

static void update_matter_enabled(lv_obj_t *scr)
{
    lv_obj_t * label = lv_obj_get_child_by_type(scr, 0, &lv_label_class);
    lv_obj_t * qr = lv_obj_get_child_by_type(scr, 0, &lv_qrcode_class);
    if (matter_enabled) {
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(qr, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(qr, LV_OBJ_FLAG_HIDDEN);
    }
    //lv_obj_invalidate(label);
    //lv_obj_invalidate(qr);
}
static void enable_matter_cb(lv_event_t *e)
{
    lv_obj_t *scr = (lv_obj_t *)lv_event_get_user_data(e);
    update_matter_enabled(scr);
}

void menuMatter(const char *title) {
    lv_obj_t *scr = initLeafScr(title);

    lv_obj_t * enable = create_switch(scr, "Enable", &matter_enabled);
    lv_obj_t * sw = lv_obj_get_child_by_type(enable, 0, &lv_switch_class);
    lv_obj_add_event_cb(sw, enable_matter_cb, LV_EVENT_VALUE_CHANGED, (void *)scr);

    lv_obj_t * qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(qr, 150);
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_update(qr, pairing_code, 14);
    lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 75);

    lv_obj_t * manual = lv_label_create(scr);
    lv_label_set_text_static(manual, pairing_code);
    lv_obj_align_to(manual, qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    update_matter_enabled(scr);
}

void menuHostname(const char *title) {
    lv_obj_t * scr = initLeafScr(title, 0);
    lv_obj_t * textentry = create_text_entry(scr, hostname, 16, false);
}
void menuNTP(const char *title) {
    lv_obj_t * scr = initLeafScr(title, 0);
    lv_obj_t * textentry = create_text_entry(scr, ntp_server, 16, false);
}
