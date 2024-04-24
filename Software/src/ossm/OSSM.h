#ifndef OSSM_SOFTWARE_OSSM_H
#define OSSM_SOFTWARE_OSSM_H

#include <memory>

#include "AiEsp32RotaryEncoder.h"
#include "ESP_FlexyStepper.h"
#include "U8g2lib.h"
#include "WiFiManager.h"
#include "boost/sml.hpp"
#include "constants/Menu.h"
#include "services/tasks.h"
#include "state/actions.h"
#include "state/events.h"
#include "state/guards.h"
#include "state/ossmi.h"
#include "utils/RecusiveMutex.h"
#include "utils/StateLogger.h"
#include "utils/update.h"

namespace sml = boost::sml;

class OSSM : public OSSMI {
  private:
    /**
     * ///////////////////////////////////////////
     * ////
     * ////  Private Objects and Services
     * ////
     * ///////////////////////////////////////////
     */
    //    AiEsp32RotaryEncoder &encoder;

    /**
     * ///////////////////////////////////////////
     * ////
     * ////  Private Variables and Flags
     * ////
     * ///////////////////////////////////////////
     */
    // Calibration Variables
    float currentSensorOffset = 0;
    float measuredStrokeMm = 0;

    // Homing Variables
    bool isForward = true;

    Menu menuOption = Menu::SimplePenetration;
    String errorMessage = "";

    // Session Variables
    float speedPercentage = 0;
    long strokePercentage = 0;

    unsigned long sessionStartTime = 0;
    int sessionStrokeCount = 0;
    double sessionDistanceMeters = 0;

    /**
     * ///////////////////////////////////////////
     * ////
     * ////  Private Functions
     * ////
     * ///////////////////////////////////////////
     */
    void clearHoming();

    void startHoming();

    void startSimplePenetration();

    bool isStrokeTooShort() override;
    Menu getMenuOption() override { return menuOption; }

    void drawError();

    void drawHello();

    void drawHelp();

    void drawWiFi();

    void drawMenu();

    void drawPlayControls();

    void drawUpdate();

    void drawNoUpdate();

    void drawUpdating();

    /**
     * ///////////////////////////////////////////
     * ////
     * ////  Static Functions and Tasks
     * ////
     * ///////////////////////////////////////////
     */
    static void startHomingTask(void *pvParameters);

    static void drawHelloTask(void *pvParameters);

    static void drawMenuTask(void *pvParameters);

    static void drawPlayControlsTask(void *pvParameters);

    static void startSimplePenetrationTask(void *pvParameters);

  public:
    explicit OSSM() {}
    void startStrokeEngine();
};

#endif  // OSSM_SOFTWARE_OSSM_H
