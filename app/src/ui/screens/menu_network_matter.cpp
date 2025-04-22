#ifdef MATTER_ENABLED

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "thermostat.hpp"
#include "../ui.h"
#include "menu_internal.h"

static bool enabled;

static void update_matter_enabled(lv_obj_t *scr)
{
    lv_obj_t * label = lv_obj_get_child_by_type(scr, 0, &lv_label_class);
    lv_obj_t * qr = lv_obj_get_child_by_type(scr, 0, &lv_qrcode_class);
    if (enabled) {
        const char * data = "MATTER NOT YET ENABLED";
        if (lv_strlen(OperatingParameters.MatterQR) > 0)
          lv_qrcode_update(qr, OperatingParameters.MatterQR, lv_strlen(OperatingParameters.MatterQR));
        else
          lv_qrcode_update(qr, data, lv_strlen(data));
        lv_label_set_text(label, OperatingParameters.MatterPairingCode);

        lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 75);
        lv_obj_align_to(label, qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
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
    updateMatterEnabled(enabled);
    update_matter_enabled(scr);
}

void menuMatter(const char *title) {
    lv_obj_t *scr = initLeafScr(title);
    enabled = OperatingParameters.MatterEnabled;

    lv_obj_t * enable = create_switch(scr, "Enable", &enabled);
    lv_obj_add_event_cb(enable, enable_matter_cb, LV_EVENT_VALUE_CHANGED, (void *)scr);

    lv_obj_t * qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(qr, 150);
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_set_dark_color(qr, lv_color_black());

    lv_obj_t * manual = lv_label_create(scr);

    update_matter_enabled(scr);
}

#endif //MATTER_ENABLED