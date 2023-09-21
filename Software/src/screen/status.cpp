#include "screen/status.hpp"
#include <WiFi.h>
#include <ESPConnect.h>

StatusScreen::StatusScreen() {
  ui_root = ui_STATUS = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_STATUS, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

  ui_STATUS_Background = lv_obj_create(ui_STATUS);
  lv_obj_set_width(ui_STATUS_Background, 240);
  lv_obj_set_height(ui_STATUS_Background, 280);
  lv_obj_set_align(ui_STATUS_Background, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_STATUS_Background, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
  lv_obj_set_style_radius(ui_STATUS_Background, 40, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_STATUS_Background, lv_color_hex(0x23253C), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_STATUS_Background, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui_STATUS_Background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_STATUS_TopBar = lv_obj_create(ui_STATUS);
  lv_obj_set_width(ui_STATUS_TopBar, 240);
  lv_obj_set_height(ui_STATUS_TopBar, 40);
  lv_obj_set_align(ui_STATUS_TopBar, LV_ALIGN_TOP_MID);
  lv_obj_clear_flag(ui_STATUS_TopBar, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
  lv_obj_set_style_radius(ui_STATUS_TopBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_STATUS_TopBar, lv_color_hex(0xD3D3D3), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_STATUS_TopBar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_STATUS_TopBarTitle = lv_textarea_create(ui_STATUS);
  lv_obj_set_width(ui_STATUS_TopBarTitle, 240);
  lv_obj_set_height(ui_STATUS_TopBarTitle, 40);
  lv_obj_set_align(ui_STATUS_TopBarTitle, LV_ALIGN_TOP_MID);
  lv_textarea_set_text(ui_STATUS_TopBarTitle, LV_SYMBOL_WIFI "Not Connected");
  lv_obj_set_style_text_color(ui_STATUS_TopBarTitle, lv_color_hex(0x202439), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_STATUS_TopBarTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(ui_STATUS_TopBarTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_STATUS_TopBarTitle, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_STATUS_TopBarTitle, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_STATUS_TopBarTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(ui_STATUS_TopBarTitle, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_opa(ui_STATUS_TopBarTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_STATUS_VirtualMotor = lv_bar_create(ui_STATUS);
  lv_obj_set_width(ui_STATUS_VirtualMotor, 200);
  lv_obj_set_height(ui_STATUS_VirtualMotor, 10);
  lv_obj_set_align(ui_STATUS_VirtualMotor, LV_ALIGN_TOP_MID);
  lv_bar_set_value(ui_STATUS_VirtualMotor, 80, LV_ANIM_OFF);
  lv_bar_set_range(ui_STATUS_VirtualMotor, 0, 100);
  lv_obj_set_x(ui_STATUS_VirtualMotor, 0);
  lv_obj_set_y(ui_STATUS_VirtualMotor, 90);
}

void StatusScreen::tick() {
  lv_bar_set_value(ui_STATUS_VirtualMotor, loopCount % 100, LV_ANIM_OFF);

  if (loopCount % 30 == 0) {
    if (showWifi) {
      strcpy(topBarTitle, "");
      strcat(topBarTitle, LV_SYMBOL_WIFI " ");
      strcat(topBarTitle, WiFi.localIP().toString().c_str());
      lv_textarea_set_text(ui_STATUS_TopBarTitle, topBarTitle);
      showWifi = false;
    } else {
      strcpy(topBarTitle, "");
      strcat(topBarTitle, LV_SYMBOL_WIFI " ");
      strcat(topBarTitle, ESPConnect.getSSID().c_str());
      lv_textarea_set_text(ui_STATUS_TopBarTitle, topBarTitle);
      showWifi = true;
    }
    lv_refr_now(NULL);
  }

  
  loopCount = (loopCount + 1) % 1000;
}