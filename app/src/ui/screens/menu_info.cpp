#include <stdio.h>
#include <stdlib.h>
#include "thermostat.hpp"
#include "../ui.h"
#include "version.h"
#include "menu_internal.h"

void menuInfo(const char *title)
{
    lv_obj_t *scr = initLeafScr(title);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *label;

    label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "Name: %s", OperatingParameters.FriendlyName);

    label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "Host: %s", OperatingParameters.DeviceName);

    label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "Wifi: %s", OperatingParameters.wifiConnected ? "Connected" : "Disconnected");

    label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "Mqtt: %s", OperatingParameters.MqttConnected ? "Connected" : "Disconnected");

    label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "MAC: %s", OperatingParameters.mac);

    label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "Version: %s", VERSION_STRING);

    label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "Build: %s", VERSION_BUILD_DATE_TIME);

    label = lv_label_create(scr);
    lv_label_set_text_static(label, VERSION_COPYRIGHT);
}
