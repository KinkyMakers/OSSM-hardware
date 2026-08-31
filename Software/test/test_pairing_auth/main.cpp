#include <ArduinoFake.h>
#include <unity.h>

// Exercise the same Arduino String conversion that the firmware uses, including
// its established handling of missing, null and non-string pairing codes.
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "pairing_auth.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

struct FakeHttp {
    using Client = FakeHttp *;
    enum Method { GET, POST };
    static constexpr int OK = 0;
    static int attachCertificateBundle(void *) { return OK; }

    // These are the ESP-IDF configuration members used by the production
    // request. The injected operations capture the actual values it selects.
    struct Config {
        const char *url;
        Method method;
        int timeout_ms;
        int buffer_size_tx;
        int (*crt_bundle_attach)(void *);
        bool skip_cert_common_name_check;
        bool disable_auto_redirect;
        const char *cert_pem;
    };

    Config configuration{};
    std::string configuredUrl;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::string> calls;
    std::string response = R"({"isPaired":true,"pairingCode":"ABC123"})";
    std::string requestBody;
    std::size_t openLength = 0;
    std::size_t readOffset = 0;
    std::size_t chunkSize = 7;
    std::size_t readFailureAt = std::numeric_limits<std::size_t>::max();
    std::int64_t declaredLength = -2;  // -2: infer length; 0: chunked/unknown; -1: failure.
    int responseStatus = 200;
    int failHeaderNumber = 0;
    int openResult = OK;
    int writeResult = -2;  // -2: write the complete request.
    int closeCount = 0;
    int cleanupCount = 0;
    bool failInit = false;
    bool complete = true;

    Client init(const Config *config) {
        calls.emplace_back("init");
        configuration = *config;
        configuredUrl = config->url;
        configuration.url = configuredUrl.c_str();
        return failInit ? nullptr : this;
    }

    int setHeader(Client client, const char *name, const char *value) {
        TEST_ASSERT_EQUAL_PTR(this, client);
        calls.emplace_back("header");
        headers.emplace_back(name, value);
        return static_cast<int>(headers.size()) == failHeaderNumber ? -1 : OK;
    }

    int open(Client client, std::size_t size) {
        TEST_ASSERT_EQUAL_PTR(this, client);
        calls.emplace_back("open");
        openLength = size;
        return openResult;
    }

    int write(Client client, const char *body, std::size_t size) {
        TEST_ASSERT_EQUAL_PTR(this, client);
        calls.emplace_back("write");
        requestBody.assign(body, size);
        return writeResult == -2 ? static_cast<int>(size) : writeResult;
    }

    std::int64_t fetchHeaders(Client client) {
        TEST_ASSERT_EQUAL_PTR(this, client);
        calls.emplace_back("fetch_headers");
        return declaredLength == -2 ? static_cast<std::int64_t>(response.size()) : declaredLength;
    }

    int statusCode(Client client) {
        TEST_ASSERT_EQUAL_PTR(this, client);
        calls.emplace_back("status");
        return responseStatus;
    }

    int read(Client client, char *buffer, std::size_t capacity) {
        TEST_ASSERT_EQUAL_PTR(this, client);
        calls.emplace_back("read");
        if (readOffset >= readFailureAt) return -1;
        if (readOffset == response.size()) return 0;
        const std::size_t count = std::min(
            {capacity, chunkSize, response.size() - readOffset, readFailureAt - readOffset});
        std::memcpy(buffer, response.data() + readOffset, count);
        readOffset += count;
        return static_cast<int>(count);
    }

    bool isComplete(Client client) {
        TEST_ASSERT_EQUAL_PTR(this, client);
        calls.emplace_back("complete");
        return complete;
    }

    void close(Client client) {
        TEST_ASSERT_EQUAL_PTR(this, client);
        calls.emplace_back("close");
        ++closeCount;
    }

    void cleanup(Client client) {
        TEST_ASSERT_EQUAL_PTR(this, client);
        calls.emplace_back("cleanup");
        ++cleanupCount;
    }

    bool called(const char *name) const {
        return std::find(calls.begin(), calls.end(), name) != calls.end();
    }
};

pairing_auth::DeviceIdentity deviceIdentity() {
    return {"AA:BB:CC:DD:EE:FF", "1234abcd", "0123456789abcdef0123456789abcdef",
            "1.2.3", "signed-test-token", "test-token-id", std::string(64, 'a')};
}

