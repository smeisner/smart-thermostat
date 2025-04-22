#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "thermostat.hpp"
#include "interface.hpp"
#include "../ui.h"
#include "menu_internal.h"

static char textvalue[33];
static bool enabled;

extern bool wifiScanActive;
// Don't use extern bool wifiScanActive because it is asynchronoous
static bool wifi_scanning = false;
static lv_obj_t *ssid_state = NULL;
static lv_obj_t *ssid_label;
static lv_obj_t *ssid_roller;

char ntp_server[33] = "pool.ntp.org";

enum CB_TYPE {
    HOSTNAME,
    WIFI_SSID,
    WIFI_PASSWORD,
    MQTT_HOST,
    MQTT_PORT,
    MQTT_USERNAME,
    MQTT_PASSWORD,
};

static void textbox_changed(enum CB_TYPE type, bool writeNVS=true)
{
    int value;
    switch(type) {
        case HOSTNAME:
            updateFriendlyHost(textvalue);
            break;
        case WIFI_SSID:
            updateWifiCreds(textvalue, WifiCreds.password, writeNVS);
            break;
        case WIFI_PASSWORD:
            updateWifiCreds(WifiCreds.ssid, textvalue, writeNVS);
            break;
        case MQTT_HOST:
            updateMqttParameters(OperatingParameters.MqttEnabled, textvalue, OperatingParameters.MqttBrokerPort, OperatingParameters.MqttBrokerUsername, OperatingParameters.MqttBrokerPassword);
            break;
        case MQTT_PORT:
            value = atoi(textvalue);
            updateMqttParameters(OperatingParameters.MqttEnabled, OperatingParameters.MqttBrokerHost, value, OperatingParameters.MqttBrokerUsername, OperatingParameters.MqttBrokerPassword);
            break;
        case MQTT_USERNAME:
            updateMqttParameters(OperatingParameters.MqttEnabled, OperatingParameters.MqttBrokerHost, OperatingParameters.MqttBrokerPort, textvalue, OperatingParameters.MqttBrokerPassword);
            break;
        case MQTT_PASSWORD:
            updateMqttParameters(OperatingParameters.MqttEnabled, OperatingParameters.MqttBrokerHost, OperatingParameters.MqttBrokerPort, OperatingParameters.MqttBrokerUsername, textvalue);
            break;

    }
}

static void textbox_changed_cb(lv_event_t *e)
{
    enum CB_TYPE type = (enum CB_TYPE)(long)lv_event_get_user_data(e);
    textbox_changed(type);
}

void menuPassword(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    lv_strncpy(textvalue, WifiCreds.password, sizeof(textvalue));
    lv_obj_t * textentry = create_text_entry(scr, textvalue, sizeof(WifiCreds.password)-1, false);
    lv_obj_add_event_cb(textentry, textbox_changed_cb, LV_EVENT_READY, (void *)(long)WIFI_PASSWORD);
}

void update_ssid_state(bool refresh_roller)
{
    if (! ssid_state)
        return;

    if (refresh_roller) {
        const char *ssids = Get_WiFiSSID_DD_List();
        lv_roller_set_options(ssid_roller, ssids, LV_ROLLER_MODE_INFINITE);
        lv_obj_align(ssid_roller, LV_ALIGN_TOP_MID, 0, 120);
        // lvgl 9.2 does not have lv_roller_set_selected_str
        // lv_roller_set_selected(ssid_roller, WifiCreds.ssid, LV_ANIM_OFF);
        if (lv_strlen(WifiCreds.ssid) != 0) {
            int i = 0;
            const char *pos = ssids;
            while (true) {
                if (strncmp(pos, WifiCreds.ssid, lv_strlen(WifiCreds.ssid)) == 0) {
                    lv_roller_set_selected(ssid_roller, i, LV_ANIM_OFF);
                    break;
                }
                pos = strchr(pos, '\n');
                if (pos == NULL) {
                    break;
                }
                pos++;
                i++;
            }
        }
    }

    lv_obj_t *label = lv_obj_get_child(ssid_label, 0);
    lv_label_set_text(label, WifiCreds.ssid);
    lv_obj_center(label);

    label = lv_obj_get_child(ssid_state, 0);
    if (wifi_scanning) {
        lv_label_set_text(label, "Scannning");
    } else {
        lv_label_set_text(label, "Scan");
    }
    lv_obj_center(label);
}

static void wifi_scan_complete_cb()
{
    wifi_scanning = false;
    display_lock();
    update_ssid_state(true);
    display_unlock();
}

static void start_stop_scan_cb(lv_event_t *e)
{
    if (wifiScanActive) {
      stopWifiScan();
      wifi_scanning = false;
    } else {
      wifi_scanning = true;
      StartWifiScan(wifi_scan_complete_cb);
    }
    update_ssid_state(false);
}

static void manual_ssid_back_cb(lv_event_t *e)
{
    menuSSID("SSID");
}

