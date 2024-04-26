#ifndef OSSM_SOFTWARE_OSSM_H
#define OSSM_SOFTWARE_OSSM_H

#include <memory>

#include "Actions.h"
#include "AiEsp32RotaryEncoder.h"
#include "ESP_FlexyStepper.h"
#include "Events.h"
#include "Guard.h"
#include "U8g2lib.h"
#include "WiFiManager.h"
#include "boost/sml.hpp"
#include "constants/Menu.h"
#include "services/tasks.h"
#include "utils/RecusiveMutex.h"
#include "utils/StateLogger.h"
#include "utils/update.h"

namespace sml = boost::sml;

class OSSM {
  private:
    void drawUpdate();
    /**
     * ///////////////////////////////////////////
     * ////
     * ////  OSSM State Machine
     * ////
     * ///////////////////////////////////////////
     *
     * This is the state machine that controls the OSSM.
     * It's based on the Boost SML library, and you don't need to be an expert
     *to use it, just follow the examples in this table:
     *
     * https://boost-ext.github.io/sml/tutorial.html#:~:text=3.%20Create%20a%20transition%20table
     *
     * Here are the basic rules:
     *  1. Each row in the table is a transition.
     *  2. Each transition has a source state, an event, a guard, an action, and
     *      a target state.
     *  3. The source state is the state that the machine must be in for the
     *      transition to be valid.
     *  4. (optional) The event is the event that triggers the transition.
     *  5. (optional) The guard is a function that returns true or false. If it
     *      returns true, then the transition is valid.
     *  6. (optional) The action is a function that is called when the
     *transition is triggered, it can't block the main thread and cannot return
     *a value.
     *  7. The target state is the state that the machine will be in after the
     *      transition is complete.
     *
     * Have fun!
     */
    struct OSSMStateMachine {
        auto operator()() const {
            // Action definitions to make the table easier to read.
            auto drawHello = [](OSSM &o) { o.drawHello(); };
            auto drawMenu = [](OSSM &o) { o.drawMenu(); };
            auto startHoming = [](OSSM &o) {
                o.clearHoming();
                o.startHoming();
            };
            auto reverseHoming = [](OSSM &o) {
                o.isForward = false;
                o.startHoming();
            };
            auto drawPlayControls = [](OSSM &o) { o.drawPlayControls(); };
            auto startSimplePenetration = [](OSSM &o) {
                o.startSimplePenetration();
            };
            auto startStrokeEngine = [](OSSM &o) { o.startStrokeEngine(); };
            auto emergencyStop = [](OSSM &o) { o.stepper.emergencyStop(); };
            auto drawHelp = [](OSSM &o) { o.drawHelp(); };
            auto drawWiFi = [](OSSM &o) { o.drawWiFi(); };
            auto drawUpdate = [](OSSM &o) { o.drawUpdate(); };
            auto drawNoUpdate = [](OSSM &o) { o.drawNoUpdate(); };
            auto drawUpdating = [](OSSM &o) { o.drawUpdating(); };
            auto stopWifiPortal = [](OSSM &o) { o.wm.stopConfigPortal(); };
            auto drawError = [](OSSM &o) { o.drawError(); };

            auto startWifi = [](OSSM &o) {
                if (WiFiClass::status() == WL_CONNECTED) {
                    return;
                }
                // Start the wifi task.

                // If you have saved wifi credentials then connect to wifi
                // immediately.
                o.wm.setConfigPortalTimeout(1);
                o.wm.setConnectTimeout(1);
                o.wm.setConnectRetries(1);
                o.wm.setConfigPortalBlocking(false);
//                if (!o.wm.autoConnect()) {
//                    ESP_LOGD("UTILS", "failed to connect and hit timeout");
//                }
                ESP_LOGD("UTILS", "exiting autoconnect");
            };

            // Guard definitions to make the table easier to read.
            auto isStrokeTooShort = [](OSSM &o) {
                return o.isStrokeTooShort();
            };

            auto isOption = [](Menu option) {
                return [option](OSSM &o) { return o.menuOption == option; };
            };

            return make_transition_table(
                // clang-format off
                *"idle"_s + done / drawHello = "homing"_s,

                "homing"_s / startHoming = "homing.idle"_s,
                "homing.idle"_s + error = "error"_s,
                "homing.idle"_s + done / reverseHoming = "homing.backward"_s,
                "homing.backward"_s + error = "error"_s,
                "homing.backward"_s + done[(isStrokeTooShort)] = "error"_s,
                "homing.backward"_s + done = "menu"_s,

                "menu"_s / (drawMenu, startWifi) = "menu.idle"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::SimplePenetration))] = "simplePenetration"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::StrokeEngine))] = "strokeEngine"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::UpdateOSSM))] = "update"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::WiFiSetup))] = "wifi"_s,
                "menu.idle"_s + buttonPress[isOption(Menu::Help)] = "help"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::Restart))] = "restart"_s,

                "simplePenetration"_s / drawPlayControls = "simplePenetration.preflight"_s,
                "simplePenetration.preflight"_s + done / startSimplePenetration = "simplePenetration.idle"_s,
                "simplePenetration.idle"_s + doublePress / emergencyStop = "menu"_s,

                "strokeEngine"_s + on_entry<_> / startStrokeEngine,
                "strokeEngine"_s + buttonPress = "menu"_s,

                "update"_s [isOnline] / drawUpdate = "update.checking"_s,
                "update"_s = "wifi"_s,
                "update.checking"_s [isUpdateAvailable] / (drawUpdating, updateOSSM) = "update.updating"_s,
                "update.checking"_s / drawNoUpdate = "update.idle"_s,
                "update.idle"_s + buttonPress = "menu"_s,
                "update.updating"_s  = X,

                "wifi"_s / drawWiFi = "wifi.idle"_s,
                "wifi.idle"_s + buttonPress / stopWifiPortal = "menu"_s,

                "help"_s / drawHelp = "help.idle"_s,
                "help.idle"_s + buttonPress = "menu"_s,

                "error"_s / drawError = "error.idle"_s,
                "error.idle"_s + buttonPress / drawHelp = "error.help"_s,
                "error.help"_s + buttonPress / restart = X,

                "restart"_s / restart = X);
            // clang-format on
        }
    };

    /**
     * ///////////////////////////////////////////
     * ////
     * ////  Private Objects and Services
     * ////
     * ///////////////////////////////////////////
     */
    ESP_FlexyStepper stepper;
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C &display;
    StateLogger logger;
    AiEsp32RotaryEncoder &encoder;

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

    bool isStrokeTooShort();

    void drawError();

    void drawHello();

    void drawHelp();

    void drawWiFi();

    void drawMenu();

    void drawPlayControls();

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
    explicit OSSM(U8G2_SSD1306_128X64_NONAME_F_HW_I2C &display,
                  AiEsp32RotaryEncoder &rotaryEncoder);

    std::unique_ptr<
        sml::sm<OSSMStateMachine, sml::thread_safe<ESP32RecursiveMutex>,
                sml::logger<StateLogger>>>
        sm = nullptr;  // The state machine

    WiFiManager wm;
    void startStrokeEngine();
    void drawNoUpdate();
    void drawUpdating();
};

#endif  // OSSM_SOFTWARE_OSSM_H
