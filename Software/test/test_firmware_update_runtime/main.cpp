#include <ArduinoFake.h>
#include <unity.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

#include "FirmwareUpdateCore.h"

namespace {

constexpr int APPLICATION_COMMAND = 10;
constexpr int FILESYSTEM_COMMAND = 20;
constexpr const char *ABC_SHA256 =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

struct Trace {
    std::vector<std::string> calls;
    void add(const std::string &call) { calls.push_back(call); }
    std::size_t count(const char *call) const {
        return std::count(calls.begin(), calls.end(), call);
    }
};

void assertBefore(const Trace &trace, const char *first, const char *second) {
    const auto left = std::find(trace.calls.begin(), trace.calls.end(), first);
    const auto right = std::find(trace.calls.begin(), trace.calls.end(), second);
    TEST_ASSERT_TRUE_MESSAGE(left != trace.calls.end(), first);
    TEST_ASSERT_TRUE_MESSAGE(right != trace.calls.end(), second);
    TEST_ASSERT_TRUE(left < right);
}

void assertTrace(const Trace &trace, std::initializer_list<const char *> expected) {
    std::string actualText, expectedText;
    for (const auto &call : trace.calls) actualText += call + "\n";
    for (const auto *call : expected) expectedText += std::string(call) + "\n";
    TEST_ASSERT_EQUAL_STRING(expectedText.c_str(), actualText.c_str());
}

struct FakeHttp {
    Trace &trace;
    std::string name;
    std::string body;
    std::string requestBody;
    std::vector<int> reads;
    std::size_t offset = 0;
    std::size_t nextRead = 0;
    std::size_t openedLength = 0;
    std::size_t largestReadRequest = 0;
    std::int64_t contentLength = 0;
    int statusCode = 200;
    int terminalRead = 0;
    int requestWriteResult = 0;
    bool overrideRequestWrite = false;
    bool ready = true;
    bool openResult = true;
    bool complete = true;

    FakeHttp(Trace &trace, const char *name) : trace(trace), name(name) {}
    void setBody(const std::string &value) {
        body = value;
        contentLength = value.size();
        offset = nextRead = 0;
    }
    void record(const char *operation) { trace.add(name + "." + operation); }
    bool initialized() {
        record("init");
        return ready;
    }
    bool open(std::size_t length) {
        record("open");
        openedLength = length;
        return openResult;
    }
    int write(const char *data, std::size_t length) {
        record("write");
        requestBody.assign(data, length);
        return overrideRequestWrite ? requestWriteResult : static_cast<int>(length);
    }
    std::int64_t fetchHeaders() {
        record("headers");
        return contentLength;
    }
    int status() {
        record("status");
        return statusCode;
    }
    int read(char *data, std::size_t capacity) {
        record("read");
        largestReadRequest = std::max(largestReadRequest, capacity);
        const int length = reads.empty()
            ? (offset < body.size()
                ? static_cast<int>(std::min(capacity, body.size() - offset))
                : terminalRead)
            : (nextRead < reads.size() ? reads[nextRead++] : terminalRead);
        if (length > 0) {
            if (static_cast<std::size_t>(length) > capacity ||
                offset + length > body.size()) {
                TEST_FAIL_MESSAGE("Invalid scripted read exceeds its buffer/body");
                return -1;
            }
            std::memcpy(data, body.data() + offset, length);
            offset += length;
        }
        return length;
    }
    bool isComplete() {
        record("complete");
        return complete;
    }
    void close() { record("close"); }
    void cleanup() { record("cleanup"); }
};

struct FakeWriter {
    Trace &trace;
    std::string written;
    std::size_t expectedSize = 0;
    int command = -1;
    unsigned writes = 0;
    unsigned shortWriteAt = 0;
    bool beginResult = true;
    bool endResult = true;
    bool evenIfRemaining = false;

    explicit FakeWriter(Trace &trace) : trace(trace) {}
    bool begin(std::size_t size, int value) {
        trace.add("writer.begin");
        expectedSize = size;
        command = value;
        return beginResult;
    }
    std::size_t write(unsigned char *data, std::size_t length) {
        trace.add("writer.write");
        const std::size_t accepted = ++writes == shortWriteAt ? length - 1 : length;
        written.append(reinterpret_cast<const char *>(data), accepted);
        return accepted;
    }
    void abort() { trace.add("writer.abort"); }
    bool end(bool allowRemaining) {
        trace.add("writer.end");
        evenIfRemaining = allowRemaining;
        return endResult;
    }
};

struct FakeSha256 {
    Trace &trace;
    std::string hashed;
    unsigned updates = 0;
    unsigned failUpdateAt = 0;
    bool startResult = true;
    bool finishResult = true;
    // A known SHA-256 result for the test body "abc". This adapter tests the
    // production checksum decisions and supplied bytes, not the crypto library.
    std::array<unsigned char, 32> digest = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
    };

