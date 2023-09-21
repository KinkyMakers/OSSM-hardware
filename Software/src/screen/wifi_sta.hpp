#pragma once

#include "lvgl_gui.hpp"

class WifiStaScreen : public ScreenInterface {
  public:
    WifiStaScreen();
    String getName() { return "Wifi: Connecting to AP"; }

  private:
    lv_obj_t *ui_WIFI;
    lv_obj_t *ui_WIFI_Panel1;
    lv_obj_t *ui_WIFI_Arc1;
    lv_obj_t *ui_WIFI_Arc2;
    lv_obj_t *ui_WIFI_Arc3;
    lv_obj_t *ui_WIFI_Arc4;
    lv_obj_t *ui_WIFI_Name;
    lv_obj_t *ui_WIFI_NameTitle;
    lv_obj_t *ui_WIFI_Password;
    lv_obj_t *ui_WIFI_PasswordTitle;
    void tick();

    uint8_t loopCount;
    char connectString[64];

  friend class LVGLGui;
};