struct State {
    volatile bool paired = true;
    String pairingCode = "previous-code";
    const char *error = "previous-error";
};

int request(FakeHttp &http, State &state, bool updatePairingCode = true,
            const pairing_auth::DeviceIdentity &identity = deviceIdentity()) {
    return pairing_auth::requestDeviceAuth(http, "https://dashboard.example.test",
                                          identity, updatePairingCode, state.paired,
                                          state.pairingCode, state.error);
}

void assertCleaned(const FakeHttp &http) {
    TEST_ASSERT_EQUAL_INT(1, http.closeCount);
    TEST_ASSERT_EQUAL_INT(1, http.cleanupCount);
    TEST_ASSERT_TRUE(http.calls.size() >= 2);
    TEST_ASSERT_EQUAL_STRING("close", http.calls[http.calls.size() - 2].c_str());
    TEST_ASSERT_EQUAL_STRING("cleanup", http.calls.back().c_str());
}

void assertUnchanged(const State &state) {
    TEST_ASSERT_TRUE(state.paired);
    TEST_ASSERT_EQUAL_STRING("previous-code", state.pairingCode.c_str());
}

void test_post_uses_verified_idf_bundle_and_five_second_timeout() {
    FakeHttp http;
    State state;
    TEST_ASSERT_EQUAL_INT(200, request(http, state));
    TEST_ASSERT_EQUAL_STRING("https://dashboard.example.test/api/ossm/auth",
                             http.configuredUrl.c_str());
    TEST_ASSERT_EQUAL_INT(FakeHttp::POST, http.configuration.method);
    TEST_ASSERT_EQUAL_INT(5000, http.configuration.timeout_ms);
    TEST_ASSERT_EQUAL_INT(2048, http.configuration.buffer_size_tx);
    TEST_ASSERT_TRUE(http.configuration.crt_bundle_attach == FakeHttp::attachCertificateBundle);
    TEST_ASSERT_FALSE(http.configuration.skip_cert_common_name_check);
    TEST_ASSERT_TRUE(http.configuration.disable_auto_redirect);
    TEST_ASSERT_NULL(http.configuration.cert_pem);
    TEST_ASSERT_NULL(state.error);
    assertCleaned(http);
}

void test_request_preserves_exact_json_body_and_provenance_headers() {
    FakeHttp http;
    State state;
    TEST_ASSERT_EQUAL_INT(200, request(http, state));
    TEST_ASSERT_EQUAL_STRING(
        "{\"mac\":\"AA:BB:CC:DD:EE:FF\",\"chip\":\"1234abcd\","
        "\"md5\":\"0123456789abcdef0123456789abcdef\",\"device\":\"OSSM\","
        "\"version\":\"1.2.3\"}", http.requestBody.c_str());
    TEST_ASSERT_EQUAL_UINT(http.requestBody.size(), http.openLength);
    const std::vector<std::pair<std::string, std::string>> expected = {
        {"Content-Type", "application/json"},
        {"Accept-Encoding", "identity"},
        {"X-RAD-Firmware-Provenance-Capability", "1"},
        {"X-RAD-Firmware-Provenance", "signed-test-token"},
        {"X-RAD-Firmware-Provenance-ID", "test-token-id"},
        {"X-RAD-Firmware-Image-SHA256", std::string(64, 'a')},
    };
    TEST_ASSERT_TRUE(http.headers == expected);
    assertCleaned(http);
}

void test_unprovisioned_device_still_sends_capability_without_token_headers() {
    FakeHttp http;
    State state;
    auto identity = deviceIdentity();
    identity.provenance.clear();
    TEST_ASSERT_EQUAL_INT(200, request(http, state, true, identity));
    const std::vector<std::pair<std::string, std::string>> expected = {
        {"Content-Type", "application/json"},
        {"Accept-Encoding", "identity"},
        {"X-RAD-Firmware-Provenance-Capability", "1"},
    };
    TEST_ASSERT_TRUE(http.headers == expected);
    assertCleaned(http);
}

