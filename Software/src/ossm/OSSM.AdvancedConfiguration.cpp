#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "utils/analog.h"
#include "utils/format.h"
#include "nvs_flash.h"
#include <Preferences.h>

#define STORAGE_NAMESPACE "AdvancedConfigurationSettings"
Preferences advancedConfigurationPrefs;


void saveSettings(AdvancedConfigurationSettings& settings) {
    advancedConfigurationPrefs.begin("ACP", false);
    bool tpInit = advancedConfigurationPrefs.isKey("nvsInit");
    String settingNameStr = "";
    if (tpInit == false) {
        advancedConfigurationPrefs.putBool("nvsInit", true);
    }
    for (auto& setting : settings.settings) {
        settingNameStr = getSettingName(setting.name).c_str();
        if(settingNameStr != ""){
            ESP_LOGD("saveSettings", "Adding setting %s with value %f", settingNameStr.c_str(), setting.currentValue());
            advancedConfigurationPrefs.putFloat(settingNameStr.c_str(), setting.savedValue.value_or(setting.defaultValue));
            ESP_LOGD("getSavedValue", "Found setting %s with value %f", settingNameStr.c_str(), advancedConfigurationPrefs.getFloat(settingNameStr.c_str(), -1.0f));
        }
    }
    advancedConfigurationPrefs.end();
}

std::optional<float> getSavedValue(AdvancedConfigurationSettingName settingName) {
    advancedConfigurationPrefs.begin("ACP", true);
    float returnValue = -1.0f;
    bool tpInit = advancedConfigurationPrefs.isKey("nvsInit");
    if (tpInit == false) {
        ESP_LOGD("getSavedValue", "No settings found in NVS");
        return std::nullopt;
    }
    String settingNameStr = getSettingName(settingName).c_str();
    if(settingNameStr != "" && advancedConfigurationPrefs.isKey(settingNameStr.c_str())){
        ESP_LOGD("getSavedValue", "Found setting %s with value %f", settingNameStr.c_str(), advancedConfigurationPrefs.getFloat(settingNameStr.c_str(), returnValue));
        returnValue = advancedConfigurationPrefs.getFloat(settingNameStr.c_str(), returnValue);
    }
    advancedConfigurationPrefs.end();

    return returnValue < 0 ? std::nullopt : std::make_optional(returnValue);
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
        .savedValue = getSavedValue(ToggleHomeDirection)
    });

    config.settings.push_back({
        .name = StepsPerRev,
        .type = DataType::NUMBER,
        .minValue = 800.0f,
        .maxValue = 51200.0f,
        .settingPrecision = 200.0f,
        .defaultValue = 800.0f,
        .savedValue = getSavedValue(StepsPerRev)
    });

    config.settings.push_back({
        .name = PulleyTeeth,
        .type = DataType::NUMBER,
        .minValue = 8.0f,
        .maxValue = 32.0f,
        .settingPrecision = 1.0f,
        .defaultValue = 20.0f,
        .savedValue = getSavedValue(PulleyTeeth)
    });

    config.settings.push_back({
        .name = MaxSpeed,
        .type = DataType::NUMBER,
        .minValue = 0.0f,
        .maxValue = 2000.0f,
        .settingPrecision = 10.0f,
        .defaultValue = 900.0f,
        .savedValue = getSavedValue(MaxSpeed)
    });

    config.settings.push_back({
        .name = RailLength,
        .type = DataType::NUMBER,
        .minValue = 50.0f,
        .maxValue = 1000.0f,
        .settingPrecision = 5.0f,
        .defaultValue = 350.0f,
        .savedValue = getSavedValue(RailLength)
    });

    config.settings.push_back({
        .name = RapidHoming,
        .type = DataType::BOOL,
        .minValue = static_cast<float>(false),
        .maxValue = static_cast<float>(true),
        .settingPrecision = 1.0f,
        .defaultValue = static_cast<float>(false),
        .savedValue = getSavedValue(RapidHoming)
    });

    config.settings.push_back({
        .name = HomingCurrentLimit,
        .type = DataType::NUMBER,
        .minValue = 1.0f,
        .maxValue = 20.0f,
        .settingPrecision = .05f,
        .defaultValue = 1.5f,
        .savedValue = getSavedValue(HomingCurrentLimit)
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

    return config;
}

void OSSM::drawAdvancedConfigurationEditingTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;

    // Initialize the settings if they haven't been loaded yet
    // TODO: Remove this once we are loading on boot.
    if (ossm->advancedConfigurationSettings.settings.empty()) {
        ossm->advancedConfigurationSettings = ossm->getAdvancedSettings();
    }

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
                    drawStr::multiLine(0, 32, "Applying default settings... \nRestarting OSSM...");
                    ossm->display.sendBuffer();
                    displayMutex.unlock();

                    // TODO: Reset the settings to their default values
                    for (auto& setting : ossm->advancedConfigurationSettings.settings) {
                        setting.savedValue = std::nullopt;
                    }
                    saveSettings(ossm->advancedConfigurationSettings);
                    vTaskDelay(1000);
                    ESP.restart();

                    break;
                case Apply:
                    // Save the setting to Flash
                    displayMutex.lock();
                    ossm->display.clearBuffer();
                    //Draw the screen
                    drawStr::title(settingName);
                    drawStr::multiLine(0, 32, "Saving settings... \nRestarting OSSM...");
                    ossm->display.sendBuffer();
                    displayMutex.unlock();

                    saveSettings(ossm->advancedConfigurationSettings);

                    vTaskDelay(1000);
                    ESP.restart();

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

            // Search through ossm->advancedConfigurationSettings for the setting we're editing and update it
            for (auto& setting : ossm->advancedConfigurationSettings.settings) {
                if ((int)setting.name == (int)ossm->activeAdvancedConfigurationSetting) {
                    setting.savedValue = valueForStorage;
                    break;
                }
            }

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

    if (ossm->advancedConfigurationSettings.settings.empty()) {
        ossm->advancedConfigurationSettings = ossm->getAdvancedSettings();
    }

    size_t numberOfSettings = ossm->advancedConfigurationSettings.settings.size();

    auto isInCorrectState = [](OSSM *ossm) {
        return ossm->sm->is("advancedConfiguration.settings"_s);
    };

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

