#ifndef OSSM_SOFTWARE_OSSM_H
#define OSSM_SOFTWARE_OSSM_H

#include <Arduino.h>
#include <services/board.h>
#include <services/communication/queue.h>

#include <command/commands.hpp>

#include "Actions.h"
#include "AiEsp32RotaryEncoder.h"
#include "Events.h"
#include "FastAccelStepper.h"
#include "Guard.h"
#include "OSSMI.h"
#include "U8g2lib.h"
#include "WiFiManager.h"
#include "boost/sml.hpp"
#include "constants/Config.h"
#include "constants/Menu.h"
#include "constants/Pins.h"
#include "constants/UserConfig.h"
#include "esp_log.h"
#include "services/tasks.h"
#include "structs/SettingPercents.h"
#include "utils/RecursiveMutex.h"
#include "utils/StateLogger.h"
#include "utils/StrokeEngineHelper.h"
#include "utils/analog.h"
#include "utils/update.h"

namespace sml = boost::sml;

class OSSM : public OSSMInterface {
  private:
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
            auto drawPlayControls = [](OSSM &o) { o.drawPlayControls(); };
            auto startStreaming = [](OSSM &o) { o.startStreaming(); };
            auto drawPatternControls = [](OSSM &o) { o.drawPatternControls(); };
            auto drawPreflight = [](OSSM &o) { o.drawPreflight(); };

            // armpit: Distinct defaults for StrokeEngine and SimplePenetration
            // for more tailored UX
            auto resetSettingsStrokeEngine = [](OSSM &o) {
                OSSM::setting.speed = 0;
                OSSM::setting.speedBLE = std::nullopt;
                OSSM::setting.stroke = 50;
                OSSM::setting.depth = 10;
                OSSM::setting.sensation = 50;
                o.playControl = PlayControls::DEPTH;

                // Prepare the encoder
                o.encoder.setBoundaries(0, 100, false);
                o.encoder.setAcceleration(10);
                o.encoder.setEncoderValue(OSSM::setting.depth);
            };

            auto resetSettingsSimplePen = [](OSSM &o) {
                OSSM::setting.speed = 0;
                OSSM::setting.speedBLE = std::nullopt;
                OSSM::setting.stroke = 0;
                OSSM::setting.depth = 50;
                OSSM::setting.sensation = 50;
                o.playControl = PlayControls::STROKE;

                // Prepare the encoder
                o.encoder.setBoundaries(0, 100, false);
                o.encoder.setAcceleration(10);
                o.encoder.setEncoderValue(OSSM::setting.stroke);

                // record session start time rounded to the nearest second
                o.sessionStartTime = millis();
                o.sessionStrokeCount = 0;
                o.sessionDistanceMeters = 0;
            };

            auto incrementControl = [](OSSM &o) {
                o.playControl =
                    static_cast<PlayControls>((o.playControl + 1) % 3);

                switch (o.playControl) {
                    case PlayControls::STROKE:
                        o.encoder.setEncoderValue(OSSM::setting.stroke);
                        break;
                    case PlayControls::DEPTH:
                        o.encoder.setEncoderValue(OSSM::setting.depth);
                        break;
                    case PlayControls::SENSATION:
                        o.encoder.setEncoderValue(OSSM::setting.sensation);
                        break;
                }
            };

            auto startSimplePenetration = [](OSSM &o) {
                o.startSimplePenetration();
            };

            auto startStrokeEngine = [](OSSM &o) { o.startStrokeEngine(); };
            auto emergencyStop = [](OSSM &o) {
                o.stepper->forceStop();
                o.stepper->disableOutputs();
            };
            auto drawHelp = [](OSSM &o) { o.drawHelp(); };
            auto drawWiFi = [](OSSM &o) { o.drawWiFi(); };
            auto drawUpdate = [](OSSM &o) { o.drawUpdate(); };
            auto drawNoUpdate = [](OSSM &o) { o.drawNoUpdate(); };
            auto drawUpdating = [](OSSM &o) { o.drawUpdating(); };
            auto stopWifiPortal = [](OSSM &o) {};
            auto drawError = [](OSSM &o) { o.drawError(); };