    explicit FakeSha256(Trace &trace) : trace(trace) {}
    void init() { trace.add("sha.init"); }
    bool start() {
        trace.add("sha.start");
        return startResult;
    }
    bool update(const unsigned char *data, std::size_t length) {
        trace.add("sha.update");
        hashed.append(reinterpret_cast<const char *>(data), length);
        return ++updates != failUpdateAt;
    }
    bool finish(unsigned char output[32]) {
        trace.add("sha.finish");
        std::copy(digest.begin(), digest.end(), output);
        return finishResult;
    }
    void free() { trace.add("sha.free"); }
};

firmware::Artifact application() {
    return {"application",
            "https://example.supabase.co/storage/v1/object/public/ossm-firmware/"
            "releases/1.0.35/build/firmware.bin",
            ABC_SHA256, 3, 0};
}

struct Transfer {
    Trace trace;
    FakeHttp http{trace, "artifact"};
    FakeWriter writer{trace};
    FakeSha256 sha{trace};
    firmware::Artifact artifact = application();
    String error;

    Transfer() { http.setBody("abc"); }
    bool run() {
        return firmware::installStreamedArtifactWith(
            artifact, APPLICATION_COMMAND, http, writer, sha, error);
    }
};

void assertAborted(Transfer &transfer, const char *error) {
    TEST_ASSERT_FALSE(transfer.run());
    TEST_ASSERT_EQUAL_STRING(error, transfer.error.c_str());
    TEST_ASSERT_EQUAL_UINT(1, transfer.trace.count("writer.abort"));
    TEST_ASSERT_EQUAL_UINT(0, transfer.trace.count("writer.end"));
    TEST_ASSERT_EQUAL_UINT(1, transfer.trace.count("sha.free"));
    TEST_ASSERT_EQUAL_UINT(1, transfer.trace.count("artifact.close"));
    TEST_ASSERT_EQUAL_UINT(1, transfer.trace.count("artifact.cleanup"));
    assertBefore(transfer.trace, "sha.free", "writer.abort");
    assertBefore(transfer.trace, "writer.abort", "artifact.close");
    assertBefore(transfer.trace, "artifact.close", "artifact.cleanup");
}

void test_transfer_accepts_partial_reads_and_finalizes_after_hash() {
    Transfer transfer;
    transfer.http.reads = {1, 2, 0};
    TEST_ASSERT_TRUE(transfer.run());
    TEST_ASSERT_EQUAL_STRING("abc", transfer.writer.written.c_str());
    TEST_ASSERT_EQUAL_STRING("abc", transfer.sha.hashed.c_str());
    TEST_ASSERT_EQUAL_STRING("", transfer.error.c_str());
    TEST_ASSERT_EQUAL_UINT(3, transfer.writer.expectedSize);
    TEST_ASSERT_EQUAL_INT(APPLICATION_COMMAND, transfer.writer.command);
    TEST_ASSERT_TRUE(transfer.writer.evenIfRemaining);
    TEST_ASSERT_EQUAL_UINT(4096, transfer.http.largestReadRequest);
    assertTrace(transfer.trace, {
        "artifact.init", "artifact.open", "artifact.headers", "artifact.status",
        "writer.begin", "sha.init", "sha.start",
        "artifact.read", "writer.write", "sha.update",
        "artifact.read", "writer.write", "sha.update",
        "artifact.read", "sha.finish", "sha.free", "writer.end",
        "artifact.close", "artifact.cleanup",
    });
}

void test_transfer_rejects_early_eof_and_read_errors() {
    for (const auto &reads : std::vector<std::vector<int>>{
             {0}, {2, 0}, {-1}, {2, -1}, {3, -1}}) {
        Transfer transfer;
        transfer.http.reads = reads;
        assertAborted(transfer, "artifact download was incomplete");
    }
}

void test_transfer_rejects_excess_bytes_before_writing_them() {
    for (const auto &reads : std::vector<std::vector<int>>{{4, 0}, {3, 1, 0}}) {
        Transfer transfer;
        transfer.http.setBody("abcd");
        transfer.http.contentLength = 3;
        transfer.http.reads = reads;
        assertAborted(transfer, "artifact write failed");
        TEST_ASSERT_EQUAL_UINT(reads.front() == 4 ? 0 : 3,
                               transfer.writer.written.size());
        TEST_ASSERT_EQUAL_STRING(transfer.writer.written.c_str(),
                                 transfer.sha.hashed.c_str());
    }
}

void test_transfer_stops_after_a_short_partition_write() {
    Transfer transfer;
    transfer.http.reads = {1, 2, 0};
    transfer.writer.shortWriteAt = 2;
    assertAborted(transfer, "artifact write failed");
    TEST_ASSERT_EQUAL_STRING("ab", transfer.writer.written.c_str());
    TEST_ASSERT_EQUAL_STRING("a", transfer.sha.hashed.c_str());
    TEST_ASSERT_EQUAL_UINT(2, transfer.trace.count("artifact.read"));
}

