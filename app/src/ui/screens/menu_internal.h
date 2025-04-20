#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum menu_type {
    HIER,
    WARN,
    LEAF,
    CALLBACK,
};

struct menu_s {
    enum menu_type type;
    union {
       const char *label;
       void* var;
       void (*callback)(void *);
    };
    union {
        const struct menu_s **menu;
        void(*screen)(const char *title);
        const char **str_list;
    };
};
#define MENU_LIST(...) (const struct menu_s *[]){ __VA_ARGS__ __VA_OPT__(,) NULL }
#define MENU_HIER(_lbl, ...) &(const struct menu_s){ .type = HIER, .label = (_lbl), .menu = MENU_LIST(__VA_ARGS__) }
#define MENU_LEAF(_lbl, ...) &(const struct menu_s){ .type = LEAF, .label = (_lbl), .screen = __VA_ARGS__ }
#define MENU_CALLBACK(_cb) &(const struct menu_s){ .type = CALLBACK, .callback = (_cb) }
#define MENU_WARN(_lbl, _msg, ...) &(const struct menu_s){ \
    .type = WARN, \
    .label = (_lbl), \
    .menu = (const struct menu_s *[]) { \
        &(const struct menu_s){ \
            .type = WARN, \
            .label = (_msg), \
            .menu = MENU_LIST(__VA_ARGS__) \
        } \
     }}

typedef const struct menu_s menu_t;
typedef void(*leaf_func_ptr)(const char *);

extern menu_t *menuTop[];

void menuInfo(const char *title);
void menuPassword(const char *title);
void menuSSID(const char *title);
void menuHostname(const char *title);
void menuNTP(const char *title);
void menuMQTTEnable(const char *title);
void menuMQTTHostname(const char *title);
void menuMQTTUsername(const char *title);
void menuMQTTPassword(const char *title);
void enableMQTTMenu(void *e);
void menuMatter(const char *title);
void menuTempUnits(const char *title);
void menuClockFormat(const char *title);
void menuBrightness(const char *title);
void menuDim(const char *title);
void menuBeep(const char *title);
void menuTimezone(const char *title);
void menuCalibrateScreen(const char *title);
void menuReboot(const char *title);
void menuHVACStages(const char *title);
void menuHVACRV(const char *title);
void menuHVACCool(const char *title);
void menuHVACFan(const char *title);
void menuHVACAuxHeat(const char *title);
void menuFactoryReset(const char *title);


#ifdef __cplusplus
#include "lvgl.h"
extern lv_obj_t * scr_leaf;
extern lv_obj_t * initLeafScr(const char *title, int x_offset=16, void (*back_button_cb)(lv_event_t *)=NULL);
extern void transition_leaf_to_menu(void);
} // extern "C"
#endif