void test_token_presence_preserves_empty_id_and_image_headers() {
    FakeHttp http;
    State state;
    auto identity = deviceIdentity();
    identity.provenanceId.clear();
    identity.imageSha256.clear();
    TEST_ASSERT_EQUAL_INT(200, request(http, state, true, identity));
    TEST_ASSERT_EQUAL_UINT(6, http.headers.size());
    TEST_ASSERT_EQUAL_STRING("X-RAD-Firmware-Provenance-ID", http.headers[4].first.c_str());
    TEST_ASSERT_TRUE(http.headers[4].second.empty());
    TEST_ASSERT_EQUAL_STRING("X-RAD-Firmware-Image-SHA256", http.headers[5].first.c_str());
    TEST_ASSERT_TRUE(http.headers[5].second.empty());
    assertCleaned(http);
}

void test_identity_strings_are_json_escaped_by_production_serializer() {
    FakeHttp http;
    State state;
    auto identity = deviceIdentity();
    identity.version = "1.2.3\"\\\n";
    TEST_ASSERT_EQUAL_INT(200, request(http, state, true, identity));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, http.requestBody.find("1.2.3\\\"\\\\\\n"));
    JsonDocument decoded;
    TEST_ASSERT_FALSE(deserializeJson(decoded, http.requestBody));
    TEST_ASSERT_EQUAL_STRING(identity.version.c_str(), decoded["version"].as<const char *>());
    assertCleaned(http);
}

void test_foreground_response_updates_paired_and_code_from_split_reads() {
    FakeHttp http;
    State state;
    state.paired = false;
    http.chunkSize = 1;
    TEST_ASSERT_EQUAL_INT(200, request(http, state));
    TEST_ASSERT_TRUE(state.paired);
    TEST_ASSERT_EQUAL_STRING("ABC123", state.pairingCode.c_str());
    TEST_ASSERT_NULL(state.error);
    TEST_ASSERT_EQUAL_UINT(http.response.size(), http.readOffset);
    TEST_ASSERT_TRUE(http.called("complete"));
    assertCleaned(http);
}

void test_unpaired_response_replaces_previous_state_and_code() {
    FakeHttp http;
    State state;
    http.response = R"({"isPaired":false,"pairingCode":"NEW456"})";
    TEST_ASSERT_EQUAL_INT(200, request(http, state));
    TEST_ASSERT_FALSE(state.paired);
    TEST_ASSERT_EQUAL_STRING("NEW456", state.pairingCode.c_str());
    assertCleaned(http);
}

void test_background_poll_changes_only_paired() {
    for (bool responsePaired : {false, true}) {
        FakeHttp http;
        State state;
        state.paired = !responsePaired;
        http.response = responsePaired
                            ? R"({"isPaired":true,"pairingCode":"NEW456"})"
                            : R"({"isPaired":false,"pairingCode":"NEW456"})";
        TEST_ASSERT_EQUAL_INT(200, request(http, state, false));
        TEST_ASSERT_EQUAL(responsePaired, state.paired);
        TEST_ASSERT_EQUAL_STRING("previous-code", state.pairingCode.c_str());
        assertCleaned(http);
    }
}

void test_missing_and_null_fields_keep_legacy_arduinojson_conversions() {
    for (const char *response : {"{}", "null", "[]",
                                R"({"isPaired":null,"pairingCode":null})"}) {
        FakeHttp http;
        State state;
        http.response = response;
        TEST_ASSERT_EQUAL_INT(200, request(http, state));
        TEST_ASSERT_FALSE(state.paired);
        TEST_ASSERT_EQUAL_STRING("null", state.pairingCode.c_str());
        assertCleaned(http);
    }
}

void test_non_string_pairing_codes_keep_legacy_conversion() {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {R"({"isPaired":false,"pairingCode":42})", "42"},
        {R"({"isPaired":false,"pairingCode":false})", "false"},
        {R"({"isPaired":false,"pairingCode":"A\u0000B"})", "A"},
    };
    for (const auto &item : cases) {
        FakeHttp http;
        State state;
        http.response = item.first;
        TEST_ASSERT_EQUAL_INT(200, request(http, state));
        TEST_ASSERT_FALSE(state.paired);
        TEST_ASSERT_EQUAL_STRING(item.second.c_str(), state.pairingCode.c_str());
        assertCleaned(http);
    }
}