void test_transfer_rejects_checksum_mismatch() {
    Transfer transfer;
    transfer.artifact.sha256.assign(64, '0');
    assertAborted(transfer, "artifact SHA-256 mismatch");
}

void test_transfer_fails_closed_on_every_sha_error() {
    const char *errors[] = {
        "artifact SHA-256 initialization failed",
        "artifact SHA-256 update failed",
        "artifact SHA-256 finalization failed",
    };
    for (unsigned failure = 0; failure < 3; ++failure) {
        Transfer transfer;
        // Matching output must never override an error returned by SHA.
        transfer.sha.startResult = failure != 0;
        transfer.sha.failUpdateAt = failure == 1 ? 1 : 0;
        transfer.sha.finishResult = failure != 2;
        assertAborted(transfer, errors[failure]);
        if (failure == 0) TEST_ASSERT_EQUAL_UINT(0, transfer.trace.count("artifact.read"));
    }
}

void test_transfer_does_not_abort_after_failed_update_end() {
    Transfer transfer;
    transfer.writer.endResult = false;
    TEST_ASSERT_FALSE(transfer.run());
    TEST_ASSERT_EQUAL_STRING("artifact finalization failed", transfer.error.c_str());
    TEST_ASSERT_EQUAL_UINT(1, transfer.trace.count("writer.end"));
    TEST_ASSERT_EQUAL_UINT(0, transfer.trace.count("writer.abort"));
    assertBefore(transfer.trace, "sha.free", "writer.end");
    assertBefore(transfer.trace, "writer.end", "artifact.close");
    TEST_ASSERT_EQUAL_UINT(1, transfer.trace.count("artifact.cleanup"));
}

void test_transfer_cleans_up_preflight_failures_without_aborting() {
    for (unsigned failure = 0; failure < 5; ++failure) {
        Transfer transfer;
        if (failure == 0) transfer.http.ready = false;
        if (failure == 1) transfer.http.openResult = false;
        if (failure == 2) transfer.http.statusCode = 404;
        if (failure == 3) transfer.http.contentLength = 4;
        if (failure == 4) transfer.writer.beginResult = false;
        TEST_ASSERT_FALSE(transfer.run());
        TEST_ASSERT_EQUAL_UINT(failure == 0 ? 0 : 1,
                               transfer.trace.count("artifact.close"));
        TEST_ASSERT_EQUAL_UINT(failure == 0 ? 0 : 1,
                               transfer.trace.count("artifact.cleanup"));
        TEST_ASSERT_EQUAL_UINT(failure == 4 ? 1 : 0,
                               transfer.trace.count("writer.begin"));
        TEST_ASSERT_EQUAL_UINT(0, transfer.trace.count("writer.abort"));
        TEST_ASSERT_EQUAL_UINT(0, transfer.trace.count("writer.end"));
        TEST_ASSERT_EQUAL_UINT(0, transfer.trace.count("sha.init"));
    }
}

void test_transfer_preserves_existing_header_length_policy() {
    Transfer noLength;
    // IDF reports 0 for missing Content-Length/chunked responses.
    noLength.http.contentLength = 0;
    TEST_ASSERT_FALSE(noLength.run());
    TEST_ASSERT_EQUAL_STRING("artifact content length mismatch", noLength.error.c_str());
    TEST_ASSERT_EQUAL_UINT(0, noLength.trace.count("writer.begin"));

    Transfer negativeLength;
    // Preserve the old negative-result branch; do not claim chunked support.
    negativeLength.http.contentLength = -1;
    TEST_ASSERT_TRUE(negativeLength.run());

    Transfer oversizedLength;
    // Keep the adapter length wide; truncation would wrap to the expected 3.
    oversizedLength.http.contentLength = (std::int64_t{1} << 32) + 3;
    TEST_ASSERT_FALSE(oversizedLength.run());
    TEST_ASSERT_EQUAL_STRING("artifact content length mismatch",
                             oversizedLength.error.c_str());
    TEST_ASSERT_EQUAL_UINT(0, oversizedLength.trace.count("writer.begin"));
}

void test_sha256_hex_encodes_every_digest_byte() {
    unsigned char digest[32];
    for (unsigned index = 0; index < 32; ++index) digest[index] = index;
    TEST_ASSERT_EQUAL_STRING(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
        firmware::sha256Hex(digest).c_str());
}

struct RecordingInstaller {
    std::vector<std::string> roles;
    std::vector<int> commands;
    unsigned failAt = 0;
    bool install(const firmware::Artifact &artifact, int command, String &error) {
        roles.push_back(artifact.role);
        commands.push_back(command);
        if (roles.size() == failAt) {
            error = "injected transfer failure";
            return false;
        }
        return true;
    }
};

