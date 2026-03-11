#ifndef UI_STRINGS_H
#define UI_STRINGS_H

#include "Progmem.h"

namespace ui {
    namespace strings {

        // ============================================================
        // Branding
        // ============================================================
        static const char researchAndDesire[] PROGMEM =
            "Research & Desire         ";
        static const char kinkyMakers[] PROGMEM = "Kinky Makers       ";

        // ============================================================
        // General UI labels
        // ============================================================
        static const char error[] PROGMEM = "Error";
        static const char homing[] PROGMEM = "Homing";
        static const char idle[] PROGMEM = "Initializing";
        static const char restart[] PROGMEM = "Restart";
        static const char settings[] PROGMEM = "Settings";
        static const char skip[] PROGMEM = "Click to exit";

        // ============================================================
        // Menu items / operation modes
        // ============================================================
        static const char simplePenetration[] PROGMEM =
            "Simple Penetration";
        static const char strokeEngine[] PROGMEM = "Stroke Engine";
        static const char streaming[] PROGMEM = "Streaming (beta)";
        static const char deepThroatTrainerSync[] PROGMEM =
            "DeepThroat Sync";
        static const char pairing[] PROGMEM = "Pairing";
        static const char update[] PROGMEM = "Update";
        static const char wifi[] PROGMEM = "Wi-Fi";

        // ============================================================
        // Play controls
        // ============================================================
        static const char speed[] PROGMEM = "Speed";
        static const char stroke[] PROGMEM = "Stroke";
        static const char sensation[] PROGMEM = "Sensation";
        static const char depth[] PROGMEM = "Depth";
        static const char buffer[] PROGMEM = "Buffer";
        static const char accel[] PROGMEM = "Accel";
        static const char max[] PROGMEM = "Max";

        // ============================================================
        // Warnings / errors
        // ============================================================
        static const char speedWarning[] PROGMEM =
            "Decrease the speed to begin playing.";
        static const char homingTookTooLong[] PROGMEM =
            "Homing took too long. Please check your wiring and try again.";
        static const char strokeTooShort[] PROGMEM =
            "Stroke too short. Please check you drive belt.";
        static const char inDevelopment[] PROGMEM =
            "This feature is in development.";
        static const char noInternalLoop[] PROGMEM =
            "No display handler implemented.";
        static const char stateNotImplemented[] PROGMEM =
            "State: %u not implemented.";
        static const char youShouldNotBeHere[] PROGMEM =
            "You should not be here.";
        static const char measuringStroke[] PROGMEM = "Measuring Stroke";
        static const char noDescription[] PROGMEM =
            "No description available";

        // ============================================================
        // Help page
        // ============================================================
        static const char helpTitle[] PROGMEM = "Get Help";
        static const char helpBody[] PROGMEM = "On Discord,\nor GitHub";
        static const char helpBottom[] PROGMEM = "Click to exit";
        static const char helpQr[] PROGMEM =
            "HTTPS://DOCS.RESEARCHANDDESIRE.COM/OSSM";

        // ============================================================
        // Update pages
        // ============================================================
        static const char updateChecking[] PROGMEM =
            "Checking for update...";
        static const char noUpdateBody[] PROGMEM = "No Update Available";
        static const char noUpdateBottom[] PROGMEM = "Click to exit";
        static const char updatingTitle[] PROGMEM = "Updating OSSM...";
        static const char updatingBody[] PROGMEM =
            "Update is in progress. This may take up to 60s.";

        // ============================================================
        // WiFi pages
        // ============================================================
        static const char wifiSetup[] PROGMEM = "Wi-Fi Setup";
        static const char wifiBody[] PROGMEM =
            "Connect to\n'Ossm Setup'";
        static const char wifiBottom[] PROGMEM = "Restart";
        static const char wifiQr[] PROGMEM =
            "WIFI:S:OSSM Setup;T:nopass;;";
        static const char ossmSetup[] PROGMEM = "OSSM Setup";
        static const char wifiConnected[] PROGMEM =
            "You are now connected to WiFi!";
        static const char longPressReset[] PROGMEM =
            "Long press to reset WiFi";

        // ============================================================
        // Pairing page
        // ============================================================
        static const char pairingTitle[] PROGMEM = "Pair OSSM";
        static const char pairingBody[] PROGMEM =
            "Enter code\nor scan QR";

        // ============================================================
        // Stroke Engine pattern names
        // ============================================================
        static const char patternName0[] PROGMEM = "Simple Stroke";
        static const char patternName1[] PROGMEM = "Teasing Pounding";
        static const char patternName2[] PROGMEM = "Robo Stroke";
        static const char patternName3[] PROGMEM = "Half'n'Half";
        static const char patternName4[] PROGMEM = "Deeper";
        static const char patternName5[] PROGMEM = "Stop'n'Go";
        static const char patternName6[] PROGMEM = "Insist";
        static const char patternName7[] PROGMEM = "Progressive Stroke";
        static const char patternName8[] PROGMEM = "Random Stroke";
        static const char patternName9[] PROGMEM = "Go to Point";

        static const char* const strokeEngineNames[10] = {
                patternName0, patternName1, patternName2,
                patternName3, patternName4, patternName5,
                patternName6, patternName7, patternName8,
                patternName9,
        };

        // ============================================================
        // Stroke Engine pattern descriptions
        // ============================================================
        static const char patternDesc0[] PROGMEM =
            "Acceleration, coasting, deceleration equally split; "
            " Sensation adds randomness in stroke.";
        static const char patternDesc1[] PROGMEM =
            "Speed shifts with sensation; balances faster strokes.";
        static const char patternDesc2[] PROGMEM =
            "Sensation varies acceleration; from robotic to gradual.";
        static const char patternDesc3[] PROGMEM =
            "Full and half depth strokes alternate; sensation affects speed.";
        static const char patternDesc4[] PROGMEM =
            "Stroke depth increases per cycle; sensation sets count.";
        static const char patternDesc5[] PROGMEM =
            "Pauses between strokes; sensation adjusts length.";
        static const char patternDesc6[] PROGMEM =
            "Stroke length decreaes per cycle to set depth; sensation sets count.";
        static const char patternDesc7[] PROGMEM =
            "Strokes are made of substrokes controlled by sensation.";
        static const char patternDesc8[] PROGMEM =
            "Random Stroke. Sensation controls maximum randomness from current location.";
        static const char patternDesc9[] PROGMEM =
            "Moves to a point between depth and stroke based on sensation.";

        static const char* const strokeEngineDescriptions[10] = {
                patternDesc0, patternDesc1, patternDesc2,
                patternDesc3, patternDesc4, patternDesc5,
                patternDesc6, patternDesc7, patternDesc8,
                patternDesc9,
        };

    }  // namespace strings
}  // namespace ui

#endif  // UI_STRINGS_H
