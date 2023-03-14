#include "lvgl_gui.hpp"

LVGLGui* lvgl_instance;
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  lvgl_instance->flush(disp, area, color_p);
}

LVGLGui::LVGLGui() {
  lvgl_instance = this;
  
  tft = new TFT_eSPI();
  img = new TFT_eSprite(tft);
}

void gui_lvgl_task(void *pvParameter) {  
  while (true) {
    vTaskDelay(5 / portTICK_PERIOD_MS);
    lv_task_handler();
  }
}

void gui_tick_task(void *pvParameter) {
  LVGLGui* gui = (LVGLGui*) pvParameter;
  
  while (true) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gui->tick();
  }
}

void LVGLGui::start() {
  this->tft->init();
  this->tft->setRotation(0);
  this->tft->fillScreen(TFT_RED);

  lv_init();
  lv_disp_draw_buf_init(&this->disp_buf, this->buf_1, this->buf_2, 240 * 10);

  /*Initialize the display*/

  lv_disp_drv_init(&this->disp_drv);
  this->disp_drv.hor_res = 240;
  this->disp_drv.ver_res = 280;
  this->disp_drv.flush_cb = my_disp_flush;
  this->disp_drv.draw_buf = &this->disp_buf;
  lv_disp_drv_register(&this->disp_drv);

  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x003a57), LV_PART_MAIN);

  lv_disp_t *dispp = lv_disp_get_default();
  lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                            true, LV_FONT_DEFAULT);
  lv_disp_set_theme(dispp, theme);

  xTaskCreatePinnedToCore(&gui_lvgl_task, "gui_lvgl_task", 4096*2, this, 0, NULL, 1);
  xTaskCreatePinnedToCore(&gui_tick_task, "gui_tick_task", 4096*2, this, 0, NULL, 1);
}

void LVGLGui::flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  this->tft->startWrite();
  this->tft->setAddrWindow(area->x1, area->y1, w, h);
  this->tft->pushColors(&color_p->full, w * h, true);
  this->tft->endWrite();

  lv_disp_flush_ready(disp);
}