void test_artifact_dispatch_preserves_order_roles_and_first_failure() {
    for (unsigned failure = 0; failure <= 2; ++failure) {
        firmware::Decision decision;
        decision.artifactCount = 2;
        decision.artifacts[0] = application();
        decision.artifacts[0].role = "filesystem";
        decision.artifacts[1] = application();
        RecordingInstaller installer;
        installer.failAt = failure;
        String error;
        TEST_ASSERT_EQUAL(failure == 0,
            firmware::installApplicationAndFilesystemWith(
                decision, APPLICATION_COMMAND, FILESYSTEM_COMMAND, installer, error));
        TEST_ASSERT_EQUAL_UINT(failure == 1 ? 1 : 2, installer.roles.size());
        TEST_ASSERT_EQUAL_STRING("filesystem", installer.roles[0].c_str());
        TEST_ASSERT_EQUAL_INT(FILESYSTEM_COMMAND, installer.commands[0]);
        if (failure != 1) {
            TEST_ASSERT_EQUAL_STRING("application", installer.roles[1].c_str());
            TEST_ASSERT_EQUAL_INT(APPLICATION_COMMAND, installer.commands[1]);
        }
    }

    firmware::Decision applicationFirst;
    applicationFirst.artifactCount = 2;
    applicationFirst.artifacts[0] = application();
    applicationFirst.artifacts[1] = application();
    applicationFirst.artifacts[1].role = "filesystem";
    RecordingInstaller installer;
    installer.failAt = 2;
    String error;
    TEST_ASSERT_FALSE(firmware::installApplicationAndFilesystemWith(
        applicationFirst, APPLICATION_COMMAND, FILESYSTEM_COMMAND, installer, error));
    TEST_ASSERT_EQUAL_STRING("application", installer.roles[0].c_str());
    TEST_ASSERT_EQUAL_STRING("filesystem", installer.roles[1].c_str());
}

void test_artifact_dispatch_requires_application_and_rejects_other_roles() {
    for (const char *role : {"", "filesystem", "bootloader", "partitions", "manifest"}) {
        firmware::Decision decision;
        if (role[0] != '\0') {
            decision.artifactCount = 1;
            decision.artifacts[0] = application();
            decision.artifacts[0].role = role;
        }
        RecordingInstaller installer;
        String error;
        TEST_ASSERT_FALSE(firmware::installApplicationAndFilesystemWith(
            decision, APPLICATION_COMMAND, FILESYSTEM_COMMAND, installer, error));
        TEST_ASSERT_EQUAL_UINT(std::strcmp(role, "filesystem") == 0 ? 1 : 0,
                               installer.roles.size());
        TEST_ASSERT_GREATER_THAN_UINT(0, error.length());
    }
}

firmware::DeviceReport report() {
    firmware::DeviceReport value;
    value.deviceType = "ossm";
    value.deviceId = "AA:BB:CC:DD:EE:FF";
    value.reportedTrack = "main";
    value.currentVersion = "1.0.34";
    value.currentBuild = "fedcba9876543210";
    return value;
}

std::string checkResponse(bool shouldUpdate = true) {
    JsonDocument document;
    deserializeJson(document, R"json({
      "protocolVersion":1,"shouldUpdate":true,"updateAvailable":true,
      "reason":"update-available","reportedTrack":"main","assignedTrack":"main",
      "trackChanged":false,"firmwareOrigin":"official",
      "currentProvenance":"current.jws.token","currentVersion":"1.0.34",
      "targetVersion":"1.0.35","nextHopVersion":"1.0.35",
      "update":{"provenance":"target.jws.token",
        "releaseId":"00000000-0000-4000-8000-000000000001",
        "buildSha":"0123456789abcdef0123456789abcdef01234567",
        "kind":"firmware","publishedAt":"2026-07-16T00:00:00.000Z",
        "artifacts":[{"role":"application",
          "url":"https://example.supabase.co/storage/v1/object/public/ossm-firmware/releases/1.0.35/build/firmware.bin",
          "sha256":"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sizeBytes":3,"installOrder":0}]},
      "nextCheckSeconds":60
    })json");
    if (!shouldUpdate) {
        document["shouldUpdate"] = false;
        document["updateAvailable"] = false;
        document["reason"] = "already-current";
        document["targetVersion"] = nullptr;
        document["nextHopVersion"] = nullptr;
        document["update"] = nullptr;
    }
    std::string output;
    serializeJson(document, output);
    return output;
}

