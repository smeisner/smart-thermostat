// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.2
// LVGL version: 8.3.6
// Project name: Thermostat

#include "../ui.h"

void ui_DeviceName_screen_init(void)
{
ui_DeviceName = lv_obj_create(NULL);
lv_obj_clear_flag( ui_DeviceName, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM );    /// Flags
lv_obj_set_scrollbar_mode(ui_DeviceName, LV_SCROLLBAR_MODE_OFF);
lv_obj_set_style_bg_color(ui_DeviceName, lv_color_hex(0xB787E2), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_DeviceName, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_color(ui_DeviceName, lv_color_hex(0xBE5757), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_grad_dir(ui_DeviceName, LV_GRAD_DIR_HOR, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_DevNameKeyboard = lv_keyboard_create(ui_DeviceName);
lv_obj_set_width( ui_DevNameKeyboard, 320);
lv_obj_set_height( ui_DevNameKeyboard, 148);
lv_obj_set_x( ui_DevNameKeyboard, 0 );
lv_obj_set_y( ui_DevNameKeyboard, 46 );
lv_obj_set_align( ui_DevNameKeyboard, LV_ALIGN_LEFT_MID );
lv_obj_set_style_bg_color(ui_DevNameKeyboard, lv_color_hex(0xBD6994), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_DevNameKeyboard, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_DeviceHostname = lv_textarea_create(ui_DeviceName);
lv_obj_set_width( ui_DeviceHostname, 205);
lv_obj_set_height( ui_DeviceHostname, LV_SIZE_CONTENT);   /// 20
lv_obj_set_x( ui_DeviceHostname, -50 );
lv_obj_set_y( ui_DeviceHostname, -97 );
lv_obj_set_align( ui_DeviceHostname, LV_ALIGN_CENTER );
lv_textarea_set_placeholder_text(ui_DeviceHostname,"Device Name");
lv_textarea_set_one_line(ui_DeviceHostname,true);
lv_obj_set_style_text_font(ui_DeviceHostname, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);



ui_DevNameSaveBtn = lv_btn_create(ui_DeviceName);
lv_obj_set_width( ui_DevNameSaveBtn, 100);
lv_obj_set_height( ui_DevNameSaveBtn, 36);
lv_obj_set_x( ui_DevNameSaveBtn, -1 );
lv_obj_set_y( ui_DevNameSaveBtn, -56 );
lv_obj_set_align( ui_DevNameSaveBtn, LV_ALIGN_RIGHT_MID );
lv_obj_add_flag( ui_DevNameSaveBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS );   /// Flags
lv_obj_clear_flag( ui_DevNameSaveBtn, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_bg_color(ui_DevNameSaveBtn, lv_color_hex(0x2095F6), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_DevNameSaveBtn, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_color(ui_DevNameSaveBtn, lv_color_hex(0x2620F6), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_grad_dir(ui_DevNameSaveBtn, LV_GRAD_DIR_VER, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_DevNameSaveLabel = lv_label_create(ui_DeviceName);
lv_obj_set_width( ui_DevNameSaveLabel, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_DevNameSaveLabel, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_DevNameSaveLabel, 109 );
lv_obj_set_y( ui_DevNameSaveLabel, -56 );
lv_obj_set_align( ui_DevNameSaveLabel, LV_ALIGN_CENTER );
lv_label_set_text(ui_DevNameSaveLabel,"Save");
lv_obj_set_style_text_color(ui_DevNameSaveLabel, lv_color_hex(0xFFE300), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_DevNameSaveLabel, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_DevNameCancelBtn = lv_btn_create(ui_DeviceName);
lv_obj_set_width( ui_DevNameCancelBtn, 100);
lv_obj_set_height( ui_DevNameCancelBtn, 36);
lv_obj_set_x( ui_DevNameCancelBtn, -1 );
lv_obj_set_y( ui_DevNameCancelBtn, -96 );
lv_obj_set_align( ui_DevNameCancelBtn, LV_ALIGN_RIGHT_MID );
lv_obj_add_flag( ui_DevNameCancelBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS );   /// Flags
lv_obj_clear_flag( ui_DevNameCancelBtn, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_bg_color(ui_DevNameCancelBtn, lv_color_hex(0x2095F6), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_DevNameCancelBtn, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_color(ui_DevNameCancelBtn, lv_color_hex(0x2620F6), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_grad_dir(ui_DevNameCancelBtn, LV_GRAD_DIR_VER, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_DevNameCancelLabel = lv_label_create(ui_DeviceName);
lv_obj_set_width( ui_DevNameCancelLabel, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_DevNameCancelLabel, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_DevNameCancelLabel, 109 );
lv_obj_set_y( ui_DevNameCancelLabel, -96 );
lv_obj_set_align( ui_DevNameCancelLabel, LV_ALIGN_CENTER );
lv_label_set_text(ui_DevNameCancelLabel,"Cancel");
lv_obj_set_style_text_color(ui_DevNameCancelLabel, lv_color_hex(0xFFE300), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_DevNameCancelLabel, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_keyboard_set_textarea(ui_DevNameKeyboard,ui_DeviceHostname);
lv_obj_add_event_cb(ui_DeviceHostname, ui_event_DeviceHostname, LV_EVENT_ALL, NULL);
lv_obj_add_event_cb(ui_DevNameSaveBtn, ui_event_DevNameSaveBtn, LV_EVENT_ALL, NULL);
lv_obj_add_event_cb(ui_DevNameCancelBtn, ui_event_DevNameCancelBtn, LV_EVENT_ALL, NULL);

}