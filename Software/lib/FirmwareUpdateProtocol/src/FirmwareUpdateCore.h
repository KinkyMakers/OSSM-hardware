#pragma once

#include <Arduino.h>

#include "FirmwareUpdateProtocol.h"

namespace firmware {

inline std::string sha256Hex(const unsigned char digest[32]) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    std::string output(64, '0');
    for (std::size_t index = 0; index < 32; ++index) {
        output[index * 2] = HEX_DIGITS[(digest[index] >> 4) & 0x0f];
        output[index * 2 + 1] = HEX_DIGITS[digest[index] & 0x0f];
    }
    return output;
}

// These routines are shared by the ESP32 wrappers and native failure tests.
// Adapters bind at compile time; no virtual dispatch or allocated callbacks.
template <class Http>
bool postCheckWith(Http &http, const DeviceReport &report, Decision &decision,
                   String &error) {
    if (!http.initialized()) {
        error = "failed to initialize HTTPS client";
        return false;
    }
    const std::string body = serializeReport(report);
    bool success = false;
    if (!http.open(body.size()) ||
        http.write(body.data(), body.size()) != static_cast<int>(body.size())) {
        error = "firmware check request failed";
    } else {
        const auto contentLength = http.fetchHeaders();
        if (contentLength < 0) {
            error = "firmware check response headers failed";
        } else {
            const int status = http.status();
            std::string response;
            char buffer[512];
            int read = 0;
            bool responseTooLarge = false;
            while ((read = http.read(buffer, sizeof(buffer))) > 0) {
                if (response.length() + read > 32768) {
                    error = "firmware response is too large";
                    responseTooLarge = true;
                    response.clear();
                    break;
                }
                response.append(buffer, read);
            }
            if (status != 200) {
                error = "firmware check returned HTTP " + String(status);
            } else if (!responseTooLarge &&
                       (read < 0 ||
                        (contentLength > 0 &&
                         static_cast<std::uint64_t>(contentLength) != response.size()) ||
                        !http.isComplete())) {
                // A valid JSON prefix can precede EOF or a zero-byte timeout.
                // Unknown-length/chunked bodies rely on the SDK completion flag.
                error = "firmware check response was incomplete";
            } else if (!response.empty()) {
                std::string parseError;
                success = parseDecision(response.c_str(), report.deviceType,
                                        decision, parseError);
                if (!success) error = parseError.c_str();
            }
        }
    }

    http.close();
    http.cleanup();
    if (success) {
        std::string validationError;
        if (!validateDecisionForReport(report, decision, validationError)) {
            error = validationError.c_str();
            return false;
        }
    }
    return success;
}

template <class Http, class Writer, class Sha256>
bool installStreamedArtifactWith(const Artifact &artifact, int updateCommand,
                                 Http &http, Writer &writer, Sha256 &sha,
                                 String &error) {
    if (!http.initialized()) {
        error = "failed to initialize artifact HTTPS client";
        return false;
    }

    bool success = false;
    if (!http.open(0)) {
        error = "artifact connection failed";
    } else {
        const auto contentLength = http.fetchHeaders();
        const int status = http.status();
        if (status != 200) {
            error = "artifact returned HTTP " + String(status);
        } else if (contentLength >= 0 &&
                   static_cast<std::uint64_t>(contentLength) !=
                       artifact.sizeBytes) {
            error = "artifact content length mismatch";
        } else if (!writer.begin(artifact.sizeBytes, updateCommand)) {
            error = "update partition rejected artifact";
        } else {
            sha.init();
            bool streamOk = sha.start();
            if (!streamOk) error = "artifact SHA-256 initialization failed";
            std::uint32_t total = 0;
            unsigned char buffer[4096];
            int read = 0;
            while (streamOk &&
                   (read = http.read(reinterpret_cast<char *>(buffer),
                                     sizeof(buffer))) > 0) {
                // total never exceeds sizeBytes; subtract to avoid overflow.
                if (static_cast<std::uint32_t>(read) > artifact.sizeBytes - total ||
                    writer.write(buffer, read) != static_cast<std::size_t>(read)) {
                    streamOk = false;
                    error = "artifact write failed";
                    break;
                }
                if (!sha.update(buffer, read)) {
                    streamOk = false;
                    error = "artifact SHA-256 update failed";
                    break;
                }
                total += read;
            }
            unsigned char digest[32] = {};
            if (streamOk && !sha.finish(digest)) {
                streamOk = false;
                error = "artifact SHA-256 finalization failed";
            }
            sha.free();

            if (!streamOk || read < 0 || total != artifact.sizeBytes) {
                if (error.length() == 0) error = "artifact download was incomplete";
                writer.abort();
            } else if (sha256Hex(digest) != artifact.sha256) {
                error = "artifact SHA-256 mismatch";
                writer.abort();
            } else if (!writer.end(true)) {
                error = "artifact finalization failed";
            } else {
                success = true;
            }
        }
    }

    http.close();
    http.cleanup();
    return success;
}

template <class Installer>
bool installApplicationAndFilesystemWith(const Decision &decision,
                                         int applicationCommand,
                                         int filesystemCommand,
                                         Installer &installer, String &error) {
    bool applicationInstalled = false;
    for (std::size_t index = 0; index < decision.artifactCount; ++index) {
        const Artifact &artifact = decision.artifacts[index];
        if (artifact.role == "filesystem") {
            if (!installer.install(artifact, filesystemCommand, error)) return false;
        } else if (artifact.role == "application") {
            if (!installer.install(artifact, applicationCommand, error)) return false;
            applicationInstalled = true;
        } else {
            error = ("unsupported installable artifact role: " + artifact.role).c_str();
            return false;
        }
    }
    if (!applicationInstalled) {
        error = "release has no application artifact";
        return false;
    }
    return true;
}

template <class Runtime>
void finishUpdateAttempt(Runtime &runtime, bool mqttStopped, const String &reason) {
    if (reason.length() > 0) runtime.logFailure(reason);
    if (mqttStopped) runtime.startMqtt();
    runtime.updateUnavailable();
    runtime.deleteTask();
}

template <class Runtime>
void runUpdateAttempt(Runtime &runtime) {
    bool mqttStopped = false;
    if (runtime.hasMqtt()) {
        // Preserve the existing stop/start policy even if the SDK reports an
        // already-stopped client. Failure paths still attempt to resume it.
        runtime.stopMqtt();
        mqttStopped = true;
    }
    if (!runtime.wifiConnected()) {
        finishUpdateAttempt(runtime, mqttStopped, "Wi-Fi is disconnected");
        return;
    }

    Decision decision;
    String error;
    const DeviceReport report = runtime.makeReport();
    if (!runtime.postCheck(report, decision, error)) {
        finishUpdateAttempt(runtime, mqttStopped, error);
        return;
    }
    runtime.observeCurrent(report, decision);
    runtime.stageUpdate(report, decision);
    runtime.logDecision(decision);
    if (!decision.shouldUpdate) {
        finishUpdateAttempt(runtime, mqttStopped, "");
        return;
    }

    runtime.drawUpdating();
    if (!runtime.install(decision, error)) {
        finishUpdateAttempt(runtime, mqttStopped, error);
        return;
    }
    runtime.restart();
}

template <class BootOps>
void confirmPendingImageWith(BootOps &boot) {
    const auto *running = boot.runningPartition();
    typename BootOps::State state{};
    if (running == nullptr || !boot.stateFor(running, state) ||
        state != BootOps::pendingState) {
        return;
    }
    boot.logConfirmation(boot.markValid());
}

}  // namespace firmware
