#ifndef OSSM_SOFTWARE_CONFIG_H
#define OSSM_SOFTWARE_CONFIG_H

#include <U8g2lib.h>

#include <cmath>
#include <cstdint>

#include "services/device_config.h"

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
        // Top acceleration of the device in mm/s/s
        constexpr float maxAcceleration =
            device_config_logic::kMaxAccelerationMmPerS2;
        // This is the measured current that use to infer when the device has
        // reached the end of its stroke. during "Homing".
        constexpr float sensorlessCurrentLimit = 6.0f;
        // Soft endstop in mm. StrokeEngine's keepoutBoundary and the frame
        // translation in stepper.cpp must use the same value.
        constexpr float keepoutBoundaryMm = 6.0f;

        // Steps/rev, pulley teeth, belt pitch, max RPM and max stroke are
        // stored in NVS (services/device_config.h) and fixed from boot until
        // restart. Defaults: 800 steps/rev, 20 teeth, 2 mm GT2, 1500 RPM,
        // 500 mm. Steps/rev must match the DIP switches on the servo/stepper.
        inline float stepsPerMM() {
            return device_config_logic::stepsPerMm(device_config::active());
        }

        inline float maxStepRateHz() {
            return std::fmin(
                device_config_logic::maxStepRateHz(device_config::active()),
                device_config_logic::kMaxStepRateHz);
        }

        inline float maxSpeedMmPerSecond() {
            return maxStepRateHz() / stepsPerMM();
        }

        // Step rate for a linear speed, clamped to the machine's limit.
        inline uint32_t stepRateHz(float mmPerSecond) {
            return device_config_logic::clampStepRateHz(
                mmPerSecond * stepsPerMM(), maxStepRateHz());
        }

        namespace Operator {
            // Define user-defined literal for unsigned integer values
            inline long long operator"" _mm(unsigned long long x) {
                return x * stepsPerMM();
            }

            // Define user-defined literal for floating-point values
            inline long double operator"" _mm(long double x) {
                return x * stepsPerMM();
            }
        }

        using namespace Operator;

        // How much of the rail is usable, in steps. The absolute max is the
        // distance between the belt attachments minus the linear block holder
        // length (75mm on OSSM); also subtract e.g. 20mm to keep the backstop
        // well away from the device.
        inline float maxStrokeSteps() {
            return device_config::active().maxStrokeMm * stepsPerMM();
        }

        // If the stroke length is less than this value, then the stroke is
        // likely the result of a poor homing.
        inline float minStrokeLengthSteps() {
            return device_config_logic::kMinStrokeLengthMm * stepsPerMM();
        }

        // Applied offset after homing process, in steps.
        inline float homingOffsetSteps() { return 10_mm; }
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
