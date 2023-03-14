
#ifndef OSSM_UI_H
#define OSSM_UI_H

#include <Arduino.h>
#include <Wire.h>

#include "OLEDDisplayUi.h"
#include "SSD1306Wire.h"

class OssmUi
{
   public:
    OssmUi(uint8_t address, int sda, int scl);

    void Setup();

    /**
     * @brief Set the FPS
     *
     * The ESP is capable of rendering 60fps in 80Mhz mode
     * but that won't give you much time for anything else
     * run it in 160Mhz mode or just set it to 30 fps
     *
     * @param target_fps
     */
    void SetTargetFps(uint8_t target_fps);

    void ResetState();

    void SetFrames(OverlayCallback *overlays, size_t overlays_count, FrameCallback *frames, size_t frames_count);

    void SetActivitySymbols(const uint8_t *active, const uint8_t *inactive);

    void UpdateState(String mode_label, const int speed_percentage, const int encoder_position);

    void UpdateMessage(String message_in);

    void showBootScreen();

    void ShowFrame(uint8_t frame)
    {
        m_ui.switchToFrame(frame);
    }

    void NextFrame()
    {
        m_ui.nextFrame();
    }

    void UpdateScreen();

    void UpdateOnly();

    bool DisplayIsConnected() const
    {
        return m_connected;
    }

   private:

    SSD1306Wire m_display;
    OLEDDisplayUi m_ui;
    uint8_t m_address;
    uint32_t m_check_connectivity_interval;
    bool m_connected;
    uint32_t m_last_update_time;

};

#endif
