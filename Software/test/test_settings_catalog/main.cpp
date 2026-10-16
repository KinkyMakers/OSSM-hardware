#include <cstring>
#include <unity.h>
#include "device_config_logic.h"
#include "serial_command_logic.h"
#include "settings_catalog.h"

using namespace settings_catalog;
using namespace serial_command_logic;
using namespace device_config_logic;

void setUp(void) {}
void tearDown(void) {}

void test_ids_and_paths_are_unique(void) {
    for (size_t i = 0; i < kSettingCount; ++i) {
        TEST_ASSERT_NOT_NULL(findById(kSettings[i].id));
        TEST_ASSERT_NOT_NULL(findByPath(kSettings[i].path));
        for (size_t j = i + 1; j < kSettingCount; ++j) {
            TEST_ASSERT_NOT_EQUAL(0, strcmp(kSettings[i].id, kSettings[j].id));
            TEST_ASSERT_NOT_EQUAL(
                0, strcmp(kSettings[i].path, kSettings[j].path));
        }
    }
}

void test_catalog_defaults_match_machine_config(void) {
    MachineConfig defaults;
    MachineConfig fromCatalog = defaultMachineConfig();
    TEST_ASSERT_EQUAL_FLOAT(defaults.stepsPerRev, fromCatalog.stepsPerRev);
    TEST_ASSERT_EQUAL_FLOAT(defaults.pulleyToothCount,
                            fromCatalog.pulleyToothCount);
    TEST_ASSERT_EQUAL_FLOAT(defaults.beltPitchMm, fromCatalog.beltPitchMm);
    TEST_ASSERT_EQUAL_FLOAT(defaults.maxRpm, fromCatalog.maxRpm);
    TEST_ASSERT_EQUAL_FLOAT(defaults.maxStrokeMm, fromCatalog.maxStrokeMm);
    TEST_ASSERT_TRUE(validateGeometry(fromCatalog, kMaxAccelerationMmPerS2) ==
                     GeometryError::None);
}

void test_catalog_numbers_match_validation(void) {
    TEST_ASSERT_TRUE(isValidAfterHomingPosition(kHomePosition.defaultNumber));
    TEST_ASSERT_TRUE(isValidMqttPublishHz(kMqttRate.defaultNumber));
    TEST_ASSERT_EQUAL_FLOAT(kMinStrokeLengthMm, 50.0f);
    TEST_ASSERT_EQUAL_FLOAT(kMaxStepRateHz, 100000.0f);
    TEST_ASSERT_NOT_NULL(strstr(kMaxStroke.constraintsJson, "\"exclusiveMin\":50"));
    TEST_ASSERT_NOT_NULL(
        strstr(kMaxRpm.constraintsJson, "\"maxStepRateHz\":100000"));
    TEST_ASSERT_NOT_NULL(strstr(kHomePosition.constraintsJson, "\"min\":0"));
    TEST_ASSERT_NOT_NULL(strstr(kHomePosition.constraintsJson, "\"max\":1"));
    TEST_ASSERT_NOT_NULL(strstr(kStepsPerRev.constraintsJson, "\"integer\":true"));
    TEST_ASSERT_NOT_NULL(strstr(kPulleyTeeth.constraintsJson, "\"integer\":true"));
}

void test_fractional_steps_per_rev_is_rejected(void) {
    MachineConfig config;
    config.stepsPerRev = 800.5f;
    TEST_ASSERT_TRUE(validateGeometry(config, kMaxAccelerationMmPerS2) ==
                     GeometryError::NotWholeNumber);
}

void test_fractional_pulley_teeth_is_rejected(void) {
    MachineConfig config;
    config.pulleyToothCount = 20.5f;
    TEST_ASSERT_TRUE(validateGeometry(config, kMaxAccelerationMmPerS2) ==
                     GeometryError::NotWholeNumber);
}

void test_device_name_rules(void) {
    TEST_ASSERT_TRUE(isValidDeviceName("OSSM"));
    TEST_ASSERT_TRUE(isValidDeviceName("Kitchen-OSSM_1"));
    TEST_ASSERT_FALSE(isValidDeviceName(""));
    TEST_ASSERT_FALSE(isValidDeviceName("bad/name"));
    char tooLong[kDeviceNameMaxBytes + 2];
    memset(tooLong, 'A', kDeviceNameMaxBytes + 1);
    tooLong[kDeviceNameMaxBytes + 1] = '\0';
    TEST_ASSERT_FALSE(isValidDeviceName(tooLong));
}