            // Guard definitions to make the table easier to read.
            auto isStrokeTooShort = [](OSSM &o) {
#ifdef AJ_DEVELOPMENT_HARDWARE
                return false;
#else
                return o.isStrokeTooShort();
#endif
            };

            auto isOption = [](Menu option) {
                return [option](OSSM &o) { return o.menuOption == option; };
            };

            auto isPreflightSafe = [](OSSM &o) {
                return getAnalogAveragePercent(
                           {Pins::Remote::speedPotPin, 50}) <
                       Config::Advanced::commandDeadZonePercentage;
            };

            auto isFirstHomed = [](OSSM &o) {
                static bool firstHomed = true;
                if (firstHomed) {
                    firstHomed = false;
                    return true;
                }
                return false;
            };
            auto isNotHomed = [](OSSM &o) { return o.isHomed == false; };
            auto setHomed = [](OSSM &o) { o.isHomed = true; };
            auto setNotHomed = [](OSSM &o) { o.isHomed = false; };

            return make_transition_table(
            // clang-format off
#ifdef AJ_DEVELOPMENT_HARDWARE
                *"idle"_s + done = "menu"_s,
#else
                *"idle"_s + done / drawHello = "homing"_s,
#endif

                "homing"_s / startHoming = "homing.forward"_s,
                "homing.forward"_s + error = "error"_s,
                "homing.forward"_s + done / startHoming = "homing.backward"_s,
                "homing.backward"_s + error = "error"_s,
                "homing.backward"_s + done[(isStrokeTooShort)] = "error"_s,
                "homing.backward"_s + done[isFirstHomed] / setHomed = "menu"_s,
                "homing.backward"_s + done[(isOption(Menu::SimplePenetration))] / setHomed = "simplePenetration"_s,
                "homing.backward"_s + done[(isOption(Menu::StrokeEngine))] / setHomed = "strokeEngine"_s,
                "homing.backward"_s + done[(isOption(Menu::Streaming))] / setHomed = "streaming"_s,

                "menu"_s / (drawMenu) = "menu.idle"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::SimplePenetration))] = "simplePenetration"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::StrokeEngine))] = "strokeEngine"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::Streaming))] = "streaming"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::UpdateOSSM))] = "update"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::WiFiSetup))] = "wifi"_s,
                "menu.idle"_s + buttonPress[isOption(Menu::Help)] = "help"_s,
                "menu.idle"_s + buttonPress[(isOption(Menu::Restart))] = "restart"_s,

                "simplePenetration"_s [isNotHomed] = "homing"_s,
                "simplePenetration"_s [isPreflightSafe] / (resetSettingsSimplePen, drawPlayControls, startSimplePenetration) = "simplePenetration.idle"_s,
                "simplePenetration"_s / drawPreflight = "simplePenetration.preflight"_s,
                "simplePenetration.preflight"_s + done / (resetSettingsSimplePen, drawPlayControls, startSimplePenetration) = "simplePenetration.idle"_s,
                "simplePenetration.preflight"_s + longPress = "menu"_s,
                "simplePenetration.idle"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,

                "strokeEngine"_s [isNotHomed] = "homing"_s,
                "strokeEngine"_s [isPreflightSafe] / (resetSettingsStrokeEngine, drawPlayControls, startStrokeEngine) = "strokeEngine.idle"_s,
                "strokeEngine"_s / drawPreflight = "strokeEngine.preflight"_s,
                "strokeEngine.preflight"_s + done / (resetSettingsStrokeEngine, drawPlayControls, startStrokeEngine) = "strokeEngine.idle"_s,
                "strokeEngine.preflight"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,
                "strokeEngine.idle"_s + buttonPress / incrementControl = "strokeEngine.idle"_s,
                "strokeEngine.idle"_s + doublePress / drawPatternControls = "strokeEngine.pattern"_s,
                "strokeEngine.pattern"_s + buttonPress / drawPlayControls = "strokeEngine.idle"_s,
                "strokeEngine.pattern"_s + doublePress / drawPlayControls = "strokeEngine.idle"_s,
                "strokeEngine.pattern"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,
                "strokeEngine.idle"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,

                "streaming"_s [isNotHomed] = "homing"_s,
                "streaming"_s [isPreflightSafe] / ( drawPlayControls, startStreaming) = "streaming.idle"_s,
                "streaming"_s / drawPreflight = "streaming.preflight"_s,
                "streaming.preflight"_s + done / ( drawPlayControls, startStreaming) = "streaming.idle"_s,
                "streaming.preflight"_s + longPress = "menu"_s,
                "streaming.idle"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,

                "update"_s [isOnline] / drawUpdate = "update.checking"_s,
                "update"_s = "wifi"_s,
                "update.checking"_s [isUpdateAvailable] / (drawUpdating, updateOSSM) = "update.updating"_s,
                "update.checking"_s / drawNoUpdate = "update.idle"_s,
                "update.idle"_s + buttonPress = "menu"_s,
                "update.updating"_s  = X,

                "wifi"_s / drawWiFi = "wifi.idle"_s,
                "wifi.idle"_s + done / stopWifiPortal = "menu"_s,
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
     * ////  Private Variables and Flags
     * ////
     * ///////////////////////////////////////////
     */

    // Homing Variables
    bool isForward = true;

    String errorMessage = "";

    unsigned long sessionStartTime = 0;
    int sessionStrokeCount = 0;
    double sessionDistanceMeters = 0;

    PlayControls playControl = PlayControls::STROKE;

    bool lastSpeedCommandWasFromBLE = false;
    bool hasActiveBLEConnection = false;

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
    void drawPatternControls();

    /**
     * ///////////////////////////////////////////
     * ////
     * ////  Static Functions and Tasks
     * ////
     * ///////////////////////////////////////////
     */
    static void startHomingTask(void *pvParameters);

    void startStreaming();

    void startStrokeEngine();

    static void drawHelloTask(void *pvParameters);

    static void drawMenuTask(void *pvParameters);

    static void drawPlayControlsTask(void *pvParameters);
    static void drawPatternControlsTask(void *pvParameters);

    void drawUpdate();
    void drawNoUpdate();

    void drawUpdating();

    void drawPreflight();
    static void drawPreflightTask(void *pvParameters);

    static void startSimplePenetrationTask(void *pvParameters);

    static void startStrokeEngineTask(void *pvParameters);

    bool isHomed;

  public:
    explicit OSSM(U8G2_SSD1306_128X64_NONAME_F_HW_I2C &display,
                  AiEsp32RotaryEncoder &rotaryEncoder,
                  FastAccelStepper *stepper);

    std::unique_ptr<
        sml::sm<OSSMStateMachine, sml::thread_safe<ESP32RecursiveMutex>,
                sml::logger<StateLogger>>>
        sm = nullptr;  // The state machine

    static SettingPercents setting;
    Menu menuOption;

    /**
     * ///////////////////////////////////////////
     * ////
     * ////  Objects and Services
     * ////
     * ///////////////////////////////////////////
     */
    FastAccelStepper *stepper;
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C &display;
    StateLogger logger;
    AiEsp32RotaryEncoder &encoder;

    /**
     * ///////////////////////////////////////////
     * ////
     * ////  Calibration Variables
     * ////
     * ///////////////////////////////////////////
     */
    float currentSensorOffset = 0;
    float measuredStrokeSteps = 0;

    /**
     * ///////////////////////////////////////////
     * ////
     * ////  Target Variables
     * ////
     * ///////////////////////////////////////////
     */
    float targetPosition = 0;
    float targetVelocity = 0;
    uint16_t targetTime = 0;

    int getSpeed() { return this->setting.speed; }
    // Implement the interface methods
    template <typename EventType>
    void process_event(const EventType &event) {
        sm->process_event(event);
    }
    void ble_click(String commandString) {
        // Visit current state to handle state-specific commands

        ESP_LOGD("OSSM", "PROCESSING CLICK");
        CommandValue command = commandFromString(commandString);
        ESP_LOGD("OSSM", "COMMAND: %d", command.command);

        String currentState;
        sm->visit_current_states(
            [&currentState](auto state) { currentState = state.c_str(); });

        switch (command.command) {
            case Commands::goToStrokeEngine:
                menuOption = Menu::StrokeEngine;
                sm->process_event(ButtonPress{});
                break;
            case Commands::goToSimplePenetration:
                menuOption = Menu::SimplePenetration;
                sm->process_event(ButtonPress{});
                break;
            case Commands::goToStreaming:
                menuOption = Menu::Streaming;
                sm->process_event(ButtonPress{});
                break;
            case Commands::goToMenu:
                sm->process_event(LongPress{});
                break;
            case Commands::setSpeed:
                // BLE devices can be trusted to send true value
                // and can bypass potentiomer smoothing logic
                lastSpeedCommandWasFromBLE = true;
                // Use speed knob config to determine how to handle BLE speed
                // command
                setting.speedBLE = command.value;
                break;
            case Commands::setStroke:
                playControl = PlayControls::STROKE;
                encoder.setEncoderValue(command.value);
                setting.stroke = command.value;
                break;
            case Commands::setDepth:
                playControl = PlayControls::DEPTH;
                encoder.setEncoderValue(command.value);
                setting.depth = command.value;
                break;
            case Commands::setSensation:
                playControl = PlayControls::SENSATION;
                encoder.setEncoderValue(command.value);
                setting.sensation = command.value;
                break;
            case Commands::setPattern:
                setting.pattern =
                    static_cast<StrokePatterns>(command.value % 7);
                break;
            case Commands::streamPosition:
                // Scale position from 0-100 to 0-180 (internal format)
                // and update streaming target
                lastPositionTime = targetPositionTime;
                targetPositionTime = {
                    static_cast<uint8_t>((command.value * 180) / 100),
                    static_cast<uint16_t>(command.time)};
                markTargetUpdated();
                ESP_LOGD("OSSM", "Stream: pos=%d, time=%d", command.value,
                         command.time);
                break;
            case Commands::ignore:
                break;
        }
    }

    void moveTo(float intensity, uint16_t inTime) {
        targetPosition = constrain(intensity, 0.0f, 100.0f);
        targetTime = inTime;
    }

    // get current state
    String getCurrentState() {
        String currentState;
        sm->visit_current_states(
            [&currentState](auto state) { currentState = state.c_str(); });

        String json = "{";
        json += "\"state\":\"" + currentState + "\",";
        json += "\"speed\":" + String((int)setting.speed) + ",";
        json += "\"stroke\":" + String((int)setting.stroke) + ",";
        json += "\"sensation\":" + String((int)setting.sensation) + ",";
        json += "\"depth\":" + String((int)setting.depth) + ",";
        json += "\"pattern\":" + String(static_cast<int>(setting.pattern));
        json += "}";
        currentState = json;

        return currentState;
    }

    // BLE command tracking methods
    bool wasLastSpeedCommandFromBLE(bool andReset = false) {
        if (andReset) {
            bool temp = lastSpeedCommandWasFromBLE;
            resetLastSpeedCommandWasFromBLE();
            return temp;
        }
        return lastSpeedCommandWasFromBLE;
    }

    void resetLastSpeedCommandWasFromBLE() {
        lastSpeedCommandWasFromBLE = false;
    }

    // BLE connection tracking methods
    bool hasActiveBLE() const { return hasActiveBLEConnection; }

    void setBLEConnectionStatus(bool isConnected) {
        hasActiveBLEConnection = isConnected;
    }
};

extern OSSM *ossm;

#endif  // OSSM_SOFTWARE_OSSM_H