void test_check_uses_production_report_parser_and_identity_validation() {
    Trace trace;
    FakeHttp http(trace, "check");
    http.setBody(checkResponse());
    firmware::Decision decision;
    String error;
    auto device = report();
    TEST_ASSERT_TRUE(firmware::postCheckWith(http, device, decision, error));
    TEST_ASSERT_EQUAL_STRING(firmware::serializeReport(device).c_str(),
                             http.requestBody.c_str());
    TEST_ASSERT_EQUAL_UINT(http.requestBody.size(), http.openedLength);
    TEST_ASSERT_TRUE(decision.shouldUpdate);
    TEST_ASSERT_EQUAL_STRING(ABC_SHA256, decision.artifacts[0].sha256.c_str());
    assertBefore(trace, "check.close", "check.cleanup");

    Trace invalidTrace;
    FakeHttp invalid(invalidTrace, "check");
    invalid.setBody(checkResponse());
    device.currentVersion = "1.0.33";
    TEST_ASSERT_FALSE(firmware::postCheckWith(invalid, device, decision, error));
    TEST_ASSERT_EQUAL_STRING("firmware response does not match device report", error.c_str());
    TEST_ASSERT_EQUAL_UINT(1, invalidTrace.count("check.cleanup"));
}

void test_check_rejects_request_and_response_failures_and_cleans_up() {
    for (unsigned failure = 0; failure < 6; ++failure) {
        Trace trace;
        FakeHttp http(trace, "check");
        http.setBody(checkResponse());
        if (failure == 0) http.ready = false;
        if (failure == 1) http.openResult = false;
        if (failure == 2) {
            http.overrideRequestWrite = true;
            http.requestWriteResult = 1;
        }
        if (failure == 3) http.statusCode = 500;
        if (failure == 4) http.setBody("");
        if (failure == 5) http.setBody("{bad json");
        firmware::Decision decision;
        String error;
        TEST_ASSERT_FALSE(firmware::postCheckWith(http, report(), decision, error));
        TEST_ASSERT_EQUAL_UINT(failure == 0 ? 0 : 1, trace.count("check.close"));
        TEST_ASSERT_EQUAL_UINT(failure == 0 ? 0 : 1, trace.count("check.cleanup"));
    }
}

void test_check_rejects_terminal_read_error_after_complete_json() {
    Trace trace;
    FakeHttp http(trace, "check");
    http.setBody(checkResponse());
    http.terminalRead = -1;
    firmware::Decision decision;
    String error;
    TEST_ASSERT_FALSE(firmware::postCheckWith(http, report(), decision, error));
    TEST_ASSERT_EQUAL_STRING("firmware check response was incomplete", error.c_str());
    TEST_ASSERT_EQUAL_UINT(1, trace.count("check.close"));
    TEST_ASSERT_EQUAL_UINT(1, trace.count("check.cleanup"));
}

void test_check_rejects_negative_headers_before_reading_or_parsing() {
    Trace trace;
    FakeHttp http(trace, "check");
    http.setBody(checkResponse());
    http.contentLength = -1;
    firmware::Decision decision;
    String error;
    TEST_ASSERT_FALSE(firmware::postCheckWith(http, report(), decision, error));
    TEST_ASSERT_EQUAL_STRING("firmware check response headers failed", error.c_str());
    TEST_ASSERT_EQUAL_UINT(0, trace.count("check.status"));
    TEST_ASSERT_EQUAL_UINT(0, trace.count("check.read"));
    TEST_ASSERT_EQUAL_UINT(0, trace.count("check.complete"));
    TEST_ASSERT_FALSE(decision.shouldUpdate);
    TEST_ASSERT_EQUAL_UINT(1, trace.count("check.close"));
    TEST_ASSERT_EQUAL_UINT(1, trace.count("check.cleanup"));
}

void test_check_rejects_wrong_or_huge_declared_length_before_parsing() {
    const std::string response = checkResponse();
    const auto actualLength = static_cast<std::int64_t>(response.size());
    for (std::int64_t declaredLength : {
             actualLength - 1, actualLength + 1,
             (std::int64_t{1} << 32) + actualLength,
             std::numeric_limits<std::int64_t>::max()}) {
        Trace trace;
        FakeHttp http(trace, "check");
        http.setBody(response);
        http.contentLength = declaredLength;
        firmware::Decision decision;
        String error;
        TEST_ASSERT_FALSE(firmware::postCheckWith(http, report(), decision, error));
        TEST_ASSERT_EQUAL_STRING("firmware check response was incomplete", error.c_str());
        TEST_ASSERT_EQUAL_UINT(response.size(), http.offset);
        TEST_ASSERT_FALSE(decision.shouldUpdate);
        TEST_ASSERT_EQUAL_UINT(1, trace.count("check.close"));
        TEST_ASSERT_EQUAL_UINT(1, trace.count("check.cleanup"));
    }
}

