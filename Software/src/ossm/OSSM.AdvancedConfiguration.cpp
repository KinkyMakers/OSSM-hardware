#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "utils/analog.h"
#include "utils/format.h"
#include <EEPROM.h>

const int EEPROM_SIZE = 512; // EEPROM size in bytes
const int EEPROM_ADDRESS = 0x0100; // Prevent conflicts with other EEPROM data

SavedSettings convertToSavedSettings(const AdvancedConfigurationSettings& settings) {
    SavedSettings savedSettings;

    for (const auto& setting : settings.settings) {
        ESP_LOGD(
        "convertToSavedSettings",
        "Saving setting name: %d value: %f",
        setting.name, setting.currentValue());
        savedSettings.savedValues.push_back({
            .name = setting.name,
            .value = setting.currentValue()
        });
    }

    return savedSettings;
}

void OSSM::saveAdvancedSettings(AdvancedConfigurationSettings settings){
    //Minimize the object down to SavedSettings. Save to EEPROM
    SavedSettings settingsForSave = convertToSavedSettings(settings);

    ESP_LOGD(
        "saveAdvancedSettings",
        "EEPROM Length: %d",
        EEPROM.length());

    ESP_LOGD(
        "saveAdvancedSettings",
        "sizeof(settingsForSave): %d",
        sizeof(settingsForSave.savedValues));

    // TODO validate that data is less than 512 bytes and then save to EEPROM
    // if (sizeof(settingsForSave.savedValues) <= EEPROM_SIZE){
    //     EEPROM.begin(EEPROM_SIZE);
    //     EEPROM.put(EEPROM_ADDRESS, settingsForSave);
    //     EEPROM.commit();
    //     EEPROM.end();
    // }
}

AdvancedConfigurationSettings mapSavedSettings(const AdvancedConfigurationSettings& settings) {
    // TODO: Implement.

    SavedSettings settingsFromSave;
    // EEPROM.get(EEPROM_ADDRESS, settingsFromSave.savedValues);
    //     ESP_LOGD(
    //     "mapSavedSettings",
    //     "sizeof(settingsFromSave): %d",
    //     sizeof(settingsFromSave));

    // for (auto& setting : settings.settings) {
    //     for (const auto& savedSetting : settingsFromSave.savedValues) {
    //         if (setting.name == savedSetting.name) {
    //                 ESP_LOGD(
    //                 "mapSavedSettings",
    //                 "savedSetting.name: %d savedSetting.value: %f",
    //                 savedSetting.name, savedSetting.value);
    //             //setting.savedValue = savedSetting.value;
    //             break;
    //         }
    //     }
    // }

    return settings;
}

AdvancedConfigurationSettings OSSM::getAdvancedSettings(){
    AdvancedConfigurationSettings config;
    config.settings.push_back({
        .name = ReadMe,
        .type = DataType::INFO,
        .minValue = 0.0f,
        .maxValue = 0.0f,
        .settingPrecision = 0.0f,
        .defaultValue = 0.0f,
        .savedValue = std::nullopt
    });

    config.settings.push_back({
        .name = ToggleHomeDirection,
        .type = DataType::BOOL,
        .minValue = static_cast<float>(false),
        .maxValue = static_cast<float>(true),
        .settingPrecision = 1.0f,
        .defaultValue = static_cast<float>(false),
        .savedValue = std::nullopt
    });

    config.settings.push_back({
        .name = StepsPerRev,
        .type = DataType::NUMBER,
        .minValue = 800.0f,
        .maxValue = 51200.0f,
        .settingPrecision = 200.0f,
        .defaultValue = 800.0f,
        .savedValue = std::nullopt
    });

    config.settings.push_back({
        .name = PulleyTeeth,
        .type = DataType::NUMBER,
        .minValue = 8.0f,
        .maxValue = 32.0f,
        .settingPrecision = 1.0f,
        .defaultValue = 20.0f,
        .savedValue = std::nullopt
    });

    config.settings.push_back({
        .name = MaxSpeed,
        .type = DataType::NUMBER,
        .minValue = 0.0f,
        .maxValue = 2000.0f,
        .settingPrecision = 10.0f,
        .defaultValue = 900.0f,
        .savedValue = std::nullopt
    });

    config.settings.push_back({
        .name = RailLength,
        .type = DataType::NUMBER,
        .minValue = 50.0f,
        .maxValue = 1000.0f,
        .settingPrecision = 5.0f,
        .defaultValue = 350.0f,
        .savedValue = std::nullopt
    });

    config.settings.push_back({
        .name = RapidHoming,
        .type = DataType::BOOL,
        .minValue = static_cast<float>(false),
        .maxValue = static_cast<float>(true),
        .settingPrecision = 1.0f,
        .defaultValue = static_cast<float>(false),
        .savedValue = std::nullopt
    });

    config.settings.push_back({
        .name = HomingCurrentLimit,
        .type = DataType::NUMBER,
        .minValue = 1.0f,
        .maxValue = 20.0f,
        .settingPrecision = .05f,
        .defaultValue = 1.5f,
        .savedValue = std::nullopt
    });

    config.settings.push_back({
        .name = ResetToDefaults,
        .type = DataType::ACTION,
        .minValue = 0.0f,
        .maxValue = 0.0f,
        .settingPrecision = 0.0f,
        .defaultValue = 0.0f,
        .savedValue = std::nullopt
    });

    config.settings.push_back({
        .name = Apply,
        .type = DataType::ACTION,
        .minValue = 0.0f,
        .maxValue = 0.0f,
        .settingPrecision = 0.0f,
        .defaultValue = 0.0f,
        .savedValue = std::nullopt
    });

    // TODO: Retrieve savedValue from EEPROM and map
    config = mapSavedSettings(config);

    return config;
}