void test_parse_help_and_empty(void) {
    TEST_ASSERT_EQUAL(static_cast<int>(Verb::Empty),
                      static_cast<int>(parseLine("").verb));
    TEST_ASSERT_EQUAL(static_cast<int>(Verb::Help),
                      static_cast<int>(parseLine("  HELP  ").verb));
}

void test_parse_get_and_info(void) {
    ParsedCommand getAll = parseLine("get");
    TEST_ASSERT_EQUAL(static_cast<int>(Verb::Get),
                      static_cast<int>(getAll.verb));
    TEST_ASSERT_FALSE(getAll.hasKey);

    ParsedCommand getOne = parseLine("get steps_per_rev");
    TEST_ASSERT_TRUE(getOne.hasKey);
    TEST_ASSERT_EQUAL_STRING("steps_per_rev", getOne.key);

    ParsedCommand info = parseLine("info home_position");
    TEST_ASSERT_EQUAL(static_cast<int>(Verb::Info),
                      static_cast<int>(info.verb));
    TEST_ASSERT_EQUAL_STRING("home_position", info.key);
}

void test_parse_set_and_quoted_name(void) {
    ParsedCommand setCmd = parseLine("set home_position 0.5");
    TEST_ASSERT_EQUAL(static_cast<int>(Verb::Set),
                      static_cast<int>(setCmd.verb));
    TEST_ASSERT_EQUAL_STRING("home_position", setCmd.key);
    TEST_ASSERT_EQUAL_STRING("0.5", setCmd.value);

    ParsedCommand named = parseLine("name \"Kitchen OSSM\"");
    TEST_ASSERT_EQUAL(static_cast<int>(Verb::Name),
                      static_cast<int>(named.verb));
    TEST_ASSERT_EQUAL_STRING("Kitchen OSSM", named.value);

    ParsedCommand missing = parseLine("set metric");
    TEST_ASSERT_TRUE(missing.hasKey);
    TEST_ASSERT_FALSE(missing.hasValue);
}

void test_parse_json(void) {
    ParsedCommand json = parseLine(
        "{\"op\":\"setting.read\",\"path\":\"setting.machine.maxRpm\"}");
    TEST_ASSERT_EQUAL(static_cast<int>(Verb::Json),
                      static_cast<int>(json.verb));
    TEST_ASSERT_TRUE(json.value[0] == '{');
}

void test_parse_bool_and_number(void) {
    bool flag = false;
    TEST_ASSERT_TRUE(parseBool("true", flag));
    TEST_ASSERT_TRUE(flag);
    TEST_ASSERT_TRUE(parseBool("0", flag));
    TEST_ASSERT_FALSE(flag);
    TEST_ASSERT_FALSE(parseBool("maybe", flag));

    float value = 0;
    TEST_ASSERT_TRUE(parseNumber("800", value));
    TEST_ASSERT_EQUAL_FLOAT(800.0f, value);
    TEST_ASSERT_TRUE(parseNumber("0.5", value));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, value);
    TEST_ASSERT_FALSE(parseNumber("800abc", value));
}

void test_restart_allowed_states(void) {
    TEST_ASSERT_TRUE(serialRestartAllowed("menu.idle"));
    TEST_ASSERT_TRUE(serialRestartAllowed("help.idle"));
    TEST_ASSERT_TRUE(serialRestartAllowed("error.idle"));
    TEST_ASSERT_FALSE(serialRestartAllowed("homing.forward"));
    TEST_ASSERT_FALSE(serialRestartAllowed("simplePenetration.idle"));
    TEST_ASSERT_FALSE(serialRestartAllowed("strokeEngine.pattern"));
    TEST_ASSERT_FALSE(serialRestartAllowed("streaming.preflight"));
    TEST_ASSERT_FALSE(serialRestartAllowed(""));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ids_and_paths_are_unique);
    RUN_TEST(test_catalog_defaults_match_machine_config);
    RUN_TEST(test_catalog_numbers_match_validation);
    RUN_TEST(test_fractional_steps_per_rev_is_rejected);
    RUN_TEST(test_fractional_pulley_teeth_is_rejected);
    RUN_TEST(test_device_name_rules);
    RUN_TEST(test_parse_help_and_empty);
    RUN_TEST(test_parse_get_and_info);
    RUN_TEST(test_parse_set_and_quoted_name);
    RUN_TEST(test_parse_json);
    RUN_TEST(test_parse_bool_and_number);
    RUN_TEST(test_restart_allowed_states);
    return UNITY_END();
}
