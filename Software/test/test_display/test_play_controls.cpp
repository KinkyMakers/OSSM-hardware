#include "test_helpers.h"

void test_drawPreflight(void) {
    ui::PreflightData data{"Simple Penetration", 75.0f, "Speed",
                           "Decrease the speed to begin playing."};
    ui::drawPreflight(&u8g2, data);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "play_controls", "preflight"));
}

void test_drawPlayControls_simple(void) {
    ui::PlayControlsData data{};
    data.speed = 65.0f;
    data.stroke = 80.0f;
    data.sensation = 50.0f;
    data.depth = 70.0f;
    data.buffer = 0;
    data.activeControl = ui::PlayControl::STROKE;
    data.strokeCount = 142;
    data.distanceMeters = 3.5f;
    data.elapsedMs = 125000;
    data.pattern = 0;
    data.isStrokeEngine = false;
    data.isStreaming = false;
    data.headerText = "Simple Penetration";
    data.speedLabel = "Speed";
    data.strokeLabel = "Stroke";
    data.distanceStr = "3.5 m";
    data.timeStr = "02:05";

    ui::drawPlayControls(&u8g2, data);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "play_controls", "play_simple"));
}

void test_drawPlayControls_strokeEngine(void) {
    ui::PlayControlsData data{};
    data.speed = 45.0f;
    data.stroke = 60.0f;
    data.sensation = 75.0f;
    data.depth = 90.0f;
    data.buffer = 0;
    data.activeControl = ui::PlayControl::SENSATION;
    data.strokeCount = 0;
    data.distanceMeters = 0;
    data.elapsedMs = 30000;
    data.pattern = 1;
    data.isStrokeEngine = true;
    data.isStreaming = false;
    data.headerText = "Teasing Pounding";
    data.speedLabel = "Speed";
    data.strokeLabel = "Stroke";
    data.distanceStr = nullptr;
    data.timeStr = "00:30";

    ui::drawPlayControls(&u8g2, data);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "play_controls", "play_stroke_engine"));
}

void test_drawPlayControls_streaming(void) {
    ui::PlayControlsData data{};
    data.speed = 50.0f;
    data.stroke = 70.0f;
    data.sensation = 40.0f;
    data.depth = 60.0f;
    data.buffer = 80.0f;
    data.activeControl = ui::PlayControl::BUFFER;
    data.strokeCount = 0;
    data.distanceMeters = 0;
    data.elapsedMs = 15000;
    data.pattern = 0;
    data.isStrokeEngine = false;
    data.isStreaming = true;
    data.headerText = "Streaming";
    data.speedLabel = "Speed";
    data.strokeLabel = "Stroke";
    data.distanceStr = nullptr;
    data.timeStr = nullptr;

    ui::drawPlayControls(&u8g2, data);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "play_controls", "play_streaming"));
}

void test_drawPatternControls(void) {
    ui::TextPage page;
    page.title = "Teasing Pounding";
    page.body = "Speed shifts with sensation; balances faster strokes.";
    page.scrollPercent = 28;
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "play_controls", "pattern_controls"));
}

void register_play_controls_tests() {
    RUN_TEST(test_drawPreflight);
    RUN_TEST(test_drawPlayControls_simple);
    RUN_TEST(test_drawPlayControls_strokeEngine);
    RUN_TEST(test_drawPlayControls_streaming);
    RUN_TEST(test_drawPatternControls);
}