void test_check_rejects_early_zero_after_valid_json_prefix() {
    const std::string prefix = checkResponse();
    for (bool unknownLength : {false, true}) {
        Trace trace;
        FakeHttp http(trace, "check");
        http.setBody(prefix + std::string(64, ' '));
        if (unknownLength) http.contentLength = 0;
        http.complete = false;
        for (std::size_t remaining = prefix.size(); remaining > 0;) {
            const auto chunk = std::min(remaining, std::size_t{512});
            http.reads.push_back(static_cast<int>(chunk));
            remaining -= chunk;
        }
        // ESP-IDF can return zero on EAGAIN before the body is complete.
        http.reads.push_back(0);
        firmware::Decision decision;
        String error;
        TEST_ASSERT_FALSE(firmware::postCheckWith(http, report(), decision, error));
        TEST_ASSERT_EQUAL_STRING("firmware check response was incomplete", error.c_str());
        TEST_ASSERT_EQUAL_UINT(prefix.size(), http.offset);
        TEST_ASSERT_FALSE(decision.shouldUpdate);
        TEST_ASSERT_EQUAL_UINT(unknownLength ? 1 : 0, trace.count("check.complete"));
        TEST_ASSERT_EQUAL_UINT(1, trace.count("check.close"));
        TEST_ASSERT_EQUAL_UINT(1, trace.count("check.cleanup"));
    }
}

void test_check_accepts_complete_chunked_or_unknown_length_response() {
    Trace trace;
    FakeHttp http(trace, "check");
    http.setBody(checkResponse());
    http.contentLength = 0;
    http.complete = true;
    firmware::Decision decision;
    String error;
    TEST_ASSERT_TRUE(firmware::postCheckWith(http, report(), decision, error));
    TEST_ASSERT_TRUE(decision.shouldUpdate);
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL_UINT(http.body.size(), http.offset);
    TEST_ASSERT_EQUAL_UINT(1, trace.count("check.complete"));
    assertBefore(trace, "check.complete", "check.close");
    TEST_ASSERT_EQUAL_UINT(1, trace.count("check.cleanup"));
}

void test_check_preserves_http_status_precedence_over_body_failures() {
    for (bool oversized : {false, true}) {
        Trace trace;
        FakeHttp http(trace, "check");
        std::string response = checkResponse();
        if (oversized) response.resize(32769, ' ');
        http.setBody(response);
        ++http.contentLength;
        http.complete = false;
        http.statusCode = 503;
        firmware::Decision decision;
        String error;
        TEST_ASSERT_FALSE(firmware::postCheckWith(http, report(), decision, error));
        TEST_ASSERT_EQUAL_STRING("firmware check returned HTTP 503", error.c_str());
        TEST_ASSERT_EQUAL_UINT(0, trace.count("check.complete"));
        TEST_ASSERT_FALSE(decision.shouldUpdate);
        TEST_ASSERT_EQUAL_UINT(1, trace.count("check.close"));
        TEST_ASSERT_EQUAL_UINT(1, trace.count("check.cleanup"));
    }
}

void test_check_enforces_response_size_limit_at_boundary() {
    for (std::size_t size : {32768U, 32769U}) {
        Trace trace;
        FakeHttp http(trace, "check");
        auto response = checkResponse();
        response.resize(size, ' ');
        http.setBody(response);
        firmware::Decision decision;
        String error;
        TEST_ASSERT_EQUAL(size == 32768,
            firmware::postCheckWith(http, report(), decision, error));
        if (size > 32768)
            TEST_ASSERT_EQUAL_STRING("firmware response is too large", error.c_str());
        TEST_ASSERT_EQUAL_UINT(512, http.largestReadRequest);
        TEST_ASSERT_EQUAL_UINT(1, trace.count("check.cleanup"));
    }
}

struct FakeRuntime {
    Trace trace;
    FakeHttp checkHttp{trace, "check"};
    FakeHttp artifactHttp{trace, "artifact"};
    FakeWriter writer{trace};
    FakeSha256 sha{trace};
    bool mqtt = true;
    bool wifi = true;
    int stopResult = 0;
    int startResult = 0;
    std::string observedToken;
    std::string stagedToken;
    std::string failureReason;

    FakeRuntime() {
        checkHttp.setBody(checkResponse());
        artifactHttp.setBody("abc");
    }
    bool hasMqtt() { return mqtt; }
    int stopMqtt() { trace.add("mqtt.stop"); return stopResult; }
    int startMqtt() { trace.add("mqtt.start"); return startResult; }
    bool wifiConnected() { trace.add("wifi"); return wifi; }
    firmware::DeviceReport makeReport() { trace.add("report"); return report(); }
    bool postCheck(const firmware::DeviceReport &device,
                   firmware::Decision &decision, String &error) {
        return firmware::postCheckWith(checkHttp, device, decision, error);
    }
    void observeCurrent(const firmware::DeviceReport &, const firmware::Decision &decision) {
        trace.add("provenance.observe");
        observedToken = decision.currentProvenance;
    }
    void stageUpdate(const firmware::DeviceReport &, const firmware::Decision &decision) {
        trace.add("provenance.stage");
        stagedToken = decision.provenance;
    }
    void logDecision(const firmware::Decision &) { trace.add("decision.log"); }
    void logFailure(const String &error) {
        trace.add("error.log");
        failureReason = error.c_str();
    }
    void drawUpdating() { trace.add("ui.updating"); }
    bool install(const firmware::Decision &decision, String &error) {
        struct Installer {
            FakeRuntime &runtime;
            bool install(const firmware::Artifact &artifact, int command, String &error) {
                return firmware::installStreamedArtifactWith(
                    artifact, command, runtime.artifactHttp, runtime.writer,
                    runtime.sha, error);
            }
        } installer{*this};
        return firmware::installApplicationAndFilesystemWith(
            decision, APPLICATION_COMMAND, FILESYSTEM_COMMAND, installer, error);
    }
    void updateUnavailable() { trace.add("ui.unavailable"); }
    void deleteTask() { trace.add("task.delete"); }
    void restart() { trace.add("restart"); }
};

