#pragma once

#include "lvgl_gui.hpp"

class WifiFailureScreen : public ScreenInterface {
  public:
    WifiFailureScreen();
    String getName() { return "Wifi: Failed to Connect"; }

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

  friend class LVGLGui;
};