void test_non_200_statuses_preserve_state_without_reading_or_following_redirects() {
    for (int status : {201, 204, 301, 302, 307, 400, 401, 404, 429, 500, 503}) {
        FakeHttp http;
        State state;
        http.responseStatus = status;
        TEST_ASSERT_EQUAL_INT(status, request(http, state));
        assertUnchanged(state);
        TEST_ASSERT_NULL(state.error);
        TEST_ASSERT_FALSE(http.called("read"));
        TEST_ASSERT_TRUE(http.configuration.disable_auto_redirect);
        assertCleaned(http);
    }
}

void test_initialization_failure_has_no_client_to_clean_up() {
    FakeHttp http;
    State state;
    http.failInit = true;
    TEST_ASSERT_LESS_THAN_INT(0, request(http, state));
    assertUnchanged(state);
    TEST_ASSERT_NOT_NULL(state.error);
    TEST_ASSERT_EQUAL_INT(0, http.closeCount);
    TEST_ASSERT_EQUAL_INT(0, http.cleanupCount);
    TEST_ASSERT_EQUAL_UINT(1, http.calls.size());
}

void test_each_header_failure_cleans_up_without_opening_connection() {
    for (int failHeader = 1; failHeader <= 6; ++failHeader) {
        FakeHttp http;
        State state;
        http.failHeaderNumber = failHeader;
        TEST_ASSERT_LESS_THAN_INT(0, request(http, state));
        assertUnchanged(state);
        TEST_ASSERT_NOT_NULL(state.error);
        TEST_ASSERT_FALSE(http.called("open"));
        assertCleaned(http);
    }
}

void test_connection_or_tls_verification_failure_stops_before_request_write() {
    FakeHttp http;
    State state;
    http.openResult = -1;
    TEST_ASSERT_LESS_THAN_INT(0, request(http, state));
    assertUnchanged(state);
    TEST_ASSERT_NOT_NULL(state.error);
    TEST_ASSERT_FALSE(http.called("write"));
    TEST_ASSERT_FALSE(http.configuration.skip_cert_common_name_check);
    assertCleaned(http);
}

void test_partial_or_failed_request_write_cannot_apply_a_response() {
    for (int writeResult : {-1, 0, 1}) {
        FakeHttp http;
        State state;
        http.writeResult = writeResult;
        TEST_ASSERT_LESS_THAN_INT(0, request(http, state));
        assertUnchanged(state);
        TEST_ASSERT_NOT_NULL(state.error);
        TEST_ASSERT_FALSE(http.called("fetch_headers"));
        assertCleaned(http);
    }
}

void test_header_fetch_failure_cleans_up_without_reading_response() {
    FakeHttp http;
    State state;
    http.declaredLength = -1;
    TEST_ASSERT_LESS_THAN_INT(0, request(http, state));
    assertUnchanged(state);
    TEST_ASSERT_NOT_NULL(state.error);
    TEST_ASSERT_FALSE(http.called("read"));
    assertCleaned(http);
}

void test_read_failures_preserve_state_even_after_a_complete_json_prefix() {
    for (std::size_t failureAt : {std::size_t(0), std::size_t(5),
                                  FakeHttp{}.response.size()}) {
        FakeHttp http;
        State state;
        http.readFailureAt = failureAt;
        TEST_ASSERT_LESS_THAN_INT(0, request(http, state));
        assertUnchanged(state);
        TEST_ASSERT_NOT_NULL(state.error);
        assertCleaned(http);
    }
}

void test_complete_chunked_response_can_update_state_without_content_length() {
    FakeHttp http;
    State state;
    http.declaredLength = 0;
    TEST_ASSERT_EQUAL_INT(200, request(http, state));
    TEST_ASSERT_TRUE(state.paired);
    TEST_ASSERT_EQUAL_STRING("ABC123", state.pairingCode.c_str());
    TEST_ASSERT_TRUE(http.called("complete"));
    assertCleaned(http);
}

void test_truncated_or_mismatched_content_length_preserves_state() {
    for (int difference : {-1, 1, 100}) {
        FakeHttp http;
        State state;
        http.declaredLength = static_cast<int>(http.response.size()) + difference;
        TEST_ASSERT_EQUAL_INT(500, request(http, state));
        assertUnchanged(state);
        TEST_ASSERT_NOT_NULL(state.error);
        assertCleaned(http);
    }
}

