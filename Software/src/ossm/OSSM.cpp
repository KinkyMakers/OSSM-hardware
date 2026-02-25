#include "OSSM.h"

#include "command/commands.hpp"
#include "ossm/state/ble.h"
#include "ossm/state/calibration.h"
#include "ossm/state/menu.h"
#include "ossm/state/session.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/communication/mqtt.h"
#include "services/communication/queue.h"
#include "services/encoder.h"
#include "services/stepper.h"

namespace sml = boost::sml;
using namespace sml;

// Global OSSM pointer (kept for backward compatibility during migration)
OSSM *ossm = nullptr;

// Static member definition, with first pattern.
SettingPercents OSSM::setting = {.speed = 0,
                                 .stroke = 50,
                                 .sensation = 50,
                                 .depth = 10,
                                 .pattern = StrokePatterns(0)};

OSSM::OSSM() {
    // Initialize global state from OSSM::setting
    settings = OSSM::setting;
}

void OSSM::ble_click(String commandString) {
    ESP_LOGD("OSSM", "PROCESSING CLICK");
    CommandValue command = commandFromString(commandString);
    ESP_LOGD("OSSM", "COMMAND: %d", command.command);

    String currentState;
    if (stateMachine != nullptr) {
        stateMachine->visit_current_states(
            [&currentState](auto state) { currentState = state.c_str(); });
    }

    switch (command.command) {
        case Commands::goToStrokeEngine:
            menuState.currentOption = Menu::StrokeEngine;
            if (stateMachine != nullptr) {
                stateMachine->process_event(ButtonPress{});
            }
            break;
        case Commands::goToSimplePenetration:
            menuState.currentOption = Menu::SimplePenetration;
            if (stateMachine != nullptr) {
                stateMachine->process_event(ButtonPress{});
            }
            break;
        case Commands::goToStreaming:
            menuState.currentOption = Menu::Streaming;
            if (stateMachine != nullptr) {
                stateMachine->process_event(ButtonPress{});
            }
            break;
        case Commands::goToMenu:
            if (stateMachine != nullptr) {
                stateMachine->process_event(LongPress{});
            }
            break;
        case Commands::setSpeed:
            // BLE devices can be trusted to send true value
            // and can bypass potentiomer smoothing logic
            bleState.lastSpeedCommandWasFromBLE = true;
            // Use speed knob config to determine how to handle BLE speed
            // command
            settings.speedBLE = command.value;
            break;
        case Commands::setStroke:
            session.playControl = PlayControls::STROKE;
            encoder.setEncoderValue(command.value);
            settings.stroke = command.value;
            break;
        case Commands::setDepth:
            session.playControl = PlayControls::DEPTH;
            encoder.setEncoderValue(command.value);
            settings.depth = command.value;
            break;
        case Commands::setSensation:
            session.playControl = PlayControls::SENSATION;
            encoder.setEncoderValue(command.value);
            settings.sensation = command.value;
            break;
        case Commands::setPattern:
            settings.pattern = static_cast<StrokePatterns>(command.value % 7);
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
        case Commands::setWifi:
        case Commands::ignore:
            break;
    }
}

String OSSM::getCurrentState(bool detailed) {
    String currentState;
    if (stateMachine != nullptr) {
        stateMachine->visit_current_states(
            [&currentState](auto state) { currentState = state.c_str(); });
    }

    String json = "{";
    if (detailed) {
        json += "\"timestamp\":" + String(millis()) + ",";
        json += "\"position\":" + String(float(-stepper->getCurrentPosition()) / float(1_mm)) + ",";
    }
    json += "\"state\":\"" + currentState + "\",";
    json += "\"speed\":" + String((int)settings.speed) + ",";
    json += "\"stroke\":" + String((int)settings.stroke) + ",";
    json += "\"sensation\":" + String((int)settings.sensation) + ",";
    json += "\"depth\":" + String((int)settings.depth) + ",";
    json += "\"pattern\":" + String(static_cast<int>(settings.pattern)) + ",";
    json += "\"sessionId\":\"" + sessionId + "\"";
    json += "}";

    return json;
}
