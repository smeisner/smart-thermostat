#include <stdio.h>
#include "../ui.h"
#include "menu_internal.h"

static lv_obj_t * scr_Menu;
lv_obj_t * scr_leaf;
static lv_obj_t * menu_m;
static lv_obj_t * main_menu;
static lv_obj_t * ico;

static void menu_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (main_menu == lv_menu_get_cur_main_page(menu_m)) {
            page_event_cb(e);
        } else {
            lv_obj_t * back = lv_menu_get_main_header_back_button(menu_m);
            lv_obj_send_event(back, LV_EVENT_CLICKED, 0);
        }
    } else {
        // change back-button icon
        lv_obj_t * label = lv_obj_get_child(ico, 0);
        lv_obj_t *page = lv_menu_get_cur_main_page(menu_m);
        if (main_menu == page) {
            lv_label_set_text(label, HOME_SYMBOL);
        } else {
            lv_label_set_text(label, BACKMENU_SYMBOL);
            lv_obj_send_event(page, LV_EVENT_VALUE_CHANGED, NULL);
        }
    }
}

static void leaf_cb(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_current_target_obj(e);
    const leaf_func_ptr func = (leaf_func_ptr) lv_event_get_user_data(e);
    lv_obj_t *label = lv_obj_get_child(obj, 0);
    const char *title = lv_label_get_text(label);
    func(title);
}

static void warn_ok_cb(lv_event_t *e)
{
    lv_obj_t *submenu = (lv_obj_t *)lv_event_get_user_data(e);
    lv_menu_set_page(menu_m, submenu);
}

static void popup_cb(lv_event_t *e)
{
    lv_obj_t *popup = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_HIDDEN);
}

// Using lv_menu is not very memory efficient, but our menu isn't too deep
static lv_obj_t * create_menu(menu_t **items, const char *title)
{
    lv_obj_t *menu = lv_menu_page_create(menu_m, NULL);
    lv_menu_set_page_title_static(menu, title);
    lv_obj_set_style_pad_top(menu, 10, 0);
    lv_obj_remove_flag(menu, LV_OBJ_FLAG_EVENT_BUBBLE);
    for (int i = 0; items[i] != NULL; i++) {
        menu_t *item = items[i];
        if (item->type == CALLBACK) {
            lv_obj_add_event_cb(menu, (void(*)(lv_event_t *))item->callback, LV_EVENT_VALUE_CHANGED, NULL);
            continue;
        }
        lv_obj_t * cont = lv_menu_cont_create(menu);
        lv_obj_t * label = lv_label_create(cont);
        lv_label_set_text_static(label, item->label);

        if (item->type == HIER) {
            lv_obj_t * submenu = create_menu(item->menu, item->label);
            lv_menu_set_load_page_event(menu_m, cont, submenu);
        } else if (item->type == WARN) {
            menu_t **childp = item->menu;
            menu_t *child = *childp;
            LV_ASSERT_FORMAT_MSG(child && child->type == WARN, "Child of %s should be type 'WARN'\n", item->label);
            lv_obj_t * submenu = create_menu(child->menu, item->label);
            lv_obj_t * popup = create_popup(scr_Menu, "WARNING", child->label, warn_ok_cb, NULL, submenu);
            lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(cont, popup_cb, LV_EVENT_CLICKED, (void *)popup);
        } else {
            lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(cont, leaf_cb, LV_EVENT_CLICKED, (void *)item->screen);
        }
    }
    return menu;
}

void transition_leaf_to_menu(void)
{
    lv_screen_load_anim(scr_Menu, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, 0);
    lv_obj_send_event(lv_menu_get_cur_main_page(menu_m), LV_EVENT_VALUE_CHANGED, NULL);

}

static void leaf_back_cb(lv_event_t *e)
{
    // Cannot use load_page() because we don't want to reinitialize the menu
    transition_leaf_to_menu();
}

lv_obj_t * initLeafScr(const char *title, int x_offset, void (*back_button_cb)(lv_event_t *))
{
    lv_obj_clean(scr_leaf);
    if (! back_button_cb)
        back_button_cb = leaf_back_cb;
    lv_obj_t * leaf_ico = create_menu_icon(scr_leaf, BACKMENU_SYMBOL, PAGE_NONE);
    lv_obj_add_event_cb(leaf_ico, back_button_cb, LV_EVENT_CLICKED, NULL);
    lv_screen_load_anim(scr_leaf, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, 0);
    lv_obj_t * label = lv_label_create(scr_leaf);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 16, 0);
    lv_label_set_text_static(label, title);
    lv_obj_t * box = create_invisible_box(scr_leaf, LV_HOR_RES - x_offset, LV_VER_RES - 42);
    lv_obj_set_pos(box, x_offset, 42);

    return box;
}

lv_obj_t * uiMenu()
{

    if (! scr_leaf)
        scr_leaf = lv_obj_create(NULL);
    scr_Menu = get_page();
    lv_obj_t * scr = scr_Menu;

	lv_obj_set_style_bg_color(scr,lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_color(scr_leaf,lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_text_color(scr_leaf, lv_color_white(), 0);
    lv_obj_set_scrollbar_mode(scr_leaf, LV_SCROLLBAR_MODE_OFF);


    ico = create_menu_icon(scr, HOME_SYMBOL, PAGE_NONE);
    lv_obj_add_event_cb(ico, menu_cb, LV_EVENT_CLICKED, (void *)PAGE_PRESENT);

    menu_m = lv_menu_create(scr);
    lv_obj_add_event_cb(menu_m, menu_cb, LV_EVENT_VALUE_CHANGED, 0);
    //lv_obj_set_pos(menu_m, 16, 32);
	lv_obj_set_style_bg_color(menu_m, lv_color_black(),LV_PART_MAIN);
    lv_obj_set_style_text_color(menu_m, lv_color_white(), 0);

    // remove back-button from menu (but keep the 'clicked' event so we can manually trigger it)
    lv_obj_t * back_btn = lv_menu_get_main_header_back_button(menu_m);
    lv_obj_clean(back_btn);

    main_menu = create_menu(menuTop, "Main Menu");
    lv_menu_set_page(menu_m, main_menu);

    return scr;
}
