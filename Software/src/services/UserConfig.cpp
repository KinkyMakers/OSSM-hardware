#include "UserConfig.h"

#include <Preferences.h>

namespace UserConfig {
    std::string getDeviceName() {
        Preferences userConfig;
        userConfig.begin("UserConfig", true);
        std::string output = userConfig.getString("DeviceName","OSSM").c_str();
        userConfig.end();
        ESP_LOGD("USER CONFIG", "Name read: %s", output.c_str());
        return output;
    }

    void setDeviceName(String value) {
        Preferences userConfig;
        userConfig.begin("UserConfig", false);
        userConfig.putString("DeviceName", value);
        userConfig.end();
        ESP_LOGI("USER CONFIG", "Rename write: %s", value.c_str());
        ESP.restart();
    }

    bool getDirection() {
        Preferences userConfig;
        userConfig.begin("UserConfig", true);
        bool output = userConfig.getBool("Direction",false);
        userConfig.end();
        ESP_LOGI("USER CONFIG", "Direction read: %d", output);
        return output;
    }

    void setDirection(bool value) {
        Preferences userConfig;
        userConfig.begin("UserConfig", false);
        userConfig.putBool("Direction", value);
        userConfig.end();
        ESP_LOGI("USER CONFIG", "Direction write: %d", value);
        ESP.restart();
    }
}
