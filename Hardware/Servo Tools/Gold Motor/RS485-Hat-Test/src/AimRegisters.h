#pragma once

#include <Arduino.h>

/**
 * AIM-series (57AIM30 "Gold Motor") Modbus register map.
 *
 * Source: YZ-AIM manual v2.55, section V.2 (Hardware/Servo Tools/Gold Motor/YZ-AIMManual_v2_55.pdf),
 * cross-checked against the GMP web tool (GMP/script.js).
 *
 * Notes from the manual:
 *  - 0x00 (Modbus enable) must be 1 before any other parameter can be written,
 *    and while it is 1 the external STEP/DIR input is ignored.
 *  - Ranges in the manual are not always respected by shipped motors (GMP's
 *    defaults use 0x01 = 7 and 0x04 = 495), so out-of-range writes ask for
 *    confirmation instead of being refused.
 */
namespace Aim {

    constexpr uint8_t defaultServerId = 1;
    constexpr uint32_t defaultBaud = 19200;
    constexpr uint16_t registerCount = 0x1A;  // 0x00..0x19

    namespace Reg {
        constexpr uint16_t modbusEnable = 0x00;
        constexpr uint16_t driveOutputEnable = 0x01;
        constexpr uint16_t gearNumerator = 0x0A;
        constexpr uint16_t stepsPerRev = 0x0B;  // electronic gear denominator
        constexpr uint16_t targetPositionLow = 0x0C;  // incremental move, FC 0x10 only (2 words, low word first)
        constexpr uint16_t alarmCode = 0x0E;
        constexpr uint16_t current = 0x0F;
        constexpr uint16_t speed = 0x10;
        constexpr uint16_t voltage = 0x11;
        constexpr uint16_t temperature = 0x12;
        constexpr uint16_t pwm = 0x13;
        constexpr uint16_t saveFlag = 0x14;
        constexpr uint16_t absPositionLow = 0x16;
        constexpr uint16_t absPositionHigh = 0x17;
        constexpr uint16_t specialFunction = 0x19;
    }

    enum class Danger : uint8_t {
        None,
        Always,     // any write needs confirmation
        WhenZero,   // writing 0 needs confirmation
        WhenNonZero // writing anything but 0 needs confirmation
    };

    struct Register {
        uint16_t addr;
        const char *name;
        bool writable;     // via FC 0x06
        bool isSigned;
        float scale;       // 0 = show raw only, otherwise display = raw * scale
        const char *unit;
        uint16_t min;
        uint16_t max;
        Danger danger;
        const char *note;
    };

    // clang-format off
    constexpr Register registers[registerCount] = {
        {0x00, "MODBUS_ENABLE",           true,  false, 0,             "",     0, 1,     Danger::None,        "1 = params writable, STEP/DIR ignored"},
        {0x01, "DRIVE_OUTPUT_ENABLE",     true,  false, 0,             "",     0, 1,     Danger::None,        "0 = motor output off"},
        {0x02, "TARGET_SPEED",            true,  false, 1,             "rpm",  0, 3000,  Danger::None,        "max speed in position mode"},
        {0x03, "ACCELERATION",            true,  false, 0,             "",     0, 60098, Danger::None,        "<60000 = internal accel ramp"},
        {0x04, "WEAK_MAGNETIC_ANGLE",     true,  false, 0,             "",     0, 306,   Danger::Always,      "internal, do not change"},
        {0x05, "SPEED_LOOP_KP",           true,  false, 0.001f,        "",     0, 10000, Danger::None,        "ones digit odd/even = pulse edge"},
        {0x06, "SPEED_LOOP_INTEGRAL",     true,  false, 1,             "ms",   2, 2000,  Danger::None,        ""},
        {0x07, "POSITION_LOOP_KP",        true,  false, 0,             "",     60, 30000, Danger::None,       "ones digit odd/even = alarm NO/NC"},
        {0x08, "SPEED_FEED",              true,  false, 1.0f / 327,    "V/krpm", 0, 65535, Danger::Always,    "factory value, do not change"},
        {0x09, "DIR_POLARITY",            true,  false, 0,             "",     0, 1,     Danger::None,        ""},
        {0x0A, "GEAR_NUMERATOR",          true,  false, 0,             "",     0, 65535, Danger::WhenZero,    "0 = special/comm mode at power-up"},
        {0x0B, "STEPS_PER_REV",           true,  false, 0,             "",     1, 65535, Danger::None,        "gear denominator"},
        {0x0C, "TARGET_POS_LOW",          false, false, 0,             "",     0, 0,     Danger::None,        "FC 0x10 only"},
        {0x0D, "TARGET_POS_HIGH",         false, false, 0,             "",     0, 0,     Danger::None,        "FC 0x10 only"},
        {0x0E, "ALARM_CODE",              false, false, 0,             "",     0, 0,     Danger::None,        ""},
        {0x0F, "CURRENT",                 false, false, 1.0f / 2000,   "A",    0, 0,     Danger::None,        ""},
        {0x10, "SPEED",                   false, true,  0.1f,          "rpm",  0, 0,     Danger::None,        ""},
        {0x11, "VOLTAGE",                 false, false, 1.0f / 327,    "V",    0, 0,     Danger::None,        ""},
        {0x12, "TEMPERATURE",             false, false, 1,             "C",    0, 0,     Danger::None,        ""},
        {0x13, "PWM_OUTPUT",              false, true,  100.0f / 32768, "%",   0, 0,     Danger::None,        ""},
        {0x14, "SAVE_FLAG",               true,  false, 0,             "",     0, 2,     Danger::None,        "write 1 = save to EEPROM"},
        {0x15, "DEVICE_ADDRESS",          false, false, 0,             "",     0, 0,     Danger::None,        "change via FC 0x7A"},
        {0x16, "ABS_POS_LOW",             true,  false, 0,             "",     0, 65535, Danger::Always,      "low word of int32 position"},
        {0x17, "ABS_POS_HIGH",            true,  false, 0,             "",     0, 65535, Danger::Always,      "high word of int32 position"},
        {0x18, "MAX_OUTPUT",              true,  false, 0.1f,          "%",    0, 609,   Danger::None,        "ones digit = stall alarm mode"},
        {0x19, "SPECIAL_FUNCTION",        true,  false, 0,             "",     0, 32768, Danger::WhenNonZero, "non-zero = homing/other modes"},
    };
    // clang-format on

    inline const Register *find(uint16_t addr) {
        return addr < registerCount ? &registers[addr] : nullptr;
    }

    inline bool needsConfirmation(const Register &r, uint16_t value) {
        switch (r.danger) {
            case Danger::Always:
                return true;
            case Danger::WhenZero:
                return value == 0;
            case Danger::WhenNonZero:
                return value != 0;
            default:
                return false;
        }
    }

    inline const char *alarmText(uint16_t code) {
        switch (code) {
            case 0x00:
                return "none";
            case 0x10:
                return "battery off (warning only)";
            case 0x12:
                return "overflow (halted)";
            case 0x14:
                return "stall / blocked rotation (halted)";
            case 0x15:
                return "overvoltage (halted)";
            default:
                return "unknown";
        }
    }

}