void OSSM::drawAdvancedConfigurationEditingTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;
    ossm->advancedConfigurationSettings = ossm->getAdvancedSettings();

    // Load in the single setting we're editing
    Setting currentAdvancedConfigurationSetting;
    for (const auto& setting : ossm->advancedConfigurationSettings.settings) {
        if ((int)setting.name == (int)ossm->activeAdvancedConfigurationSetting) {
            currentAdvancedConfigurationSetting = setting;
            break;
        }
    }
    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("advancedConfiguration.settings.edit.editing"_s);
    };
    bool isInActionState = currentAdvancedConfigurationSetting.type == DataType::ACTION;
    bool isInInfoState = currentAdvancedConfigurationSetting.type == DataType::INFO;

    bool shouldUpdateDisplay = true;
    bool actionPending = true;
    String settingName =
        UserConfig::language.AdvancedConfigurationSettingNames[(int)currentAdvancedConfigurationSetting.name];

    static float valueForStorage = 0;
    static float valueToEncoderClicks = 0;

    ossm->encoder.setAcceleration(10);

    // Initialize the edit view for the current setting
    ossm->encoder.setBoundaries(currentAdvancedConfigurationSetting.minValue / currentAdvancedConfigurationSetting.settingPrecision,
        currentAdvancedConfigurationSetting.maxValue / currentAdvancedConfigurationSetting.settingPrecision,
        currentAdvancedConfigurationSetting.type == DataType::BOOL);
    ossm->encoder.setEncoderValue((currentAdvancedConfigurationSetting.currentValue() / currentAdvancedConfigurationSetting.settingPrecision) - 1);

    while (isInCorrectState(ossm) && actionPending) {

        if(isInActionState) {
            // Perform the action
            switch (currentAdvancedConfigurationSetting.name) {
                case ResetToDefaults:
                    // Reset the setting to the default value
                    displayMutex.lock();
                    ossm->display.clearBuffer();
                    //Draw the screen
                    drawStr::title(settingName);
                    drawStr::multiLine(0, 32, "Nothing to see here.. \nShort press to go back.");
                    ossm->display.sendBuffer();
                    displayMutex.unlock();
                    break;
                case Apply:
                    // Save the setting to EEPROM
                    displayMutex.lock();
                    ossm->display.clearBuffer();
                    //Draw the screen
                    drawStr::title(settingName);
                    drawStr::multiLine(0, 32, "Saving settings... \nRestarting OSSM...");
                    ossm->display.sendBuffer();
                    displayMutex.unlock();

                    saveAdvancedSettings(ossm->advancedConfigurationSettings);

                    break;
                default:
                    break;
            }
            actionPending = false;
        }
        else if (isInInfoState){
            // Do nothing, tell the user they should click to return.
            displayMutex.lock();
            ossm->display.clearBuffer();
            //Draw the screen
            drawStr::title(settingName);
            drawStr::multiLine(0, 32, "Nothing to see here.. \nShort press to go back.");
            ossm->display.sendBuffer();
            displayMutex.unlock();

            actionPending = false;
        }

        else {
            shouldUpdateDisplay =
                shouldUpdateDisplay || valueToEncoderClicks != ossm->encoder.readEncoder();

            valueForStorage = (ossm->encoder.readEncoder()) * currentAdvancedConfigurationSetting.settingPrecision;
            valueToEncoderClicks = ossm->encoder.readEncoder();

            if (!shouldUpdateDisplay) {
                vTaskDelay(100);
                continue;
            }

            // TODO: Apply value change back to the shared object, to later be saved.
            // ossm->advancedConfigurationSettings = ...

            displayMutex.lock();
            ossm->display.clearBuffer();

            String defaultValueString = "  Default value: " +
                (currentAdvancedConfigurationSetting.type == DataType::BOOL ?
                    (currentAdvancedConfigurationSetting.defaultValue == 0 ? "False" : "True") :
                    String(currentAdvancedConfigurationSetting.defaultValue, currentAdvancedConfigurationSetting.settingPrecision < 1 ? 2 : 0));
            String currentValueString = "  Value: " +
                (currentAdvancedConfigurationSetting.type == DataType::BOOL ?
                    (valueForStorage == 0 ? "False" : "True") :
                    String(valueForStorage, currentAdvancedConfigurationSetting.settingPrecision < 1 ? 2 : 0));

            //Draw the screen
            drawStr::title(settingName);
            drawStr::multiLine(0, 32, defaultValueString.c_str());
            drawStr::multiLine(0, 44, currentValueString.c_str());
            drawShape::settingBar("", valueToEncoderClicks, 128, 0, RIGHT_ALIGNED, 0,
                0,
                currentAdvancedConfigurationSetting.maxValue / currentAdvancedConfigurationSetting.settingPrecision);

            ossm->display.sendBuffer();
            displayMutex.unlock();
        }

        vTaskDelay(25); //8x faster than the normal menu, for improved feel of responsiveness when scrolling to input large values.
    }

    vTaskDelete(nullptr);
};

