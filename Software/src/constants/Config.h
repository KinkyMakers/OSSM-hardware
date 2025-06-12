#ifndef OSSM_SOFTWARE_CONFIG_H
#define OSSM_SOFTWARE_CONFIG_H

/**
    Default Config for OSSM - Reference board users should tweak UserConfig to
   match their personal build.

    //TODO: restore user overrides with a clever null coalescing operator
*/
namespace Config {

    /**
            Motion System Config
    */
    namespace Driver {

        // Top linear speed of the device.
        constexpr float maxSpeedMmPerSecond = 900.0f;

        // Top acceleration of the device.
        constexpr float maxAcceleration = 10000.0f;

        // This should match the step/rev of your stepper or servo.
        // N.b. the iHSV57 has a table on the side for setting the DIP switches
        // to your preference.
        constexpr float motorStepPerRevolution = 800.0f;

        // Number of teeth the pulley that is attached to the servo/stepper
        // shaft has.
        constexpr float pulleyToothCount = 20.0f;

        // Set to your belt pitch (Distance between two teeth on the belt) (E.g.
        // GT2 belt has 2mm tooth pitch)
        constexpr float beltPitchMm = 2.0f;

        // This is the measured current that use to infer when the device has
        // reached the end of its stroke. during "Homing".
        constexpr float sensorlessCurrentLimit = 1.2f;

        constexpr float stepsPerMM =
            motorStepPerRevolution / (pulleyToothCount * beltPitchMm);

        namespace Operator {
            // Define user-defined literal for unsigned integer values
            constexpr long long operator"" _mm(unsigned long long x) {
                return x * stepsPerMM;
            }

            // Define user-defined literal for floating-point values
            constexpr long double operator"" _mm(long double x) {
                return x * stepsPerMM;
            }
        }

        using namespace Operator;


        // This is in millimeters, and is what's used to define how much of
        // your rail is usable.
        // The absolute max your OSSM would have is the distance between the
        // belt attachments subtract the linear block holder length (75mm on
        // OSSM) Recommended to also subtract e.g. 20mm to keep the backstop
        // well away from the device.
        constexpr float maxStrokeSteps = 500.0_mm;

        // If the stroke length is less than this value, then the stroke is
        // likely the result of a poor homing.
        constexpr float minStrokeLengthMm = 50.0_mm;
    }

    /**
        Web Config
*/
    namespace Web {
        // This should be unique to your device. You will use this on the
        // web portal to interact with your OSSM.
        // there is NO security other than knowing this name, make this unique
        // to avoid collisions with other users
        constexpr char *ossmId = nullptr;
    }

    /**
        Font Config. These must be the "f" variants of the font to support other
       languages.
*/
    namespace Font {
        static auto bold = u8g2_font_helvB08_tf;
        static auto base = u8g2_font_helvR08_tf;
        static auto small = u8g2_font_6x10_tf;
    }

    /**
        Advanced Config
*/
    namespace Advanced {

        // After homingStart this is the physical buffer distance from the
        // effective zero to the home switch This is to stop the home switch
        // being smacked constantly
        constexpr float strokeZeroOffsetMm = 6.0f;
        // The minimum value of the pot in percent
        // prevents noisy pots registering commands when turned down to zero by
        // user
        constexpr float commandDeadZonePercentage = 1.0f;
        // affects acceleration in stepper trajectory (Aggressiveness of motion)

        constexpr float accelerationScaling = 100.0f;

    }

}

// Alias for "_mm" operator
using namespace Config::Driver::Operator;

#endif  // OSSM_SOFTWARE_CONFIG_H
