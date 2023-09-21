#pragma once

#include "lvgl_gui.hpp"

class BootScreen : public ScreenInterface {
  public:
    BootScreen();
    String getName() { return "CANfuck Boot"; }

  private:
    lv_obj_t *ui_BOOT;
    lv_obj_t *ui_BOOT_Panel1;
    lv_obj_t *ui_BOOT_TextArea1;
    lv_obj_t *ui_BOOT_Image5;
    lv_obj_t *ui_BOOT_Name;
    lv_obj_t *ui_BOOT_NameTitle;
    lv_obj_t *ui_BOOT_Status;
    lv_obj_t *ui_BOOT_StatusTitle;

    void tick();

  friend class LVGLGui;
};