void test_attempt_restarts_only_after_real_check_transfer_and_finalization() {
    FakeRuntime runtime;
    runtime.artifactHttp.reads = {1, 2, 0};
    firmware::runUpdateAttempt(runtime);
    TEST_ASSERT_EQUAL_STRING("abc", runtime.writer.written.c_str());
    TEST_ASSERT_EQUAL_STRING("abc", runtime.sha.hashed.c_str());
    TEST_ASSERT_EQUAL_STRING("current.jws.token", runtime.observedToken.c_str());
    TEST_ASSERT_EQUAL_STRING("target.jws.token", runtime.stagedToken.c_str());
    TEST_ASSERT_EQUAL_UINT(1, runtime.trace.count("restart"));
    TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("mqtt.start"));
    TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("ui.unavailable"));
    TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("task.delete"));
    assertBefore(runtime.trace, "mqtt.stop", "wifi");
    assertBefore(runtime.trace, "wifi", "report");
    assertBefore(runtime.trace, "report", "check.open");
    assertBefore(runtime.trace, "check.cleanup", "provenance.observe");
    assertBefore(runtime.trace, "provenance.observe", "provenance.stage");
    assertBefore(runtime.trace, "provenance.stage", "decision.log");
    assertBefore(runtime.trace, "decision.log", "ui.updating");
    assertBefore(runtime.trace, "ui.updating", "artifact.open");
    assertBefore(runtime.trace, "writer.end", "artifact.cleanup");
    assertBefore(runtime.trace, "artifact.cleanup", "restart");
}

void test_attempt_resumes_mqtt_for_every_returning_failure() {
    for (unsigned failure = 0; failure < 11; ++failure) {
        FakeRuntime runtime;
        if (failure == 0) runtime.wifi = false;
        if (failure == 1) runtime.checkHttp.openResult = false;
        if (failure == 2) runtime.checkHttp.terminalRead = -1;
        if (failure == 3) runtime.artifactHttp.reads = {2, 0};
        if (failure == 4) runtime.writer.shortWriteAt = 1;
        if (failure == 5) runtime.sha.digest.fill(0);
        if (failure == 6) runtime.sha.finishResult = false;
        if (failure == 7) runtime.writer.endResult = false;
        if (failure == 8) runtime.checkHttp.contentLength = -1;
        if (failure == 9) runtime.checkHttp.complete = false;
        if (failure == 10) ++runtime.checkHttp.contentLength;
        firmware::runUpdateAttempt(runtime);
        TEST_ASSERT_EQUAL_UINT(1, runtime.trace.count("mqtt.stop"));
        TEST_ASSERT_EQUAL_UINT(1, runtime.trace.count("mqtt.start"));
        TEST_ASSERT_EQUAL_UINT(1, runtime.trace.count("ui.unavailable"));
        TEST_ASSERT_EQUAL_UINT(1, runtime.trace.count("task.delete"));
        TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("restart"));
        TEST_ASSERT_FALSE(runtime.failureReason.empty());
        assertBefore(runtime.trace, "mqtt.start", "ui.unavailable");
        assertBefore(runtime.trace, "ui.unavailable", "task.delete");
        if (failure <= 2 || failure >= 8) {
            TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("provenance.observe"));
            TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("writer.begin"));
            if (failure != 0)
                assertBefore(runtime.trace, "check.cleanup", "mqtt.start");
        } else {
            assertBefore(runtime.trace, "artifact.cleanup", "mqtt.start");
        }
    }
}

void test_attempt_no_update_and_failed_mqtt_resume_still_finish_without_restart() {
    FakeRuntime runtime;
    runtime.checkHttp.setBody(checkResponse(false));
    runtime.stopResult = -1;
    runtime.startResult = -1;
    firmware::runUpdateAttempt(runtime);
    TEST_ASSERT_EQUAL_UINT(1, runtime.trace.count("mqtt.stop"));
    TEST_ASSERT_EQUAL_UINT(1, runtime.trace.count("mqtt.start"));
    TEST_ASSERT_EQUAL_UINT(1, runtime.trace.count("provenance.observe"));
    TEST_ASSERT_EQUAL_UINT(1, runtime.trace.count("provenance.stage"));
    TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("writer.begin"));
    TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("ui.updating"));
    TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("restart"));
    TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("error.log"));
    assertBefore(runtime.trace, "decision.log", "mqtt.start");
    assertBefore(runtime.trace, "mqtt.start", "ui.unavailable");
    assertBefore(runtime.trace, "ui.unavailable", "task.delete");
}

