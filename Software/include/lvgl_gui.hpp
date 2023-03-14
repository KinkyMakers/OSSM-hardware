#ifndef LVGL_GUI_H
#define LVGL_GUI_H

#include "esp_log.h"
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <map>

class ScreenInterface {
  public:
    bool getIsActive() { return isActive; }
    virtual String getName() { return "generic"; }

  protected:
    virtual void tick();
    bool isActive = false;

    lv_obj_t *ui_root;

  friend class LVGLGui;
};

class LVGLGui {
  public:
    LVGLGui();

    void start();
    void flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);

    void activate(ScreenInterface* screen) {
      if (screen == NULL) {
        return; // TODO - Log a warning here
      }

      ESP_LOGI("gui", "Activating new screen '%s'", screen->getName());

      if (this->activeScreen != NULL) {
        this->activeScreen->isActive = false;
      }

      this->activeScreen = screen;
      lv_disp_load_scr(this->activeScreen->ui_root);
      this->activeScreen->isActive = true;
      forceInvalidateCounter = 0; // Force a refresh next tick to make sure UI is rendering right after activation
    }

    void tick() {
      if (this->activeScreen == NULL) {
        return;
      }

      this->activeScreen->tick();

      if (forceInvalidateCounter == 0) {
        lv_obj_invalidate(this->activeScreen->ui_root);
      }
      // TODO - Get a delta time and force a refresh every like 5-10 seconds in case of wierd UI glitches???
      forceInvalidateCounter = (forceInvalidateCounter + 1) % 10;
    }

  private:
    uint16_t forceInvalidateCounter = 0;

    TFT_eSPI* tft;
    TFT_eSprite* img;

    lv_disp_draw_buf_t disp_buf;
    
    lv_color_t buf_1[240 * 10];
    lv_color_t buf_2[240 * 10];

    lv_disp_drv_t disp_drv;
    lv_indev_drv_t indev_drv;

    ScreenInterface* activeScreen = NULL;
};

LV_IMG_DECLARE(canfuck_small);

#endif