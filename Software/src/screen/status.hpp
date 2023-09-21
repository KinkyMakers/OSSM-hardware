#pragma once

#include "lvgl_gui.hpp"

class StatusScreen : public ScreenInterface {
  public:
    StatusScreen();
    String getName() { return "Status"; }

  private:
    lv_obj_t *ui_STATUS;
    lv_obj_t *ui_STATUS_Background;
    lv_obj_t *ui_STATUS_TopBar;
    lv_obj_t *ui_STATUS_TopBarTitle;
    lv_obj_t *ui_STATUS_VirtualMotor;

    uint16_t loopCount = 0;
    bool showWifi = false;
    char topBarTitle[64];

    void tick();

  friend class LVGLGui;
};