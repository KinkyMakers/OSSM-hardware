#include "serial_console.h"

#include <cstring>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <RadBle.h>

#include "ossm/OSSM.h"
#include "serial_command_logic.h"
#include "services/communication/rad_ble.h"
#include "services/device_config.h"
#include "services/tasks.h"
#include "settings_catalog.h"

namespace {

constexpr const char *TAG = "CONSOLE";
constexpr size_t kLineCapacity = 512;

String currentStateName() {
    if (ossm == nullptr) return "";
    JsonDocument current;
    if (deserializeJson(current, ossm->getCurrentState())) return "";
    return current["state"] | "";
}

void replyOk(const char *text) {
    if (text == nullptr || text[0] == '\0')
        Serial.println("ok");
    else
        Serial.printf("ok %s\n", text);
}

void replyErr(const char *code, const char *message) {
    Serial.printf("err %s: %s\n", code, message);
}

void replyResult(const radble::Result &result) {
    if (result.ok) {
        if (result.resultJson.length() == 0 || result.resultJson == "{}")
            replyOk("");
        else
            Serial.printf("ok %s\n", result.resultJson.c_str());
        return;
    }
    replyErr(result.code.c_str(), result.message.c_str());
}

radble::Result dispatchSettingRead(const char *path) {
    JsonDocument request;
    request["op"] = "setting.read";
    request["path"] = path;
    return dispatchRadCommand(request.as<JsonObjectConst>());
}

radble::Result dispatchSettingWrite(const char *path, JsonVariantConst value) {
    JsonDocument request;
    request["op"] = "setting.write";
    request["path"] = path;
    request["args"]["value"] = value;
    return dispatchRadCommand(request.as<JsonObjectConst>());
}

void printHelp() {
    Serial.println("ok USB settings console. Commands:");
    Serial.println("  help | get [id] | info [id] | set <id> <value>");
    Serial.println("  name <value> | restart");
    Serial.println("  {JSON}  raw RAD BLE request (no control lease)");
    Serial.println("id               type    units     range                    apply");
    for (const auto &spec : settings_catalog::kSettings) {
        Serial.printf("  %-16s %-7s %-9s %-24s %s\n", spec.id, spec.valueType,
                      spec.units[0] == '\0' ? "-" : spec.units, spec.rangeText,
                      settings_catalog::applyWhenText(spec.apply));
    }
    Serial.printf("Geometry: %s\n", settings_catalog::kGeometryRules);
    Serial.println("Details: Software/docs/settings.md");
}

void printInfo(const settings_catalog::SettingSpec &spec) {
    Serial.printf("ok %s\n", spec.id);
    Serial.printf("  path: %s\n", spec.path);
    Serial.printf("  type: %s  units: %s  range: %s\n", spec.valueType,
                  spec.units[0] == '\0' ? "-" : spec.units, spec.rangeText);
    Serial.printf("  apply: %s  persist: %s  default: %s\n",
                  settings_catalog::applyWhenText(spec.apply),
                  spec.persist ? "yes" : "no", spec.defaultText);
    if (spec.constraintsJson[0] != '\0')
        Serial.printf("  constraints: %s\n", spec.constraintsJson);
    Serial.printf("  %s\n", spec.description);
    if (std::strncmp(spec.path, "setting.machine.", 16) == 0)
        Serial.printf("  %s\n", settings_catalog::kGeometryRules);
}

void printOne(const settings_catalog::SettingSpec &spec) {
    if (std::strcmp(spec.path, "device.name") == 0) {
        Serial.printf("ok %s=%s  [%s]\n", spec.id,
                      radble::loadDeviceName("OSSM").c_str(), spec.rangeText);
        return;
    }
    const radble::Result result = dispatchSettingRead(spec.path);
    if (!result.ok) {
        replyResult(result);
        return;
    }
    Serial.printf("ok %s %s  %s [%s]\n", spec.id, result.resultJson.c_str(),
                  spec.units[0] == '\0' ? "-" : spec.units, spec.rangeText);
}

void setSetting(const settings_catalog::SettingSpec &spec, const char *text) {
    if (std::strcmp(spec.path, "device.name") == 0) {
        if (!settings_catalog::isValidDeviceName(text)) {
            replyErr("invalid_value", spec.rangeText);
            return;
        }
        if (!radble::persistDeviceName(text)) {
            replyErr("storage_failed", "Could not save the device name");
            return;
        }
        Serial.printf("ok device_name=%s (BLE name after restart)\n", text);
        return;
    }

    JsonDocument value;
    if (spec.kind == settings_catalog::ValueKind::Bool) {
        bool flag = false;
        if (!serial_command_logic::parseBool(text, flag)) {
            replyErr("invalid_value", "Expected true or false");
            return;
        }
        value.set(flag);
    } else {
        float number = 0;
        if (!serial_command_logic::parseNumber(text, number)) {
            replyErr("invalid_value", "Expected a number");
            return;
        }
        if (spec.kind == settings_catalog::ValueKind::Integer &&
            !device_config_logic::isWholeNumber(number)) {
            replyErr("invalid_value", "Expected a whole number");
            return;
        }
        value.set(number);
    }
    replyResult(dispatchSettingWrite(spec.path, value.as<JsonVariantConst>()));
}

void handleName(const char *text) {
    String name = text == nullptr ? "" : text;
    name.trim();
    if (name.isEmpty()) name = "OSSM";
    if (!settings_catalog::isValidDeviceName(name.c_str())) {
        replyErr("invalid_value", settings_catalog::kDeviceName.rangeText);
        return;
    }
    if (!radble::persistDeviceName(name)) {
        replyErr("storage_failed", "Could not save the device name");
        return;
    }
    Serial.printf("ok device_name=%s (BLE name after restart)\n", name.c_str());
}

void handleRestart() {
    const String state = currentStateName();
    if (!serial_command_logic::serialRestartAllowed(state.c_str())) {
        replyErr("invalid_state",
                 "Restart is blocked while homing or a motion mode is running");
        return;
    }
    replyOk("restarting");
    Serial.flush();
    delay(100);
    ESP.restart();
}

void handleJson(const char *line) {
    JsonDocument request;
    const DeserializationError error = deserializeJson(request, line);
    if (error) {
        replyErr("invalid_value", "JSON could not be parsed");
        return;
    }
    replyResult(dispatchRadCommand(request.as<JsonObjectConst>()));
}

void handleLine(const char *line) {
    const auto parsed = serial_command_logic::parseLine(line);
    switch (parsed.verb) {
        case serial_command_logic::Verb::Empty:
            return;
        case serial_command_logic::Verb::Help:
            printHelp();
            return;
        case serial_command_logic::Verb::Get:
            if (!parsed.hasKey) {
                for (const auto &spec : settings_catalog::kSettings)
                    printOne(spec);
                if (device_config::restartRequired())
                    Serial.println("ok restartRequired=true");
                return;
            }
            if (const auto *spec = settings_catalog::findById(parsed.key)) {
                printOne(*spec);
                return;
            }
            replyErr("unknown_path", "Unknown setting id");
            return;
        case serial_command_logic::Verb::Info:
            if (!parsed.hasKey) {
                printHelp();
                return;
            }
            if (const auto *spec = settings_catalog::findById(parsed.key)) {
                printInfo(*spec);
                return;
            }
            replyErr("unknown_path", "Unknown setting id");
            return;
        case serial_command_logic::Verb::Set:
            if (!parsed.hasKey || !parsed.hasValue) {
                replyErr("invalid_value", "Usage: set <id> <value>");
                return;
            }
            if (const auto *spec = settings_catalog::findById(parsed.key)) {
                setSetting(*spec, parsed.value);
                return;
            }
            replyErr("unknown_path", "Unknown setting id");
            return;
        case serial_command_logic::Verb::Name:
            handleName(parsed.value);
            return;
        case serial_command_logic::Verb::Restart:
            handleRestart();
            return;
        case serial_command_logic::Verb::Json:
            handleJson(parsed.value);
            return;
        case serial_command_logic::Verb::Unknown:
            replyErr("unsupported", "Unknown command; type help");
            return;
    }
}

void serialConsoleTask(void *) {
    char line[kLineCapacity];
    size_t length = 0;
    ESP_LOGI(TAG, "USB settings console ready; type help");
    Serial.println("ok console ready; type help");
    while (true) {
        while (Serial.available() > 0) {
            const int incoming = Serial.read();
            if (incoming < 0) break;
            if (incoming == '\r') continue;
            if (incoming == '\n') {
                line[length] = '\0';
                handleLine(line);
                length = 0;
                continue;
            }
            if (length + 1 >= kLineCapacity) {
                length = 0;
                replyErr("invalid_value", "Line too long");
                continue;
            }
            line[length++] = static_cast<char>(incoming);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace

void startSerialConsole() {
    xTaskCreatePinnedToCore(serialConsoleTask, "serialConsole", 8192, nullptr,
                            1, nullptr, Tasks::operationTaskCore);
}
