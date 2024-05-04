
#include "OssmUi.h"

// speed and encoder state

volatile int s_speed_percentage = 0;
volatile int s_encoder_position = 0;
String s_mode_label = "STROKE";
String s_message = "Machine Homing";

// KM logo

const uint8_t km_logo[] PROGMEM = {
    0xF8, 0xFF, 0xFF, 0xFF, 0x1F, 0xFE, 0xFF, 0xFF, 0xFF, 0x7F, 0xFE, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x87, 0xFF, 0x3F, 0xF8, 0xFF, 0x87, 0xFF, 0x1F, 0xFC, 0xFF, 0x87, 0xFF, 0x0F, 0xFE, 0xFF, 0x87, 0xFF, 0x07,
    0xFF, 0xFF, 0x87, 0xFF, 0x83, 0xFF, 0xFF, 0x87, 0xFF, 0xC1, 0xFF, 0xFF, 0x87, 0xFF, 0xE0, 0xFF, 0xFF, 0x87, 0x7F,
    0x80, 0xFF, 0xE1, 0x87, 0x3F, 0x80, 0xFF, 0xE1, 0x87, 0x1F, 0x84, 0xFF, 0xE1, 0x87, 0x1F, 0x06, 0xFF, 0xE0, 0x87,
    0x0F, 0x06, 0xFF, 0xE4, 0x87, 0x87, 0x07, 0xFE, 0xE4, 0x87, 0x83, 0x6F, 0x7E, 0xE4, 0x87, 0xC1, 0x67, 0x7E, 0xE6,
    0x07, 0xE0, 0xC7, 0x3C, 0xE6, 0x07, 0xF0, 0xE7, 0x3C, 0xE7, 0x87, 0xE0, 0xC7, 0x38, 0xE7, 0x87, 0xC1, 0xCF, 0x99,
    0xE7, 0x87, 0x83, 0xE7, 0x91, 0xE7, 0x87, 0x07, 0xC7, 0xC3, 0xE7, 0x87, 0x0F, 0xEE, 0xC3, 0xE7, 0x87, 0x1F, 0xC4,
    0xC7, 0xE7, 0x87, 0x1F, 0xE0, 0xE7, 0xE7, 0x87, 0x7F, 0xE0, 0xE7, 0xE7, 0x87, 0xFF, 0xE0, 0xFF, 0xFF, 0x87, 0xFF,
    0xC1, 0xFF, 0xFF, 0x87, 0xFF, 0x81, 0xFF, 0xFF, 0x87, 0xFF, 0x07, 0xFF, 0xFF, 0x87, 0xFF, 0x07, 0xFE, 0xFF, 0x87,
    0xFF, 0x1F, 0xFC, 0xFF, 0x87, 0xFF, 0x1F, 0xF8, 0xFF, 0x87, 0xFF, 0x7F, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
    0xFE, 0xFF, 0xFF, 0xFF, 0x7F, 0xF8, 0xFF, 0xFF, 0xFF, 0x3F,
};

// default activity symbols

const uint8_t s_active_symbol[8] PROGMEM = {B00000000, B00000000, B00011000, B00100100,
                                            B01000010, B01000010, B00100100, B00011000};

const uint8_t s_inactive_symbol[8] PROGMEM = {B00000000, B00000000, B00000000, B00000000,
                                              B00011000, B00011000, B00000000, B00000000};

// default overlays and frames

static void OssmUiOverlaySpeed(OLEDDisplay* display, OLEDDisplayUiState* state)
{
    display->setFont(ArialMT_Plain_10);

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 0, "SPEED");

    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    // int offset = display->getWidth() - display->getStringWidth(s_mode_label);
    display->drawString(display->getWidth(), 0, s_mode_label);

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(64, 50, s_message);
}
static void OssmUiOverlayBooting(OLEDDisplay* display, OLEDDisplayUiState* state)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_10);
    display->drawString(64, 0, " TEST                          ");
    display->drawString(64, 50, s_message);
}
static void OssmUiFrameKMlogo(OLEDDisplay* display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
    // display->fillRect(10,14+(50-int(s_speed_percentage/2)),10,20-int(s_speed_percentage/100));
    display->fillRect(10, 14 + (34 - int(s_speed_percentage / 3)), 10, int(s_speed_percentage / 3));
    display->drawXbm(x + 44, y + 6, 40, 40, km_logo);
    // display->fillRect(106,64-int(s_encoder_position/2),10,64);
    display->fillRect(106, 14 + (34 - int(s_encoder_position / 3)), 10, int(s_encoder_position / 3));
}

const size_t s_overlay_count = 1;
OverlayCallback s_overlays[] = {OssmUiOverlaySpeed};

const size_t s_frame_count = 1;
FrameCallback s_frames[] = {OssmUiFrameKMlogo};

// OssmUi constructor and methods

OssmUi::OssmUi(uint8_t address, int sda, int scl)
    : m_display(address, sda, scl),
      m_ui(&m_display),
      m_address(address),
      m_check_connectivity_interval(250),
      m_connected(false),
      m_last_update_time(0)
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
    // no auto transition
    m_ui.disableAutoTransition();
    // no transition animation
    m_ui.setTimePerTransition(0);
    // Get rid of the indicators on screen
    m_ui.disableAllIndicators();
}

void OssmUi::showBootScreen()
{
    //m_ui.switchToFrame(1);
    // const size_t s_overlay_count = 1;
    // OverlayCallback s_overlays[] = {OssmUiOverlayBooting};
    // const size_t s_frame_count = 1;
    // FrameCallback s_frames[] = {OssmUiFrameKMlogo};
    // m_ui.setFrames(s_frames, s_frame_count);
    // m_ui.setOverlays(s_overlays, s_overlay_count);
}

void OssmUi::SetTargetFps(uint8_t target_fps)
{
    m_ui.setTargetFPS(target_fps);
}

void OssmUi::ResetState()
{
    m_display.end();
    m_display.init();
    m_display.flipScreenVertically();
    // m_ui.switchToFrame(0);
}

void OssmUi::SetFrames(OverlayCallback* overlays, size_t overlays_count, FrameCallback* frames, size_t frames_count)
{
    m_ui.setFrames(frames, frames_count);
    m_ui.setOverlays(overlays, overlays_count);
}

void OssmUi::SetActivitySymbols(const uint8_t* active, const uint8_t* inactive)
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

    // Initialising the UI will init the display too.
    m_ui.init();

    // flip screen
    m_display.flipScreenVertically();
}

void OssmUi::UpdateState(String mode_label, const int speed_percentage, const int encoder_position)
{
    s_mode_label = mode_label;
    s_speed_percentage = speed_percentage;
    s_encoder_position = encoder_position;
}

void OssmUi::UpdateMessage(String message_in)
{
    s_message = message_in;
    m_ui.update();
}

void OssmUi::UpdateScreen()
{
    m_ui.update();

    // periodically check connectivity
    uint32_t now = millis();
    if (now - m_last_update_time > m_check_connectivity_interval)
    {
        m_last_update_time = now;
        Wire.beginTransmission(m_address);
        if (0 == Wire.endTransmission(m_address))
        {
            if (!m_connected)
            {
                ResetState();
            }
            m_connected = true;
        }
        else
        {
            m_connected = false;
        }
    }
}

void OssmUi::UpdateOnly()
{
    m_ui.update();
}
