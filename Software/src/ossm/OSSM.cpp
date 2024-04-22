#include "OSSM.h"

#include "constants/Images.h"
#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"
#include "services/encoder.h"
#include "services/stepper.h"

namespace sml = boost::sml;
using namespace sml;

// Now we can define the OSSM constructor since OSSMStateMachine::operator() is
// fully defined
OSSM::OSSM(U8G2_SSD1306_128X64_NONAME_F_HW_I2C &display,
           AiEsp32RotaryEncoder &encoder)
    : display(display),
      encoder(encoder),
      sm(std::make_unique<
          sml::sm<OSSMStateMachine, sml::thread_safe<ESP32RecursiveMutex>,
                  sml::logger<StateLogger>>>(logger, *this)) {
    initStepper(stepper);

    // All initializations are done, so start the state machine.
    sm->process_event(Done{});
}

/**
 * This task will write the word "OSSM" to the screen
 * then briefly show the RD logo.
 * and then end on the Kinky Makers logo.
 *
 * The Kinky Makers logo will stay on the screen until the next state is ready
 * :).
 * @param pvParameters
 */
void OSSM::drawHelloTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;

    int frameIdx = 0;
    const int nFrames = 8;

    int startX = 24;
    int offsetY = 12;

    // Bounce the Y position from 0 to 32, up to 24 and down to 32
    std::array<int, 8> framesY = {6, 12, 24, 48, 44, 42, 44, 48};
    std::array<int, 4> heights = {0, 0, 0, 0};
    int letterSpacing = 20;

    while (frameIdx < nFrames + 9) {
        if (frameIdx < nFrames) {
            heights[0] = framesY[frameIdx] - offsetY;
        }
        if (frameIdx - 3 > 0 && frameIdx - 3 < nFrames) {
            heights[1] = framesY[frameIdx - 3] - offsetY;
        }
        if (frameIdx - 6 > 0 && frameIdx - 6 < nFrames) {
            heights[2] = framesY[frameIdx - 6] - offsetY;
        }
        if (frameIdx - 9 > 0 && frameIdx - 9 < nFrames) {
            heights[3] = framesY[frameIdx - 9] - offsetY;
        }
        // increment the frame index
        frameIdx++;

        ossm->display.clearBuffer();
        ossm->display.setFont(u8g2_font_maniac_tf);
        ossm->display.drawUTF8(startX, heights[0], "O");
        ossm->display.drawUTF8(startX + letterSpacing, heights[1], "S");
        ossm->display.drawUTF8(startX + letterSpacing * 2, heights[2], "S");
        ossm->display.drawUTF8(startX + letterSpacing * 3, heights[3], "M");
        ossm->display.sendBuffer();
        // Saying hi to the watchdog :).
        vTaskDelay(1);
    };

    // Delay for a second, then show the RDLogo.
    vTaskDelay(1500);
    ossm->display.clearBuffer();
    drawStr::title("Research and Desire");
    ossm->display.drawXBMP(35, 14, 57, 50, Images::RDLogo);
    ossm->display.sendBuffer();

    ossm->wm.setConfigPortalTimeout(1);
    ossm->wm.setConnectRetries(0);
    ossm->wm.setConnectTimeout(1);
    ossm->wm.setDisableConfigPortal(true);
    if (!ossm->wm.autoConnect()) {
        ESP_LOGD("OSSM::drawHelloTask", "Failed to connect to WiFi");
    }
    vTaskDelay(500);

    ossm->display.clearBuffer();
    drawStr::title("Kinky Makers");
    ossm->display.drawXBMP(40, 14, 50, 50, Images::KMLogo);
    ossm->display.sendBuffer();

    vTaskDelay(1000);

    ossm->display.clearBuffer();
    drawStr::title(UserConfig::language.MeasuringStroke);
    ossm->display.drawXBMP(40, 14, 50, 50, Images::KMLogo);
    ossm->display.sendBuffer();

    // delete the task
    vTaskDelete(nullptr);
}

void OSSM::drawHello() {
    xTaskCreatePinnedToCore(drawHelloTask, "drawHello", 10000, this, 1,
                            &displayTask, 0);
}

void OSSM::drawError() {
    // Throw the e-break on the stepper
    try {
        stepper.emergencyStop();
    } catch (const std::exception &e) {
        ESP_LOGD("OSSM::drawError", "Caught exception: %s", e.what());
    }

    display.clearBuffer();
    drawStr::title(UserConfig::language.Error);
    drawStr::multiLine(0, 20, errorMessage);
    display.sendBuffer();
}
