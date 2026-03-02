#include "actions.h"

#include "ossm/homing/homing.h"
#include "ossm/menu/menu.h"
#include "ossm/pages/error.h"
#include "ossm/pages/pairing.h"
#include "ossm/pages/hello.h"
#include "ossm/pages/help.h"
#include "ossm/pages/preflight.h"
#include "ossm/pages/update.h"
#include "ossm/pages/wifi.h"
#include "ossm/pattern_controls/pattern_controls.h"
#include "ossm/play_controls/play_controls.h"
#include "ossm/simple_penetration/simple_penetration.h"
#include "ossm/streaming/streaming.h"
#include "ossm/state/calibration.h"
#include "ossm/state/session.h"
#include "ossm/state/settings.h"
#include "ossm/stroke_engine/stroke_engine.h"
#include "services/encoder.h"
#include "services/stepper.h"
#include "services/wm.h"

void ossmDrawHello() {
    pages::drawHello();
}

void ossmDrawMenu() {
    menu::drawMenu();
}

void ossmStartHoming() {
    homing::clearHoming();
    homing::startHoming();
}

void ossmDrawPlayControls() {
    play_controls::drawPlayControls();
}

void ossmStartStreaming() {
    streaming::startStreaming();
}

void ossmDrawPatternControls() {
    pattern_controls::drawPatternControls();
}

void ossmDrawPreflight() {
    pages::drawPreflight();
}

void ossmResetSettingsStrokeEngine() {
    settings.speed = 0;
    settings.speedBLE = std::nullopt;
    settings.stroke = 50;
    settings.depth = 10;
    settings.sensation = 50;
    session.playControl = PlayControls::DEPTH;

    // Prepare the encoder
    encoder.setBoundaries(0, 100, false);
    encoder.setAcceleration(10);
    encoder.setEncoderValue(settings.depth);
}

void ossmResetSettingsSimplePen() {
    settings.speed = 0;
    settings.speedBLE = std::nullopt;
    settings.stroke = 0;
    settings.depth = 50;
    settings.sensation = 50;
    session.playControl = PlayControls::STROKE;

    // Prepare the encoder
    encoder.setBoundaries(0, 100, false);
    encoder.setAcceleration(10);
    encoder.setEncoderValue(settings.stroke);

    // record session start time rounded to the nearest second
    session.startTime = millis();
    session.strokeCount = 0;
    session.distanceMeters = 0;
}

void ossmResetSettingsStreaming() {
    settings.speed = 0;
    settings.speedBLE = std::nullopt;
    settings.stroke = 50;
    settings.depth = 50;
    settings.sensation = 50;
    settings.buffer = 100;
    session.playControl = PlayControls::DEPTH;

    // Prepare the encoder
    encoder.setBoundaries(0, 100, false);
    encoder.setAcceleration(10);
    encoder.setEncoderValue(settings.depth);
}

void ossmIncrementControlStrokeEngine() {
    session.playControl = static_cast<PlayControls>((session.playControl + 1) % 3);
    switch (session.playControl) {
        case PlayControls::STROKE:
            encoder.setEncoderValue(settings.stroke);
            break;
        case PlayControls::DEPTH:
            encoder.setEncoderValue(settings.depth);
            break;
        case PlayControls::SENSATION:
            encoder.setEncoderValue(settings.sensation);
            break;
        default:
            break;
    }
}

void ossmIncrementControlStreaming() {
    session.playControl = static_cast<PlayControls>((session.playControl + 1) % 4);
    switch (session.playControl) {
        case PlayControls::STROKE:
            encoder.setEncoderValue(settings.stroke);
            break;
        case PlayControls::DEPTH:
            encoder.setEncoderValue(settings.depth);
            break;
        case PlayControls::SENSATION:
            encoder.setEncoderValue(settings.sensation);
            break;
        case PlayControls::BUFFER:
            encoder.setEncoderValue(settings.buffer);
            break;
    }
};

void ossmStartSimplePenetration() {
    simple_penetration::startSimplePenetration();
}

void ossmStartStrokeEngine() {
    stroke_engine::startStrokeEngine();
}

void ossmEmergencyStop() {
    stepper->forceStop();
    stepper->disableOutputs();
}

void ossmDrawHelp() {
    pages::drawHelp();
}

void ossmDrawWiFi() {
    pages::drawWiFi();
}

void ossmDrawUpdate() {
    pages::drawUpdate();
}

void ossmDrawNoUpdate() {
    pages::drawNoUpdate();
}

void ossmDrawUpdating() {
    pages::drawUpdating();
}

void ossmDrawError() {
    pages::drawError();
}

void ossmCheckPairing() {
    pages::checkPairing();
}

void ossmSetHomed() {
    calibration.isHomed = true;
}

void ossmSetNotHomed() {
    calibration.isHomed = false;
}

void ossmResetWiFi() {
    wm.resetSettings();
}

void ossmRestart() {
    ESP.restart();
}