static void manual_ssid_cb(lv_event_t *e)
{
    lv_obj_t *scr = initLeafScr("SSID", 0, manual_ssid_back_cb);
    ssid_state = NULL;
    lv_strncpy(textvalue, WifiCreds.ssid, sizeof(textvalue));
    lv_obj_t * textentry = create_text_entry(scr, textvalue, sizeof(WifiCreds.ssid)-1, false);
    lv_obj_add_event_cb(textentry, textbox_changed_cb, LV_EVENT_READY, (void *)(long)WIFI_SSID);
}

static void ssid_roller_cb(lv_event_t *e)
{
    lv_roller_get_selected_str(ssid_roller, textvalue, sizeof(textvalue));
    textbox_changed(WIFI_SSID, false);
    update_ssid_state(false);
}

static void ssid_back_cb(lv_event_t *e)
{
    // Delay writing ssid until exiting menu
    updateWifiCreds(WifiCreds.ssid, WifiCreds.password);
    transition_leaf_to_menu();
}

void menuSSID(const char *title)
{
    wifi_scanning = wifiScanActive;
    lv_obj_t *scr = initLeafScr(title, 16, ssid_back_cb);
    ssid_state = label_button(scr, 240-32, 40, 0, 20, "");
    lv_obj_add_event_cb(ssid_state, start_stop_scan_cb, LV_EVENT_CLICKED, NULL);

    ssid_label = label_button(scr, 240-32, 40, 0, 70, "");
    lv_obj_add_event_cb(ssid_label, manual_ssid_cb, LV_EVENT_CLICKED, NULL);

    ssid_roller = lv_roller_create(scr);
    lv_obj_add_event_cb(ssid_roller, ssid_roller_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_width(ssid_roller, 240-32);

    update_ssid_state(true);
}

static void mqtt_enable_cb(lv_event_t *e)
{
    updateMqttParameters(enabled, OperatingParameters.MqttBrokerHost, OperatingParameters.MqttBrokerPort, OperatingParameters.MqttBrokerUsername, OperatingParameters.MqttBrokerPassword);

}

void enableMQTTMenu(void *_e)
{
    lv_obj_t *page = lv_event_get_current_target_obj((lv_event_t *)_e);
    bool enable = OperatingParameters.MqttEnabled;
    int i = 1;  // item '0' is the enable menu
    while (true) {
        lv_obj_t *obj = lv_obj_get_child(page, i++);
        if (! obj)
            break;
        if (enable) {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
void menuMQTTEnable(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    enabled = OperatingParameters.MqttEnabled;
    lv_obj_t * enable = create_switch(scr, "Enable", &enabled);
    lv_obj_add_event_cb(enable, mqtt_enable_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void menuMQTTHostname(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    lv_strncpy(textvalue, OperatingParameters.MqttBrokerHost, sizeof(textvalue));
    lv_obj_t * textentry = create_text_entry(scr, textvalue, sizeof(OperatingParameters.MqttBrokerHost)-1, false);
    lv_obj_add_event_cb(textentry, textbox_changed_cb, LV_EVENT_READY, (void *)(long)MQTT_HOST);
}
void menuMQTTPort(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    lv_snprintf(textvalue, sizeof(textvalue), "%d", OperatingParameters.MqttBrokerPort);
    lv_strncpy(textvalue, OperatingParameters.MqttBrokerHost, sizeof(textvalue));
    lv_obj_t * textentry = create_text_entry(scr, textvalue, 5, false, true);
    lv_obj_add_event_cb(textentry, textbox_changed_cb, LV_EVENT_READY, (void *)(long)MQTT_PORT);
}
void menuMQTTUsername(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    lv_strncpy(textvalue, OperatingParameters.MqttBrokerUsername, sizeof(textvalue));
    lv_obj_t * textentry = create_text_entry(scr, textvalue, sizeof(OperatingParameters.MqttBrokerUsername)-1, false);
    lv_obj_add_event_cb(textentry, textbox_changed_cb, LV_EVENT_READY, (void *)(long)MQTT_USERNAME);
}
void menuMQTTPassword(const char *title)
{
    lv_obj_t *scr = initLeafScr(title, 0);
    lv_strncpy(textvalue, OperatingParameters.MqttBrokerPassword, sizeof(textvalue));
    lv_obj_t * textentry = create_text_entry(scr, textvalue, sizeof(OperatingParameters.MqttBrokerPassword)-1, false);
    lv_obj_add_event_cb(textentry, textbox_changed_cb, LV_EVENT_READY, (void *)(long)MQTT_PASSWORD);
}


void menuHostname(const char *title) {
    lv_obj_t * scr = initLeafScr(title, 0);
    lv_strncpy(textvalue, OperatingParameters.FriendlyName, sizeof(textvalue));
    lv_obj_t * textentry = create_text_entry(scr, textvalue, 31, false);
    lv_obj_add_event_cb(textentry, textbox_changed_cb, LV_EVENT_READY, (void *)(long)HOSTNAME);
}
void menuNTP(const char *title) {
    lv_obj_t * scr = initLeafScr(title, 0);
    lv_obj_t * textentry = create_text_entry(scr, ntp_server, 16, false);
}