void OSSM::drawAdvancedConfigurationMenuTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;

    ossm->advancedConfigurationSettings = ossm->getAdvancedSettings();

    size_t numberOfSettings = ossm->advancedConfigurationSettings.settings.size();

    auto isInCorrectState = [](OSSM *ossm) {
        return ossm->sm->is("advancedConfiguration.settings"_s);
    };

    static float editEncoder = 0;
    static long encoderValue = 0;
    static float editEncoderOffset = 0;
    static float minValue = 0;
    static float maxValue = 100;
    static bool editState = false;
    int currentSelection = 0;
    int nextSelection = 0;
    bool shouldUpdateDisplay = true;
    String settingName = "nextSelection";
    String settingDescription =
        UserConfig::language.AdvancedConfigurationSettingDescriptions[nextSelection];

    ossm->encoder.setAcceleration(10);
    ossm->encoder.setBoundaries(0, numberOfSettings * 3 - 1, true);
    ossm->encoder.setEncoderValue(nextSelection * 3);

    while (isInCorrectState(ossm)) {
        nextSelection = ossm->encoder.readEncoder() / 3;
        shouldUpdateDisplay =
            shouldUpdateDisplay || currentSelection != nextSelection;
        if (!shouldUpdateDisplay) {
            vTaskDelay(100);
            continue;
        }
        ossm->activeAdvancedConfigurationSetting = (AdvancedConfigurationSettingName)nextSelection;

        settingName = UserConfig::language.AdvancedConfigurationSettingNames[nextSelection].c_str();

        if (nextSelection < numberOfSettings) {
            settingDescription =
                UserConfig::language.AdvancedConfigurationSettingDescriptions[nextSelection];
        } else {
            settingDescription = "No description available";
        }

        currentSelection = nextSelection;

        displayMutex.lock();
        ossm->display.clearBuffer();

        // Draw the title
        drawStr::title(settingName);
        drawStr::multiLine(0, 20, settingDescription);
        drawShape::scroll(100 * nextSelection / numberOfSettings);

        ossm->display.sendBuffer();
        displayMutex.unlock();


        vTaskDelay(200);
    }

    vTaskDelete(nullptr);
};

void OSSM::drawAdvancedConfiguration() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawAdvancedConfigurationMenuTask, "drawAdvancedConfigurationMenuTask", stackSize,
                this, 1, &drawAdvancedConfigurationMenuTaskH);
}

void OSSM::drawAdvancedConfigurationEditing() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawAdvancedConfigurationEditingTask, "drawAdvancedConfigurationEditingTask", stackSize,
                this, 1, &drawAdvancedConfigurationEditingTaskH);
}

