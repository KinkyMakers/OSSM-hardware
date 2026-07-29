#ifndef OSSM_COMMUNICATION_COMMAND_HPP
#define OSSM_COMMUNICATION_COMMAND_HPP

#include <queue>
#include <regex>

#include "Arduino.h"
#include "NimBLECharacteristic.h"
#include "NimBLEService.h"
#include "NimBLEUUID.h"
#include "queue.h"
#include "rad_ble.h"
#include "services/led.h"
#include "stream_command_parser.h"

static const std::regex commandRegex(
    R"(go:(simplePenetration|strokeEngine|streaming|menu)|set:(speed|stroke|depth|sensation|buffer|pattern):\d+|set:wifi:[^|]+\|.+|stream:\d+:\d+)");

/** Handler class for characteristic actions */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    uint32_t lastWriteTime = 0;
    float writeHz = 0;
    const float alpha = 0.1;  // Smoothing factor for exponential moving average

    void onWrite(NimBLECharacteristic* pCharacteristic,
                 NimBLEConnInfo& connInfo) override {
        const auto receivedAt = std::chrono::steady_clock::now();
        std::string cmd = pCharacteristic->getValue();

        // Motion Lab can pack several ordered waypoints into one ATT write.
        // This preserves the full 40 Hz logical stream even when the host's
        // CoreBluetooth write window is smaller than the requested cadence.
        // Every waypoint retains its own duration and the payload-level
        // receive timestamp is captured before queueing any of them.
        const auto *bytes = reinterpret_cast<const uint8_t *>(cmd.data());
        if (stream_command_parser::hasPackedPrefix(bytes, cmd.size())) {
            stream_command_parser::PackedCommands packed;
            if (!stream_command_parser::parsePacked(bytes, cmd.size(), packed)) {
                ESP_LOGD("NIMBLE_COMMAND", "Invalid packed stream command");
                pCharacteristic->setValue("fail:stream:invalid_packed_format");
                return;
            }
            for (size_t index = 0; index < packed.count; ++index) {
                const auto &command = packed.commands[index];
                if (!enqueueTarget({command.position,
                                    command.durationMilliseconds,
                                    receivedAt})) {
                    ESP_LOGE("Streaming",
                             "STREAM_ERROR type=input_overflow source=packed_command");
                    pCharacteristic->setValue("fail:stream:overflow");
                    return;
                }
            }
            pulseForCommunication();
            return;
        }

        // RAD BLE v1 deliberately multiplexes OSSM's established command
        // characteristic. JSON is queued to the shared dispatcher; existing
        // go:/set:/stream: text keeps its original behavior.
        if (!cmd.empty() && cmd.front() == '{') {
            if (!radBleServer.enqueue(
                    0, connInfo.getConnHandle(),
                    reinterpret_cast<const uint8_t*>(cmd.data()), cmd.size()))
                ESP_LOGW("NIMBLE_COMMAND", "RAD BLE command queue is full");
            return;
        }

        // Streaming waypoints dominate the command rate. Parse and enqueue
        // them directly from the NimBLE callback so they never allocate in
        // the general String queue or wait for the notification loop.
        if (cmd.compare(0, 7, "stream:") == 0) {
            stream_command_parser::Command command;
            if (!stream_command_parser::parse(cmd.data(), cmd.size(), command)) {
                ESP_LOGD("NIMBLE_COMMAND", "Invalid stream command");
                pCharacteristic->setValue("fail:stream:invalid_format");
                return;
            }
            if (!enqueueTarget({command.position,
                                command.durationMilliseconds,
                                receivedAt})) {
                ESP_LOGE("Streaming",
                         "STREAM_ERROR type=input_overflow source=command");
                pCharacteristic->setValue("fail:stream:overflow");
                return;
            }
            pulseForCommunication();
            return;
        }

        if (!std::regex_match(cmd, commandRegex)) {
            ESP_LOGD("NIMBLE_COMMAND", "Invalid command: %s", cmd.c_str());
            pCharacteristic->setValue("fail:" + String(cmd.c_str()));
            return;
        }
        messageQueue.push(String(cmd.c_str()));

        // Trigger LED communication pulse for received command
        pulseForCommunication();
    }

    /**
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        ESP_LOGV("NIMBLE_COMMAND",
                 "Notification/Indication return code: %d, %s", code,
                 NimBLEUtils::returnCodeToString(code));
    }
} inline chrCallbacks;

inline NimBLECharacteristic* initCommandCharacteristic(NimBLEService* pService,
                                                NimBLEUUID uuid) {
    // Command characteristic (writable, readable)
    NimBLECharacteristic* pChar = pService->createCharacteristic(
        uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR);

    // Store the characteristic pointer globally

    pChar->setCallbacks(&chrCallbacks);

    return pChar;
}

#endif  // OSSM_COMMUNICATION_COMMAND_HPP
