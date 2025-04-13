#include "ui.h"

// iterate over all children of parent object and highlight
// the taget one.  All others get unhighlighted
// Only works on immediate children
void select_object(lv_obj_t * parent, lv_obj_t *obj)
{
    int idx = 0;
    lv_obj_t * child = lv_obj_get_child(parent, idx++);
    while(child) {
        /*Do something with "child" */
        if (child == obj) {
            lv_obj_set_style_border_color(child, lv_color_hex(0xffa500), 0);
        } else {
            lv_obj_set_style_border_color(child, lv_color_hex(0xffffff), 0);
        }
        child = lv_obj_get_child(parent, idx++);
    }
}

// Create an invisible box with no padding, and no scrolling
lv_obj_t * create_invisible_box(lv_obj_t *scr, int width, int height)
{
    lv_obj_t * box = lv_obj_create(scr);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_bg_opa(box, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(box, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_text_color(box, lv_color_white(), 0);
    return box;
}

// Create an icon representing a HVAC state in a specified color
lv_obj_t *create_hvac_icon(lv_obj_t *scr, const char *icon, uint32_t color)
{
    lv_obj_t * l_icon = lv_label_create(scr);
    lv_obj_set_style_text_align(l_icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l_icon, icon);
    lv_obj_set_style_text_font(l_icon, &FONT_28, 0);
    lv_obj_set_style_text_color(l_icon, lv_color_hex(color), 0);
    return l_icon;
}

// Create a menu icon in the upper right corner of the screen which
// will transfer to the specified target page when clicked
lv_obj_t * create_menu_icon(lv_obj_t *scr, const char *icon, lv_obj_t ** page)
{
    // Menu hit box
    lv_obj_t * menu_box = create_invisible_box(scr, 50, 50);
    lv_obj_set_pos(menu_box, 190, 0);

    // Menu
    lv_obj_t * l_menu = lv_label_create(menu_box);
    lv_obj_set_style_text_align(l_menu, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l_menu, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_label_set_text(l_menu, icon);
    lv_obj_set_style_text_font(l_menu, &FONT_22, 0);

    lv_obj_add_flag(menu_box, LV_OBJ_FLAG_CLICKABLE);
    if (page) {
        lv_obj_add_event_cb(menu_box, page_event_cb, LV_EVENT_ALL, (void *)page);
    }
    lv_obj_set_style_bg_opa(menu_box, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_border_width(menu_box, LV_STATE_DEFAULT, 0);
    return menu_box;
}

// Create an empty button of the specified size and alignment.
// it will have a border, no shadow, and transparent interior
lv_obj_t * button(lv_obj_t *scr, int width, int height, int x, int y, lv_align_t align)
{
   lv_obj_t * b_off = lv_button_create(scr);
   lv_obj_set_size(b_off, width, height);
   lv_obj_align(b_off, align, x, y);
   lv_obj_set_style_bg_opa(b_off, LV_STATE_DEFAULT, LV_OPA_TRANSP);
   lv_obj_set_style_border_color(b_off, lv_color_white(), 0);
   lv_obj_set_style_border_width(b_off, 2, 0);
   lv_obj_set_style_shadow_width(b_off, 0, 0);
   return b_off;
}

// Create an empty button containing a specified label
lv_obj_t * label_button(lv_obj_t *scr, int width, int height, int x, int y, const char *label, lv_align_t align)
{
   lv_obj_t *b_but = button(scr, width, height, x, y, align);
   lv_obj_t * l_label = lv_label_create(b_but);
   lv_label_set_text(l_label, label);
   lv_obj_center(l_label);
   return b_but;
}

static void switch_cb(lv_event_t *e)
{
    bool * var = (bool *)lv_event_get_user_data(e);
    lv_obj_t * obj = lv_event_get_current_target_obj(e);
    *var = lv_obj_has_state(obj, LV_STATE_CHECKED);
}

lv_obj_t * create_switch(lv_obj_t *scr, const char *label, bool *var)
{
    lv_obj_t * box = create_invisible_box(scr, 240-32, LV_SIZE_CONTENT);
    lv_obj_t *lbl = lv_label_create(box);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text_static(lbl, label);

    lv_obj_t * sw = lv_switch_create(box);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(sw, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(sw, switch_cb, LV_EVENT_VALUE_CHANGED, (void *)var);
    if (*var) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    return box;
}
static void popup_cb(lv_event_t *e)
{
    lv_obj_t * mbox = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_add_flag(mbox, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t * create_popup(lv_obj_t *scr, const char *title, const char *message,
    void (*ok_cb)(lv_event_t *), void (*cancel_cb)(lv_event_t *), void *userData)
{
    lv_obj_t * mbox = lv_msgbox_create(scr);
    lv_obj_set_width(mbox, 240-32);
    lv_msgbox_add_title(mbox, title);
    lv_msgbox_add_text(mbox, message);
    // lv_msgbox_add_close_button(mbox);

    lv_obj_t * btn;
    btn = lv_msgbox_add_footer_button(mbox, "OK");
    lv_obj_add_event_cb(btn, popup_cb, LV_EVENT_CLICKED, (void *)mbox);
    lv_obj_add_event_cb(btn, ok_cb, LV_EVENT_CLICKED, userData);
    btn = lv_msgbox_add_footer_button(mbox, "Cancel");
    lv_obj_add_event_cb(btn, popup_cb, LV_EVENT_CLICKED, (void *)mbox);
    if (cancel_cb) {
        lv_obj_add_event_cb(btn, cancel_cb, LV_EVENT_CLICKED, userData);
    }

    return mbox;
}

static void radio_event_handler(lv_event_t *e)
{
    int *var = (int *)lv_event_get_user_data(e);
    lv_obj_t * cont = (lv_obj_t *)lv_event_get_current_target(e);
    lv_obj_t * act_cb = lv_event_get_target_obj(e);
    lv_obj_t * old_cb = lv_obj_get_child(cont, *var);
    if(act_cb == cont) return;
    lv_obj_remove_state(old_cb, LV_STATE_CHECKED);   /*Uncheck the previous radio button*/
    lv_obj_add_state(act_cb, LV_STATE_CHECKED);     /*Check the current radio button*/

    *var = lv_obj_get_index(act_cb);
}

lv_obj_t * create_radio(lv_obj_t *scr, int *value, int count, const char **items)
{
    lv_obj_t * box = create_invisible_box(scr, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(box, radio_event_handler, LV_EVENT_CLICKED, (void *)value);

    for (int i = 0; i < count; i++) {
        lv_obj_t * obj = lv_checkbox_create(box);
        lv_checkbox_set_text(obj, items[i]);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_set_style_bg_image_src(obj, NULL, LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (*value == i) {
            lv_obj_add_state(obj, LV_STATE_CHECKED);
        }
    }
    return box;
}

static void text_entry_cb(lv_event_t * e)
{
    char *var = (char *)lv_event_get_user_data(e);
    lv_obj_t * obj = lv_event_get_target_obj(e);
    lv_strcpy(var, lv_textarea_get_text(obj));
}

// NOTE: scr should have no X offset or the keyboard won't fit
lv_obj_t * create_text_entry(lv_obj_t * scr, char *str, int max_len, bool is_pass)
{
        /*Create a label and position it above the text box*/
        //lv_obj_t * pwd_label = lv_label_create(scr);
        //lv_label_set_text(pwd_label, "Wifi Password:");
        //lv_obj_set_pos(pwd_label, 16, 32);

        lv_obj_t * box = create_invisible_box(scr, LV_HOR_RES, LV_VER_RES-42);
        /*Create the text box*/
        lv_obj_t * textarea = lv_textarea_create(box);
        lv_textarea_set_text(textarea, str);
        lv_textarea_set_max_length(textarea, max_len);
        lv_textarea_set_password_mode(textarea, is_pass);
        lv_textarea_set_one_line(textarea, true);
        lv_obj_set_width(textarea, 240-32);
        lv_obj_align(textarea, LV_ALIGN_TOP_LEFT, 16, 0);
        lv_obj_add_event_cb(textarea, text_entry_cb, LV_EVENT_VALUE_CHANGED, (void *)str);

        /*Create a keyboard*/
        lv_obj_t * kb = lv_keyboard_create(box);
        lv_obj_set_size(kb,  LV_HOR_RES, LV_VER_RES / 2);
        lv_obj_set_style_bg_color(kb,lv_color_black(),LV_PART_MAIN);

        lv_keyboard_set_textarea(kb, textarea); /*Focus it on one of the text areas to start*/

        return box;
}
