#include "test_helpers.h"

// ============================================================
// declared pages — draw every ui::pages:: declaration exactly
// once, patching only the truly runtime fields where needed.
// ============================================================

void test_page_help(void) {
    ui::drawTextPage(&u8g2, ui::pages::helpPage);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/pages", "help"));
}

void test_page_updateChecking(void) {
    ui::drawTextPage(&u8g2, ui::pages::updateCheckingPage);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(
        savePBMGrouped(&u8g2, "textpage/pages", "update_checking"));
}

void test_page_noUpdate(void) {
    ui::drawTextPage(&u8g2, ui::pages::noUpdatePage);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/pages", "no_update"));
}

void test_page_updating(void) {
    ui::drawTextPage(&u8g2, ui::pages::updatingPage);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/pages", "updating"));
}

void test_page_error(void) {
    ui::TextPage page = ui::pages::errorPage;
    page.body = "Homing took too long. Please check your wiring and try again.";
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/pages", "error"));
}

void test_page_wifiDisconnected(void) {
    ui::drawTextPage(&u8g2, ui::pages::wifiDisconnectedPage);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(
        savePBMGrouped(&u8g2, "textpage/pages", "wifi_disconnected"));
}

void test_page_wifiConnected(void) {
    ui::drawTextPage(&u8g2, ui::pages::wifiConnectedPage);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/pages", "wifi_connected"));
}

void test_page_pairing(void) {
    ui::TextPage page = ui::pages::pairingPage;
    page.subtitle = "AABBCCDDEEFF";
    page.qrUrl = "HTTPS://DASHBOARD.RESEARCHANDDESIRE.COM/OSSM/AABBCCDDEEFF";
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/pages", "pairing"));
}

// ============================================================
// patterns — render each stroke engine pattern as seen on the
// pattern selection screen, with name, description, and scroll.
// ============================================================

static const size_t numberOfPatterns =
    sizeof(ui::strings::strokeEngineNames) /
    sizeof(ui::strings::strokeEngineNames[0]);

static void toSnakeCase(char* dst, size_t dstLen, const char* src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dstLen - 1; i++) {
        char c = src[i];
        if (c == ' ' || c == '\'') {
            dst[j++] = '_';
        } else {
            dst[j++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
    }
    dst[j] = '\0';
}

static void test_page_pattern(void) {
    static size_t idx = 0;
    size_t i = idx++;

    ui::TextPage page;
    page.title = ui::strings::strokeEngineNames[i];
    page.body = ui::strings::strokeEngineDescriptions[i];
    page.scrollPercent = ui::scrollPercent(i, numberOfPatterns);
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));

    char filename[128];
    char snake[96];
    toSnakeCase(snake, sizeof(snake), ui::strings::strokeEngineNames[i]);
    snprintf(filename, sizeof(filename), "%zu_%s", i, snake);
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/patterns", filename));
}

// ============================================================
// Registration
// ============================================================

void register_pages_tests() {
    // declared pages
    RUN_TEST(test_page_help);
    RUN_TEST(test_page_updateChecking);
    RUN_TEST(test_page_noUpdate);
    RUN_TEST(test_page_updating);
    RUN_TEST(test_page_error);
    RUN_TEST(test_page_wifiDisconnected);
    RUN_TEST(test_page_wifiConnected);
    RUN_TEST(test_page_pairing);

    // stroke engine patterns
    for (size_t i = 0; i < numberOfPatterns; i++) {
        RUN_TEST(test_page_pattern);
    }
}