void test_attempt_without_mqtt_never_stops_or_starts_a_client() {
    for (bool success : {false, true}) {
        FakeRuntime runtime;
        runtime.mqtt = false;
        runtime.wifi = success;
        firmware::runUpdateAttempt(runtime);
        TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("mqtt.stop"));
        TEST_ASSERT_EQUAL_UINT(0, runtime.trace.count("mqtt.start"));
        TEST_ASSERT_EQUAL_UINT(success ? 1 : 0, runtime.trace.count("restart"));
        TEST_ASSERT_EQUAL_UINT(success ? 0 : 1, runtime.trace.count("task.delete"));
    }
}

struct FakeBoot {
    using State = int;
    static constexpr State pendingState = 1;
    Trace trace;
    int partition = 0;
    bool missingPartition = false;
    bool stateResult = true;
    State state = pendingState;
    int markResult = 0;
    int loggedResult = 999;

    const int *runningPartition() {
        trace.add("running");
        return missingPartition ? nullptr : &partition;
    }
    bool stateFor(const int *, State &output) {
        trace.add("state");
        output = state;
        return stateResult;
    }
    int markValid() { trace.add("mark"); return markResult; }
    void logConfirmation(int result) {
        trace.add("log");
        loggedResult = result;
    }
};

void test_confirmation_requires_running_partition_and_pending_state() {
    FakeBoot missing;
    missing.missingPartition = true;
    firmware::confirmPendingImageWith(missing);
    assertTrace(missing.trace, {"running"});

    FakeBoot failedRead;
    failedRead.stateResult = false;
    firmware::confirmPendingImageWith(failedRead);
    assertTrace(failedRead.trace, {"running", "state"});

    for (int state : {0, 2, 3, 4, -1}) {
        FakeBoot boot;
        boot.state = state;
        firmware::confirmPendingImageWith(boot);
        assertTrace(boot.trace, {"running", "state"});
    }
}

void test_confirmation_marks_pending_once_and_reports_mark_failure() {
    for (int result : {0, -1}) {
        FakeBoot boot;
        boot.markResult = result;
        firmware::confirmPendingImageWith(boot);
        assertTrace(boot.trace, {"running", "state", "mark", "log"});
        TEST_ASSERT_EQUAL_INT(result, boot.loggedResult);
    }
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_transfer_accepts_partial_reads_and_finalizes_after_hash);
    RUN_TEST(test_transfer_rejects_early_eof_and_read_errors);
    RUN_TEST(test_transfer_rejects_excess_bytes_before_writing_them);
    RUN_TEST(test_transfer_stops_after_a_short_partition_write);
    RUN_TEST(test_transfer_rejects_checksum_mismatch);
    RUN_TEST(test_transfer_fails_closed_on_every_sha_error);
    RUN_TEST(test_transfer_does_not_abort_after_failed_update_end);
    RUN_TEST(test_transfer_cleans_up_preflight_failures_without_aborting);
    RUN_TEST(test_transfer_preserves_existing_header_length_policy);
    RUN_TEST(test_sha256_hex_encodes_every_digest_byte);
    RUN_TEST(test_artifact_dispatch_preserves_order_roles_and_first_failure);
    RUN_TEST(test_artifact_dispatch_requires_application_and_rejects_other_roles);
    RUN_TEST(test_check_uses_production_report_parser_and_identity_validation);
    RUN_TEST(test_check_rejects_request_and_response_failures_and_cleans_up);
    RUN_TEST(test_check_rejects_terminal_read_error_after_complete_json);
    RUN_TEST(test_check_rejects_negative_headers_before_reading_or_parsing);
    RUN_TEST(test_check_rejects_wrong_or_huge_declared_length_before_parsing);
    RUN_TEST(test_check_rejects_early_zero_after_valid_json_prefix);
    RUN_TEST(test_check_accepts_complete_chunked_or_unknown_length_response);
    RUN_TEST(test_check_preserves_http_status_precedence_over_body_failures);
    RUN_TEST(test_check_enforces_response_size_limit_at_boundary);
    RUN_TEST(test_attempt_restarts_only_after_real_check_transfer_and_finalization);
    RUN_TEST(test_attempt_resumes_mqtt_for_every_returning_failure);
    RUN_TEST(test_attempt_no_update_and_failed_mqtt_resume_still_finish_without_restart);
    RUN_TEST(test_attempt_without_mqtt_never_stops_or_starts_a_client);
    RUN_TEST(test_confirmation_requires_running_partition_and_pending_state);
    RUN_TEST(test_confirmation_marks_pending_once_and_reports_mark_failure);
    return UNITY_END();
}
