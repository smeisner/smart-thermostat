#include "../ui.h"

extern lv_obj_t * common_main;
static lv_event_dsc_t *page_change;

lv_obj_t * uiMainPageAbsent()
{
    lv_obj_t * scr = uiMainPagePresent();
    main_page_fade_out(0);
    return scr;
}
