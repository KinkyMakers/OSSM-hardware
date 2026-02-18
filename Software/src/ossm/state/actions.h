#ifndef OSSM_STATE_ACTIONS_H
#define OSSM_STATE_ACTIONS_H

// Forward declarations for action implementations (defined in actions.cpp)
void ossmDrawHello();
void ossmDrawMenu();
void ossmStartHoming();
void ossmDrawPlayControls();
void ossmStartStreaming();
void ossmDrawPatternControls();
void ossmDrawPreflight();
void ossmResetSettingsStrokeEngine();
void ossmResetSettingsSimplePen();
void ossmIncrementControl();
void ossmStartSimplePenetration();
void ossmStartStrokeEngine();
void ossmEmergencyStop();
void ossmDrawHelp();
void ossmDrawWiFi();
void ossmDrawUpdate();
void ossmDrawNoUpdate();
void ossmDrawUpdating();
void ossmDrawError();
void ossmSetHomed();
void ossmSetNotHomed();
void ossmCheckPairing();
void ossmResetWiFi();
void ossmRestart();

namespace actions {

    constexpr auto drawHello = []() { ossmDrawHello(); };
    
    constexpr auto drawMenu = []() { ossmDrawMenu(); };
    
    constexpr auto startHoming = []() { ossmStartHoming(); };
    
    constexpr auto drawPlayControls = []() { ossmDrawPlayControls(); };
    
    constexpr auto startStreaming = []() { ossmStartStreaming(); };
    
    constexpr auto drawPatternControls = []() { ossmDrawPatternControls(); };
    
    constexpr auto drawPreflight = []() { ossmDrawPreflight(); };
    
    constexpr auto resetSettingsStrokeEngine = []() { ossmResetSettingsStrokeEngine(); };

    constexpr auto resetSettingsSimplePen = []() { ossmResetSettingsSimplePen(); };

    constexpr auto incrementControl = []() { ossmIncrementControl(); };

    constexpr auto startSimplePenetration = []() { ossmStartSimplePenetration(); };

    constexpr auto startStrokeEngine = []() { ossmStartStrokeEngine(); };
    
    constexpr auto emergencyStop = []() { ossmEmergencyStop(); };
    
    constexpr auto drawHelp = []() { ossmDrawHelp(); };
    
    constexpr auto drawWiFi = []() { ossmDrawWiFi(); };
    
    constexpr auto drawUpdate = []() { ossmDrawUpdate(); };
    
    constexpr auto drawNoUpdate = []() { ossmDrawNoUpdate(); };
    
    constexpr auto drawUpdating = []() { ossmDrawUpdating(); };
    
    constexpr auto stopWifiPortal = []() {};
    
    constexpr auto resetWiFi = []() { ossmResetWiFi(); };
    
    constexpr auto drawError = []() { ossmDrawError(); };
    
    constexpr auto checkPairing = []() { ossmCheckPairing(); };

    constexpr auto setHomed = []() { ossmSetHomed(); };
    
    constexpr auto setNotHomed = []() { ossmSetNotHomed(); };
    
    constexpr auto restart = []() { ossmRestart(); };

}  // namespace actions

#endif  // OSSM_STATE_ACTIONS_H
