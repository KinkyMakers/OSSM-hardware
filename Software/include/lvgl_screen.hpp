#pragma once

#include "config.h"
#if LVGL_AVAILABLE == 1

#include <lvgl.h>

class LVGLGui;
class ScreenInterface {
  public:
    ScreenInterface() { };

    bool getIsActive() { return isActive; }
    virtual String getName() { return "generic"; }

  protected:
    virtual void tick();
    bool isActive = false;

    lv_obj_t *ui_root;

  friend class LVGLGui;
};
#endif