void test_transport_incomplete_flag_rejects_valid_json_with_any_length_mode() {
    for (int declaredLength : {-2, 0}) {
        FakeHttp http;
        State state;
        http.declaredLength = declaredLength;
        http.complete = false;
        TEST_ASSERT_EQUAL_INT(500, request(http, state));
        assertUnchanged(state);
        TEST_ASSERT_NOT_NULL(state.error);
        assertCleaned(http);
    }
}

void test_invalid_empty_and_truncated_json_preserve_previous_state() {
    for (const char *response : {"", "not json", "{", R"({"isPaired":false,)",
                                R"({"isPaired":false,"pairingCode":"cut)"}) {
        for (bool updateCode : {false, true}) {
            FakeHttp http;
            State state;
            http.response = response;
            TEST_ASSERT_EQUAL_INT(500, request(http, state, updateCode));
            assertUnchanged(state);
            TEST_ASSERT_NOT_NULL(state.error);
            assertCleaned(http);
        }
    }
}

void test_oversized_declared_response_is_rejected_before_allocating_body() {
    for (std::int64_t length : {std::int64_t(pairing_auth::MAX_RESPONSE_BYTES + 1),
                               std::int64_t(0x100000000LL),
                               std::numeric_limits<std::int64_t>::max()}) {
        FakeHttp http;
        State state;
        http.declaredLength = length;
        TEST_ASSERT_EQUAL_INT(500, request(http, state));
        assertUnchanged(state);
        TEST_ASSERT_NOT_NULL(state.error);
        TEST_ASSERT_FALSE(http.called("read"));
        assertCleaned(http);
    }
}

void test_streamed_response_limit_accepts_boundary_and_rejects_one_extra_byte() {
    for (std::size_t extra : {std::size_t(0), std::size_t(1)}) {
        FakeHttp http;
        State state;
        http.declaredLength = 0;
        http.chunkSize = 512;
        http.response.resize(pairing_auth::MAX_RESPONSE_BYTES + extra, ' ');
        TEST_ASSERT_EQUAL_INT(extra ? 500 : 200, request(http, state));
        if (extra) {
            assertUnchanged(state);
            TEST_ASSERT_NOT_NULL(state.error);
        } else {
            TEST_ASSERT_TRUE(state.paired);
            TEST_ASSERT_EQUAL_STRING("ABC123", state.pairingCode.c_str());
            TEST_ASSERT_NULL(state.error);
        }
        assertCleaned(http);
    }
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_post_uses_verified_idf_bundle_and_five_second_timeout);
    RUN_TEST(test_request_preserves_exact_json_body_and_provenance_headers);
    RUN_TEST(test_unprovisioned_device_still_sends_capability_without_token_headers);
    RUN_TEST(test_token_presence_preserves_empty_id_and_image_headers);
    RUN_TEST(test_identity_strings_are_json_escaped_by_production_serializer);
    RUN_TEST(test_foreground_response_updates_paired_and_code_from_split_reads);
    RUN_TEST(test_unpaired_response_replaces_previous_state_and_code);
    RUN_TEST(test_background_poll_changes_only_paired);
    RUN_TEST(test_missing_and_null_fields_keep_legacy_arduinojson_conversions);
    RUN_TEST(test_non_string_pairing_codes_keep_legacy_conversion);
    RUN_TEST(test_non_200_statuses_preserve_state_without_reading_or_following_redirects);
    RUN_TEST(test_initialization_failure_has_no_client_to_clean_up);
    RUN_TEST(test_each_header_failure_cleans_up_without_opening_connection);
    RUN_TEST(test_connection_or_tls_verification_failure_stops_before_request_write);
    RUN_TEST(test_partial_or_failed_request_write_cannot_apply_a_response);
    RUN_TEST(test_header_fetch_failure_cleans_up_without_reading_response);
    RUN_TEST(test_read_failures_preserve_state_even_after_a_complete_json_prefix);
    RUN_TEST(test_complete_chunked_response_can_update_state_without_content_length);
    RUN_TEST(test_truncated_or_mismatched_content_length_preserves_state);
    RUN_TEST(test_transport_incomplete_flag_rejects_valid_json_with_any_length_mode);
    RUN_TEST(test_invalid_empty_and_truncated_json_preserve_previous_state);
    RUN_TEST(test_oversized_declared_response_is_rejected_before_allocating_body);
    RUN_TEST(test_streamed_response_limit_accepts_boundary_and_rejects_one_extra_byte);
    return UNITY_END();
}
