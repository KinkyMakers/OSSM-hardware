#include "test_helpers.h"

void test_scroll_zero(void) {
    ui::TextPage page;
    page.title = "Pattern 1";
    page.body = "First pattern in list.";
    page.scrollPercent = 0;
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/scroll", "zero"));
}

void test_scroll_fifty(void) {
    ui::TextPage page;
    page.title = "Pattern 4";
    page.body = "Midpoint of the list.";
    page.scrollPercent = 50;
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/scroll", "fifty"));
}

void test_scroll_hundred(void) {
    ui::TextPage page;
    page.title = "Pattern 7";
    page.body = "Last pattern in list.";
    page.scrollPercent = 100;
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/scroll", "hundred"));
}

void test_scroll_one(void) {
    ui::TextPage page;
    page.title = "Almost top";
    page.body = "Scroll barely visible.";
    page.scrollPercent = 1;
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/scroll", "one"));
}

void test_scroll_ninetynine(void) {
    ui::TextPage page;
    page.title = "Almost bottom";
    page.body = "Scroll near the end.";
    page.scrollPercent = 99;
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(
        savePBMGrouped(&u8g2, "textpage/scroll", "ninetynine"));
}

void test_scroll_negative_disabled(void) {
    ui::TextPage page;
    page.title = "No Scroll";
    page.body = "Default -1 means no scrollbar.";
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(
        savePBMGrouped(&u8g2, "textpage/scroll", "negative_disabled"));
}

void test_scroll_over_hundred(void) {
    ui::TextPage page;
    page.title = "Overflow";
    page.body = "scrollPercent > 100 should clamp or not crash.";
    page.scrollPercent = 200;
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(bufferHasContent(&u8g2));
    TEST_ASSERT_TRUE(
        savePBMGrouped(&u8g2, "textpage/scroll", "over_hundred"));
}

void test_scroll_empty_page(void) {
    ui::TextPage page;
    page.scrollPercent = 50;
    ui::drawTextPage(&u8g2, page);
    u8g2_SetMaxClipWindow(&u8g2);
    TEST_ASSERT_TRUE(savePBMGrouped(&u8g2, "textpage/scroll", "empty_page"));
}

void register_scroll_tests() {
    RUN_TEST(test_scroll_zero);
    RUN_TEST(test_scroll_fifty);
    RUN_TEST(test_scroll_hundred);
    RUN_TEST(test_scroll_one);
    RUN_TEST(test_scroll_ninetynine);
    RUN_TEST(test_scroll_negative_disabled);
    RUN_TEST(test_scroll_over_hundred);
    RUN_TEST(test_scroll_empty_page);
}
