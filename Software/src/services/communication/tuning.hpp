#ifndef OSSM_COMMUNICATION_STREAM_TUNING_HPP
#define OSSM_COMMUNICATION_STREAM_TUNING_HPP

#ifdef OSSM_STREAM_TUNING

#include <ArduinoJson.h>
#include <NimBLECharacteristic.h>
#include <NimBLEService.h>
#include <NimBLEUUID.h>

#include <array>
#include <string>

#include "ossm/streaming/streaming.h"

namespace stream_tuning_ble {

    constexpr uint32_t kSchemaVersion = 2;

    inline void writeParameters(
        JsonObject object,
        const timed_streaming::TuningParameters &parameters) {
        object["jerkRampMilliseconds"] =
            parameters.jerkRampMilliseconds;
        object["primeMilliseconds"] = parameters.primeMilliseconds;
        object["executionHorizonMilliseconds"] =
            parameters.executionHorizonMilliseconds;
        object["accelerationScale"] = parameters.accelerationScale;
        object["momentumDecayMilliseconds"] =
            parameters.momentumDecayMilliseconds;
        object["maximumCoastFraction"] = parameters.maximumCoastFraction;
        object["edgeRepulsionStrength"] =
            parameters.edgeRepulsionStrength;
        object["centerSpringStrength"] =
            parameters.centerSpringStrength;
    }

    inline String responseJSON(streaming::TuningApplyStatus status) {
        const auto snapshot = streaming::tuningSnapshot();
        JsonDocument response;
        response["schemaVersion"] = kSchemaVersion;
        response["ok"] = status == streaming::TuningApplyStatus::Applied;
        response["supported"] = snapshot.supported;
        response["status"] = streaming::tuningApplyStatusName(status);
        response["revision"] = snapshot.revision;
        char hash[17]{};
        snprintf(hash, sizeof(hash), "%016llx",
                 static_cast<unsigned long long>(snapshot.hash));
        response["hash"] = hash;
        writeParameters(response["parameters"].to<JsonObject>(),
                        snapshot.parameters);
        String encoded;
        serializeJson(response, encoded);
        return encoded;
    }

    inline bool hasExactParameterKeys(JsonObjectConst object) {
        static constexpr std::array<const char *, 9> keys = {
            "schemaVersion",
            "jerkRampMilliseconds",
            "primeMilliseconds",
            "executionHorizonMilliseconds",
            "accelerationScale",
            "momentumDecayMilliseconds",
            "maximumCoastFraction",
            "edgeRepulsionStrength",
            "centerSpringStrength",
        };
        if (object.size() != keys.size()) return false;
        for (const char *key : keys)
            if (!object[key].is<JsonVariantConst>()) return false;
        return true;
    }

    inline bool decodeParameters(
        const std::string &value,
        timed_streaming::TuningParameters &parameters) {
        JsonDocument document;
        const auto error = deserializeJson(document, value);
        if (error || !document.is<JsonObject>()) return false;
        const JsonObjectConst object = document.as<JsonObjectConst>();
        if (!hasExactParameterKeys(object) ||
            object["schemaVersion"].as<uint32_t>() != kSchemaVersion)
            return false;
        if (!object["jerkRampMilliseconds"].is<uint32_t>() ||
            !object["primeMilliseconds"].is<uint32_t>() ||
            !object["executionHorizonMilliseconds"].is<uint32_t>() ||
            !object["accelerationScale"].is<double>() ||
            !object["momentumDecayMilliseconds"].is<uint32_t>() ||
            !object["maximumCoastFraction"].is<double>() ||
            !object["edgeRepulsionStrength"].is<double>() ||
            !object["centerSpringStrength"].is<double>())
            return false;
        parameters.jerkRampMilliseconds =
            object["jerkRampMilliseconds"].as<uint32_t>();
        parameters.primeMilliseconds =
            object["primeMilliseconds"].as<uint32_t>();
        parameters.executionHorizonMilliseconds =
            object["executionHorizonMilliseconds"].as<uint32_t>();
        parameters.accelerationScale =
            object["accelerationScale"].as<double>();
        parameters.momentumDecayMilliseconds =
            object["momentumDecayMilliseconds"].as<uint32_t>();
        parameters.maximumCoastFraction =
            object["maximumCoastFraction"].as<double>();
        parameters.edgeRepulsionStrength =
            object["edgeRepulsionStrength"].as<double>();
        parameters.centerSpringStrength =
            object["centerSpringStrength"].as<double>();
        return timed_streaming::validateTuningParameters(parameters) ==
               timed_streaming::TuningValidationError::None;
    }

    inline bool isResetRequest(const std::string &value) {
        JsonDocument document;
        if (deserializeJson(document, value) ||
            !document.is<JsonObject>())
            return false;
        const JsonObjectConst object = document.as<JsonObjectConst>();
        return object.size() == 2 &&
               object["schemaVersion"].as<uint32_t>() == kSchemaVersion &&
               object["reset"].is<bool>() && object["reset"].as<bool>();
    }

    class Callbacks : public NimBLECharacteristicCallbacks {
      public:
        void onWrite(NimBLECharacteristic *characteristic,
                     NimBLEConnInfo &) override {
            const std::string value = characteristic->getValue();
            timed_streaming::TuningParameters requested{};
            if (isResetRequest(value)) {
                lastStatus_ =
                    streaming::applyTuningParameters(requested);
            } else if (!decodeParameters(value, requested)) {
                lastStatus_ = streaming::TuningApplyStatus::Invalid;
            } else {
                lastStatus_ =
                    streaming::applyTuningParameters(requested);
            }
            characteristic->setValue(responseJSON(lastStatus_));
        }

        void onRead(NimBLECharacteristic *characteristic,
                    NimBLEConnInfo &) override {
            characteristic->setValue(responseJSON(lastStatus_));
        }

      private:
        streaming::TuningApplyStatus lastStatus_ =
            streaming::TuningApplyStatus::Applied;
    };

    inline Callbacks callbacks;

    inline NimBLECharacteristic *init(NimBLEService *service,
                                      NimBLEUUID uuid) {
        auto *characteristic = service->createCharacteristic(
            uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
        characteristic->setCallbacks(&callbacks);
        characteristic->setValue(
            responseJSON(streaming::TuningApplyStatus::Applied));
        return characteristic;
    }

}  // namespace stream_tuning_ble

#endif  // OSSM_STREAM_TUNING

#endif  // OSSM_COMMUNICATION_STREAM_TUNING_HPP
