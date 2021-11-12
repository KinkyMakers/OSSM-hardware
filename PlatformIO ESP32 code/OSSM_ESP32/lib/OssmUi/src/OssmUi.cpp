
#include "OssmUi.h"

// speed and encoder state

volatile int s_speed_percentage = 0;
volatile int s_encoder_position = 0;

// default activity symbols

const uint8_t s_active_symbol[8] PROGMEM = {
    B00000000,
    B00000000,
    B00011000,
    B00100100,
    B01000010,
    B01000010,
    B00100100,
    B00011000};

const uint8_t s_inactive_symbol[8] PROGMEM = {
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00011000,
    B00011000,
    B00000000,
    B00000000};

// default overlays and frames

static void OssmUiOverlaySpeed(OLEDDisplay* display, OLEDDisplayUiState* state)
{
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(0, 0, "SPEED " + String(s_speed_percentage) + "%");
}

static void OssmUiFrameStrokePercentage(OLEDDisplay* display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(10 + x, 10 + y, "Stroke Percentage " + String(s_encoder_position));
    // display->drawString(10 + x, 20 + y, "Limit Switch " + String( digitalRead(LIMIT_SWITCH_PIN) ) );
}

static void OssmUiFramePosition(OLEDDisplay* display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(10 + x, 10 + y, "Position");
}

const size_t s_overlay_count = 1;
OverlayCallback s_overlays[] = {OssmUiOverlaySpeed};

const size_t s_frame_count = 2;
FrameCallback s_frames[] = {OssmUiFrameStrokePercentage, OssmUiFramePosition};

// OssmUi constructor and methods

OssmUi::OssmUi(uint8_t address, int sda, int scl)
    : m_display(address, sda, scl),
      m_ui(&m_display)
{
    // fps
    m_ui.setTargetFPS(20);
    // activity indicators
    m_ui.setActiveSymbol(s_active_symbol);
    m_ui.setInactiveSymbol(s_inactive_symbol);
    // frames
    m_ui.setFrames(s_frames, s_frame_count);
    // overlays
    m_ui.setOverlays(s_overlays, s_overlay_count);
}

void OssmUi::SetTargetFps(uint8_t target_fps)
{
    m_ui.setTargetFPS(target_fps);
}

void OssmUi::SetFrames(OverlayCallback* overlays, size_t overlays_count,
                       FrameCallback* frames, size_t frames_count)
{
    m_ui.setFrames(frames, frames_count);
    m_ui.setOverlays(overlays, overlays_count);
}

void OssmUi::SetActivitySymbols(const uint8_t *active, const uint8_t *inactive)
{
    m_ui.setActiveSymbol(active);
    m_ui.setInactiveSymbol(inactive);
}

void OssmUi::Setup()
{
    // OLED SETUP

    // You can change this to
    // TOP, LEFT, BOTTOM, RIGHT
    m_ui.setIndicatorPosition(LEFT);

    // Defines where the first frame is located in the bar.
    m_ui.setIndicatorDirection(LEFT_RIGHT);

    // You can change the transition that is used
    // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
    // m_ui.setFrameAnimation(SLIDE_LEFT);

    // Initialising the UI will init the display too.
    m_ui.init();
    m_ui.disableAutoTransition();

    m_display.flipScreenVertically();
}

void OssmUi::UpdateState(const int speed_percentage, const int encoder_position)
{
    s_speed_percentage = speed_percentage;
    s_encoder_position = encoder_position;
}
